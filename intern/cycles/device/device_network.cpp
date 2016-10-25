/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "device.h"
#include "device_intern.h"
#include "device_network.h"

#include "util_foreach.h"
#include "util_logging.h"
#include "util_queue.h"

#if defined(WITH_NETWORK)

CCL_NAMESPACE_BEGIN

typedef map<device_ptr, device_ptr> PtrMap;
typedef vector<uint8_t> DataVector;
typedef map<device_ptr, DataVector> DataMap;

/* tile list */
typedef vector<RenderTile> TileList;

/* search a list of tiles and find the one that matches the passed render tile */
static TileList::iterator tile_list_find(TileList& tile_list, RenderTile& tile)
{
	for(TileList::iterator it = tile_list.begin(); it != tile_list.end(); ++it)
		if(tile.x == it->x && tile.y == it->y && tile.start_sample == it->start_sample)
			return it;
	return tile_list.end();
}

class NetworkDevice : public Device
{
public:
	boost::asio::io_service io_service;
	tcp::socket socket;
	device_ptr mem_counter;
	DeviceTask the_task; /* todo: handle multiple tasks */

	thread_mutex rpc_lock;

	thread *receive_thread;

	void *mem_copy_from_data;
	size_t mem_copy_from_size;
	thread_mutex mem_copy_from_mutex;
	bool mem_copy_from_finished;
	thread_condition_variable mem_copy_from_cond;

	bool load_kernels_result;
	bool load_kernels_finished;
	thread_mutex load_kernels_mutex;
	thread_condition_variable load_kernels_cond;

	bool task_wait_finished;
	thread_mutex task_wait_mutex;
	thread_condition_variable task_wait_cond;

	const string& error_message()
	{
		return error_func.error;
	}

	bool have_error()
	{
		return error_func.have_error();
	}

	NetworkDevice(DeviceInfo& info, Stats &stats, const char *address)
	: Device(info, stats, true), socket(io_service)
	{
		error_func = NetworkError();

		boost::system::error_code error = boost::asio::error::host_not_found;
//		if(getenv("CYCLES_IP")) {
//			socket.connect(tcp::endpoint(boost::asio::ip::address::from_string(getenv("CYCLES_IP")), SERVER_PORT), error);
//		}
//		else {
			stringstream portstr;
			portstr << SERVER_PORT;

			tcp::resolver resolver(io_service);
			tcp::resolver::query query(address, portstr.str());
			tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
			tcp::resolver::iterator end;

			while(error && endpoint_iterator != end)
			{
				socket.close();
				socket.connect(*endpoint_iterator++, error);
			}
//		}

		if(error)
			error_func.network_error(error.message());

		receive_thread = new thread(function_bind(&NetworkDevice::receive, this));

		mem_counter = 0;
	}

	~NetworkDevice()
	{
		RPCSend snd(socket, &error_func, "stop");
		snd.write();

		/* Join with the receive thread. */
		delete receive_thread;
	}

	bool buffer_copy_needed()
	{
		return false;
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		if(have_error()) return;

		thread_scoped_lock lock(rpc_lock);

		mem.device_pointer = ++mem_counter;

		RPCSend snd(socket, &error_func, "mem_alloc");

		snd.add(mem);
		snd.add(type);
		snd.write();
	}

	void mem_copy_to(device_memory& mem)
	{
		if(have_error()) return;

		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "mem_copy_to");

		snd.add(mem);
		snd.write();
		snd.write_buffer((void*)mem.data_pointer, mem.memory_size());
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		if(have_error()) return;

		thread_scoped_lock lock(rpc_lock);
		thread_scoped_lock mem_copy_from_lock(mem_copy_from_mutex);

		size_t data_size = mem.memory_size();

		RPCSend snd(socket, &error_func, "mem_copy_from");

		snd.add(mem);
		snd.add(y);
		snd.add(w);
		snd.add(h);
		snd.add(elem);
		snd.write();

		lock.unlock();

		/* Set up what that the receiving thread needs to store the data. */
		mem_copy_from_finished = false;
		mem_copy_from_data = (void*) mem.data_pointer;
		mem_copy_from_size = data_size;

		/* Give up lock on the mutex to allow receiving and wait for it to finish. */
		while(!mem_copy_from_finished) mem_copy_from_cond.wait(mem_copy_from_lock);
	}

	void mem_zero(device_memory& mem)
	{
		if(have_error()) return;

		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "mem_zero");

		snd.add(mem);
		snd.write();
	}

	void mem_free(device_memory& mem)
	{
		if(have_error()) return;

		if(mem.device_pointer) {
			thread_scoped_lock lock(rpc_lock);

			RPCSend snd(socket, &error_func, "mem_free");

			snd.add(mem);
			snd.write();

			mem.device_pointer = 0;
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		if(have_error()) return;

		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "const_copy_to");

		string name_string(name);

		snd.add(name_string);
		snd.add(size);
		snd.write();
		snd.write_buffer(host, size);
	}

	void tex_alloc(const char *name,
	               device_memory& mem,
	               InterpolationType interpolation,
	               ExtensionType extension)
	{
		if(have_error()) return;

		VLOG(1) << "Texture allocate: " << name << ", "
		        << string_human_readable_number(mem.memory_size()) << " bytes. ("
		        << string_human_readable_size(mem.memory_size()) << ")";

		thread_scoped_lock lock(rpc_lock);

		mem.device_pointer = ++mem_counter;

		RPCSend snd(socket, &error_func, "tex_alloc");

		string name_string(name);

		snd.add(name_string);
		snd.add(mem);
		snd.add(interpolation);
		snd.add(extension);
		snd.write();
		snd.write_buffer((void*)mem.data_pointer, mem.memory_size());
	}

	void tex_free(device_memory& mem)
	{
		if(have_error()) return;

		if(mem.device_pointer) {
			thread_scoped_lock lock(rpc_lock);

			RPCSend snd(socket, &error_func, "tex_free");

			snd.add(mem);
			snd.write();

			mem.device_pointer = 0;
		}
	}

	bool load_kernels(const DeviceRequestedFeatures& requested_features)
	{
		if(have_error()) return false;

		thread_scoped_lock lock(rpc_lock);
		thread_scoped_lock load_kernels_lock(load_kernels_mutex);

		RPCSend snd(socket, &error_func, "load_kernels");
		snd.add(requested_features.experimental);
		snd.add(requested_features.max_closure);
		snd.add(requested_features.max_nodes_group);
		snd.add(requested_features.nodes_features);
		snd.write();

		lock.unlock();

		load_kernels_finished = false;
		while(!load_kernels_finished) load_kernels_cond.wait(load_kernels_lock);

		return load_kernels_result;
	}

	void task_add(DeviceTask& task)
	{
		if(have_error()) return;

		thread_scoped_lock lock(rpc_lock);

		the_task = task;

		RPCSend snd(socket, &error_func, "task_add");
		snd.add(task);
		snd.write();
	}

	void receive()
	{
		/* Needed to cache the buffer pointers */
		TileList the_tiles;

		for(;;) {
			RPCReceive rcv(socket, &error_func);

			if(have_error()) break;

			if(rcv.name == "acquire_tile") {
				RenderTile tile;
				if(the_task.acquire_tile(this, tile)) { /* write return as bool */
					the_tiles.push_back(tile);

					thread_scoped_lock lock(rpc_lock);
					RPCSend snd(socket, &error_func, "acquire_tile_done");
					snd.add(tile);
					device_memory &mem = tile.buffers->buffer;
					snd.add(mem);
					snd.add(0);
					snd.add(tile.buffers->params.width);
					snd.add(tile.buffers->params.height);
					snd.add(tile.buffers->params.get_passes_size()*sizeof(float));
					snd.write();
				}
				else {
					thread_scoped_lock lock(rpc_lock);
					RPCSend snd(socket, &error_func, "acquire_tile_none");
					snd.write();
				}
			}
			else if(rcv.name == "release_tile") {
				RenderTile tile;
				rcv.read(tile);

				TileList::iterator it = tile_list_find(the_tiles, tile);
				if(it != the_tiles.end()) {
					tile.buffers = it->buffers;
					the_tiles.erase(it);
				}

				rcv.read_buffer((void*) tile.buffers->buffer.data_pointer, tile.buffers->buffer.memory_size());

				assert(tile.buffers != NULL);

				the_task.release_tile(tile);
			}
			else if(rcv.name == "mem_copy_from_done") {
				thread_scoped_lock mem_copy_from_lock(mem_copy_from_mutex);
				rcv.read_buffer(mem_copy_from_data, mem_copy_from_size);
				mem_copy_from_finished = true;
				mem_copy_from_lock.unlock();
				mem_copy_from_cond.notify_one();
			}
			else if(rcv.name == "load_kernels_done") {
				thread_scoped_lock load_kernels_lock(load_kernels_mutex);
				rcv.read(load_kernels_result);
				load_kernels_finished = true;
				load_kernels_lock.unlock();
				load_kernels_cond.notify_one();
			}
			else if(rcv.name == "task_wait_done") {
				thread_scoped_lock task_wait_lock(task_wait_mutex);
				task_wait_finished = true;
				task_wait_lock.unlock();
				task_wait_cond.notify_one();
			}
			else if(rcv.name == "stop_ok") {
				break;
			}
		}

		/* Wake up the task_wait thread. */
		task_wait_cond.notify_all();
	}

	void task_wait()
	{
		if(have_error()) return;

		thread_scoped_lock lock(rpc_lock);
		RPCSend snd(socket, &error_func, "task_wait");
		snd.write();
		lock.unlock();

		thread_scoped_lock task_wait_lock(task_wait_mutex);
		task_wait_finished = false;
		while(!task_wait_finished && !have_error()) task_wait_cond.wait(task_wait_lock);
	}

	void task_cancel()
	{
		if(have_error()) return;

		thread_scoped_lock lock(rpc_lock);
		RPCSend snd(socket, &error_func, "task_cancel");
		snd.write();
	}

	int get_split_task_count(DeviceTask& task)
	{
		return 1;
	}

private:
	NetworkError error_func;
};

Device *device_network_create(DeviceInfo& info, Stats &stats, const char *address)
{
	return new NetworkDevice(info, stats, address);
}

void device_network_info(vector<DeviceInfo>& devices)
{
	DeviceInfo info;

	info.type = DEVICE_NETWORK;
	info.description = "Network Device";
	info.id = "NETWORK";
	info.num = 0;
	info.advanced_shading = true; /* todo: get this info from device */
	info.pack_images = false;

	devices.push_back(info);
}

class DeviceServer {
public:
	thread_mutex receive_lock;
	thread_mutex send_lock;

	void network_error(const string &message) {
		error_func.network_error(message);
	}

	bool have_error() { return error_func.have_error(); }

	DeviceServer(Device *device_, tcp::socket& socket_)
	: device(device_), socket(socket_), task_wait_thread(NULL)
	{
		error_func = NetworkError();
	}

	void listen()
	{
		/* receive remote function calls */
		while(listen_step());
		delete task_wait_thread;
		task_wait_thread = NULL;
	}

protected:

	queue<RenderTile> acquired_tiles;
	bool out_of_tiles;
	thread_mutex acquire_tile_mutex;
	thread_condition_variable acquire_tile_cond;

	bool listen_step()
	{
		thread_scoped_lock lock(receive_lock);
		RPCReceive rcv(socket, &error_func);

		if(have_error()) {
			cout << "Network error: " << error_func.error << "\n";
			return false;
		}

		if(rcv.name == "stop") {
			thread_scoped_lock lock(send_lock);
			RPCSend snd(socket, &error_func, "stop_ok");
			snd.write();
			return false;
		}

		process(rcv, lock);
		return true;
	}

	/* create a memory buffer for a device buffer and insert it into mem_data */
	DataVector &data_vector_insert(device_ptr client_pointer, size_t data_size)
	{
		/* create a new DataVector and insert it into mem_data */
		pair<DataMap::iterator,bool> data_ins = mem_data.insert(
		        DataMap::value_type(client_pointer, DataVector()));

		/* make sure it was a unique insertion */
		assert(data_ins.second);

		/* get a reference to the inserted vector */
		DataVector &data_v = data_ins.first->second;

		/* size the vector */
		data_v.resize(data_size);

		return data_v;
	}

	DataVector &data_vector_find(device_ptr client_pointer)
	{
		DataMap::iterator i = mem_data.find(client_pointer);
		assert(i != mem_data.end());
		return i->second;
	}

	/* setup mapping and reverse mapping of client_pointer<->real_pointer */
	void pointer_mapping_insert(device_ptr client_pointer, device_ptr real_pointer)
	{
		pair<PtrMap::iterator,bool> mapins;

		/* insert mapping from client pointer to our real device pointer */
		mapins = ptr_map.insert(PtrMap::value_type(client_pointer, real_pointer));
		assert(mapins.second);

		/* insert reverse mapping from real our device pointer to client pointer */
		mapins = ptr_imap.insert(PtrMap::value_type(real_pointer, client_pointer));
		assert(mapins.second);
	}

	device_ptr device_ptr_from_client_pointer(device_ptr client_pointer)
	{
		PtrMap::iterator i = ptr_map.find(client_pointer);
		assert(i != ptr_map.end());
		return i->second;
	}

	device_ptr device_ptr_from_client_pointer_erase(device_ptr client_pointer)
	{
		PtrMap::iterator i = ptr_map.find(client_pointer);
		assert(i != ptr_map.end());

		device_ptr result = i->second;

		/* erase the mapping */
		ptr_map.erase(i);

		/* erase the reverse mapping */
		PtrMap::iterator irev = ptr_imap.find(result);
		assert(irev != ptr_imap.end());
		ptr_imap.erase(irev);

		/* erase the data vector */
		DataMap::iterator idata = mem_data.find(client_pointer);
		assert(idata != mem_data.end());
		mem_data.erase(idata);

		return result;
	}

	void task_wait()
	{
		device->task_wait();

		if(have_error()) return;

		thread_scoped_lock lock(send_lock);
		RPCSend snd(socket, &error_func, "task_wait_done");
		snd.write();
	}

	/* note that the lock must be already acquired upon entry.
	 * This is necessary because the caller often peeks at
	 * the header and delegates control to here when it doesn't
	 * specifically handle the current RPC.
	 * The lock must be unlocked before returning */
	void process(RPCReceive& rcv, thread_scoped_lock &lock)
	{
		if(rcv.name == "mem_alloc") {
			MemoryType type;
			network_device_memory mem;
			device_ptr client_pointer;

			rcv.read(mem);
			rcv.read(type);

			lock.unlock();

			client_pointer = mem.device_pointer;

			/* create a memory buffer for the device buffer */
			size_t data_size = mem.memory_size();
			DataVector &data_v = data_vector_insert(client_pointer, data_size);

			if(data_size)
				mem.data_pointer = (device_ptr)&(data_v[0]);
			else
				mem.data_pointer = 0;

			/* perform the allocation on the actual device */
			device->mem_alloc(mem, type);

			/* store a mapping to/from client_pointer and real device pointer */
			pointer_mapping_insert(client_pointer, mem.device_pointer);
		}
		else if(rcv.name == "mem_copy_to") {
			network_device_memory mem;

			rcv.read(mem);
			lock.unlock();

			device_ptr client_pointer = mem.device_pointer;

			DataVector &data_v = data_vector_find(client_pointer);

			size_t data_size = mem.memory_size();

			/* get pointer to memory buffer	for device buffer */
			mem.data_pointer = (device_ptr)&data_v[0];

			/* copy data from network into memory buffer */
			rcv.read_buffer((uint8_t*)mem.data_pointer, data_size);

			/* translate the client pointer to a real device pointer */
			mem.device_pointer = device_ptr_from_client_pointer(client_pointer);

			/* copy the data from the memory buffer to the device buffer */
			device->mem_copy_to(mem);
		}
		else if(rcv.name == "mem_copy_from") {
			network_device_memory mem;
			int y, w, h, elem;

			rcv.read(mem);
			rcv.read(y);
			rcv.read(w);
			rcv.read(h);
			rcv.read(elem);

			device_ptr client_pointer = mem.device_pointer;
			mem.device_pointer = device_ptr_from_client_pointer(client_pointer);

			DataVector &data_v = data_vector_find(client_pointer);

			mem.data_pointer = (device_ptr)&(data_v[0]);

			device->mem_copy_from(mem, y, w, h, elem);

			size_t data_size = mem.memory_size();

			RPCSend snd(socket, &error_func, "mem_copy_from_done");
			snd.write();
			snd.write_buffer((uint8_t*)mem.data_pointer, data_size);
			lock.unlock();
		}
		else if(rcv.name == "mem_zero") {
			network_device_memory mem;
			
			rcv.read(mem);
			lock.unlock();

			device_ptr client_pointer = mem.device_pointer;
			mem.device_pointer = device_ptr_from_client_pointer(client_pointer);

			DataVector &data_v = data_vector_find(client_pointer);

			mem.data_pointer = (device_ptr)&(data_v[0]);

			device->mem_zero(mem);
		}
		else if(rcv.name == "mem_free") {
			network_device_memory mem;
			device_ptr client_pointer;

			rcv.read(mem);
			lock.unlock();

			client_pointer = mem.device_pointer;

			mem.device_pointer = device_ptr_from_client_pointer_erase(client_pointer);

			device->mem_free(mem);
		}
		else if(rcv.name == "const_copy_to") {
			string name_string;
			size_t size;

			rcv.read(name_string);
			rcv.read(size);

			vector<char> host_vector(size);
			rcv.read_buffer(&host_vector[0], size);
			lock.unlock();

			device->const_copy_to(name_string.c_str(), &host_vector[0], size);
		}
		else if(rcv.name == "tex_alloc") {
			network_device_memory mem;
			string name;
			InterpolationType interpolation;
			ExtensionType extension_type;
			device_ptr client_pointer;

			rcv.read(name);
			rcv.read(mem);
			rcv.read(interpolation);
			rcv.read(extension_type);
			lock.unlock();

			client_pointer = mem.device_pointer;

			size_t data_size = mem.memory_size();

			DataVector &data_v = data_vector_insert(client_pointer, data_size);

			if(data_size)
				mem.data_pointer = (device_ptr)&(data_v[0]);
			else
				mem.data_pointer = 0;

			rcv.read_buffer((uint8_t*)mem.data_pointer, data_size);

			device->tex_alloc(name.c_str(), mem, interpolation, extension_type);

			pointer_mapping_insert(client_pointer, mem.device_pointer);
		}
		else if(rcv.name == "tex_free") {
			network_device_memory mem;
			device_ptr client_pointer;

			rcv.read(mem);
			lock.unlock();

			client_pointer = mem.device_pointer;

			mem.device_pointer = device_ptr_from_client_pointer_erase(client_pointer);

			device->tex_free(mem);
		}
		else if(rcv.name == "load_kernels") {
			DeviceRequestedFeatures requested_features;
			rcv.read(requested_features.experimental);
			rcv.read(requested_features.max_closure);
			rcv.read(requested_features.max_nodes_group);
			rcv.read(requested_features.nodes_features);

			bool result;
			result = device->load_kernels(requested_features);
			RPCSend snd(socket, &error_func, "load_kernels_done");
			snd.add(result);
			snd.write();
			lock.unlock();
		}
		else if(rcv.name == "task_add") {
			DeviceTask task;

			rcv.read(task);
			lock.unlock();

			if(task.buffer)
				task.buffer = device_ptr_from_client_pointer(task.buffer);

			if(task.rgba_half)
				task.rgba_half = device_ptr_from_client_pointer(task.rgba_half);

			if(task.rgba_byte)
				task.rgba_byte = device_ptr_from_client_pointer(task.rgba_byte);

			if(task.shader_input)
				task.shader_input = device_ptr_from_client_pointer(task.shader_input);

			if(task.shader_output)
				task.shader_output = device_ptr_from_client_pointer(task.shader_output);

			if(task.shader_output_luma)
				task.shader_output_luma = device_ptr_from_client_pointer(task.shader_output_luma);

			if(task.type == DeviceTask::PATH_TRACE) {
				thread_scoped_lock acquire_tile_lock(acquire_tile_mutex);
				out_of_tiles = false;
				acquired_tiles = queue<RenderTile>();
				acquire_tile_lock.unlock();

				thread_scoped_lock lock(send_lock);
				for(int i = 0; i < 2; i++) {
					RPCSend snd(socket, &error_func, "acquire_tile");
					snd.write();
				}
			}

			task.acquire_tile = function_bind(&DeviceServer::task_acquire_tile, this, _1, _2);
			task.release_tile = function_bind(&DeviceServer::task_release_tile, this, _1);
			task.update_progress_sample = function_bind(&DeviceServer::task_update_progress_sample, this);
			task.update_tile_sample = function_bind(&DeviceServer::task_update_tile_sample, this, _1);
			task.get_cancel = function_bind(&DeviceServer::task_get_cancel, this);

			device->task_add(task);
		}
		else if(rcv.name == "task_wait") {
			lock.unlock();

			delete task_wait_thread;
			task_wait_thread = new thread(function_bind(&DeviceServer::task_wait, this));
		}
		else if(rcv.name == "task_cancel") {
			lock.unlock();
			device->task_cancel();
		}
		else if(rcv.name == "acquire_tile_done") {
			RenderTile tile;
			rcv.read(tile);
			{
				thread_scoped_lock buffer_map_lock(buffer_map_mutex);
				RenderBufferMemory &rmem = buffer_map[tile.buffer];
				assert(rmem.mem == NULL);
				rmem.mem = new network_device_memory;
				rcv.read(*rmem.mem);
				rcv.read(rmem.y);
				rcv.read(rmem.w);
				rcv.read(rmem.h);
				rcv.read(rmem.elem);
			}
			lock.unlock();

			thread_scoped_lock acquire_tile_lock(acquire_tile_mutex);
			assert(!out_of_tiles);
			if(acquired_tiles.empty()) {
				cout << "Queue ran out of tiles!\n";
			}
			acquired_tiles.push(tile);
			acquire_tile_cond.notify_one();
		}
		else if(rcv.name == "acquire_tile_none") {
			lock.unlock();

			thread_scoped_lock acquire_tile_lock(acquire_tile_mutex);
			out_of_tiles = true;
			acquire_tile_cond.notify_all();
		}
		else {
			cout << "Error: unexpected RPC receive call \"" + rcv.name + "\"\n";
			lock.unlock();
		}
	}

	bool task_acquire_tile(Device *device, RenderTile& tile)
	{
		if(have_error()) return false;

		thread_scoped_lock lock(send_lock);
		RPCSend snd(socket, &error_func, "acquire_tile");
		snd.write();
		lock.unlock();

		thread_scoped_lock acquire_tile_lock(acquire_tile_mutex);
		while(acquired_tiles.empty() && !out_of_tiles) {
			/* Wait for notifications until either a new tile arrives or there are no more tiles.
			 * Note that the thread might not wait at all because there already were tiles in the queue. */
			acquire_tile_cond.wait(acquire_tile_lock);
		}

		if(acquired_tiles.empty()) {
			assert(out_of_tiles);
			return false;
		}
		tile = acquired_tiles.front();
		acquired_tiles.pop();

		acquire_tile_lock.unlock();

		if(tile.buffer) tile.buffer = ptr_map[tile.buffer];
		if(tile.rng_state) tile.rng_state = ptr_map[tile.rng_state];
		return true;
	}

	void task_update_progress_sample()
	{
		; /* skip */
	}

	void task_update_tile_sample(RenderTile&)
	{
		; /* skip */
	}

	void task_release_tile(RenderTile& tile)
	{
		if(have_error()) return;

		device_ptr buffer_ptr = tile.buffer;
		if(tile.buffer) tile.buffer = ptr_imap[tile.buffer];
		if(tile.rng_state) tile.rng_state = ptr_imap[tile.rng_state];

		size_t data_size;
		uint8_t *data;
		{
			/* Copy render buffer from device. */
			thread_scoped_lock buffer_map_lock(buffer_map_mutex);
			assert(buffer_map.count(tile.buffer));

			RenderBufferMemory &rmem = buffer_map[tile.buffer];
			DataVector &data_v = data_vector_find(tile.buffer);
			rmem.mem->data_pointer = (device_ptr)&(data_v[0]);
			rmem.mem->device_pointer = buffer_ptr;
			data_size = rmem.mem->memory_size();
			data = &data_v[0];
			device->mem_copy_from(*rmem.mem, rmem.y, rmem.w, rmem.h, rmem.elem);
			delete rmem.mem;
			buffer_map.erase(tile.buffer);
		}

		thread_scoped_lock lock(send_lock);
		RPCSend snd(socket, &error_func, "release_tile");
		snd.add(tile);
		snd.write();
		snd.write_buffer(data, data_size);
	}

	bool task_get_cancel()
	{
		return have_error();
	}

	/* properties */
	Device *device;
	tcp::socket& socket;

	/* mapping of remote to local pointer */
	PtrMap ptr_map;
	PtrMap ptr_imap;
	DataMap mem_data;
	struct RenderBufferMemory {
		network_device_memory *mem = NULL;
		int y, w, h, elem;
	};
	map<device_ptr, RenderBufferMemory> buffer_map;
	thread_mutex buffer_map_mutex;

	/*struct AcquireEntry {
		string name;
		RenderTile tile;
	};

	thread_mutex acquire_mutex;
	list<AcquireEntry> acquire_queue;*/

	thread *task_wait_thread;
private:
	NetworkError error_func;

	/* todo: free memory and device (osl) on network error */

};

void Device::server_run()
{
	try {
		/* starts thread that responds to discovery requests */
		ServerDiscovery discovery;

		for(;;) {
			/* accept connection */
			boost::asio::io_service io_service;
			tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), SERVER_PORT));

			tcp::socket socket(io_service);
			acceptor.accept(socket);

			string remote_address = socket.remote_endpoint().address().to_string();
			printf("Connected to remote client at: %s\n", remote_address.c_str());

			DeviceServer server(this, socket);
			try {
				server.listen();
			}
			catch (std::exception& e) {
				cout << e.what() << '\n';
			}

			printf("Disconnected.\n");
		}
	}
	catch(exception& e) {
		fprintf(stderr, "Network server exception: %s\n", e.what());
	}
}

CCL_NAMESPACE_END

#endif


