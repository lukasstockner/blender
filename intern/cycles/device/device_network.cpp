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

#include "device/device.h"
#include "device/device_intern.h"
#include "device/device_network.h"

#include "util/util_foreach.h"
#include "util/util_queue.h"
#include "util/util_logging.h"
#include "util/util_time.h"

#if defined(WITH_NETWORK)

CCL_NAMESPACE_BEGIN

typedef map<device_ptr, device_ptr> PtrMap;
typedef vector<uint8_t> DataVector;
typedef map<device_ptr, DataVector> DataMap;

class NetworkDevice : public Device
{
public:
	boost::asio::io_service io_service;
	tcp::socket socket;
	device_ptr mem_counter;
	DeviceTask the_task; /* todo: handle multiple tasks */

	thread_mutex rpc_lock;

	TaskPool pool;

	bool server_uses_qbvh;
	int server_num_active_tiles;

	virtual bool show_samples() const
	{
		return false;
	}

	virtual bool use_qbvh() const
	{
		return server_uses_qbvh;
	}

	virtual int num_active_tiles() const
	{
		return server_num_active_tiles;
	}

	NetworkDevice(DeviceInfo& info, Stats &stats, bool background)
	: Device(info, stats, background), socket(io_service)
	{
		error_func = NetworkError();
		stringstream portstr;
		portstr << (info.port? info.port : SERVER_PORT);

		tcp::resolver resolver(io_service);
		tcp::resolver::query query(info.address, portstr.str());
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		tcp::resolver::iterator end;

		boost::system::error_code error = boost::asio::error::host_not_found;
		while(error && endpoint_iterator != end)
		{
			socket.close();
			socket.connect(*endpoint_iterator++, error);
		}

		socket.set_option(tcp::no_delay(true));

		if(error)
			error_func.network_error(error.message());

		RPCReceive rcv(socket, &error_func);
		if(rcv.name != "hello") {
			error_func.network_error("Wrong initialization!\n");
			server_uses_qbvh = true;
			server_num_active_tiles = 1;
		}
		else {
			rcv.read(server_uses_qbvh);
			rcv.read(server_num_active_tiles);

			/* Different compilation options can cause incompatibility.
			 * This check is not perfect, but it's something. */
			int kernel_data_size;
			rcv.read(kernel_data_size);
			if(kernel_data_size != sizeof(KernelData)) {
				error_func.network_error("KernelData size mismatch!\n");
			}
		}

		mem_counter = 0;
		server_listening = true;
	}

	~NetworkDevice()
	{
		RPCSend snd(socket, &error_func, "stop");
		snd.write();
	}

	void mem_alloc(const char *name, device_memory& mem, MemoryType type)
	{
		if(name) {
			VLOG(1) << "Buffer allocate: " << name << ", "
				    << string_human_readable_number(mem.memory_size()) << " bytes. ("
				    << string_human_readable_size(mem.memory_size()) << ")";
		}

		thread_scoped_lock lock(rpc_lock);

		mem.device_pointer = ++mem_counter;

		mem_data_pointers[mem.device_pointer] = (void*) mem.data_pointer;

		RPCSend snd(socket, &error_func, "mem_alloc");

		snd.add(mem);
		snd.add(type);
		snd.write();
	}

	void mem_copy_to(device_memory& mem)
	{
		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "mem_copy_to");

		snd.add(mem);
		snd.write();
		snd.write_buffer((void*)mem.data_pointer, mem.memory_size());
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		/* While the listening thread is running, we might get calls from the
		 * server anytime. Therefore, we can't really wait for an answer here,
		 * since we might get another call first and handling these outside
		 * of the dedicated thread is tricky. */
		assert(server_listening);

		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "mem_copy_from");

		snd.add(mem);
		snd.add(y);
		snd.add(w);
		snd.add(h);
		snd.add(elem);
		snd.write();

		size_t offset = elem*y*w;
		size_t size = elem*w*h;

		RPCReceive rcv(socket, &error_func);
		rcv.read_buffer((uint8_t*)mem.data_pointer + offset, size);
	}

	void mem_zero(device_memory& mem)
	{
		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "mem_zero");

		snd.add(mem);
		snd.write();
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			thread_scoped_lock lock(rpc_lock);

			RPCSend snd(socket, &error_func, "mem_free");

			snd.add(mem);
			snd.write();

			mem_data_pointers.erase(mem.device_pointer);
			mem.device_pointer = 0;
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
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
		if(error_func.have_error())
			return false;

		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "load_kernels");
		snd.add(requested_features.experimental);
		snd.add(requested_features.max_closure);
		snd.add(requested_features.max_nodes_group);
		snd.add(requested_features.nodes_features);
		snd.write();

		bool result;
		RPCReceive rcv(socket, &error_func);
		rcv.read(result);

		return result;
	}

	void pixels_copy_from(device_memory& /*mem*/, int /*y*/, int /*w*/, int /*h*/)
	{
		/* No-op, is handled as part of task_wait_done. */
	}

	void listen_task()
	{
		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "task_wait");
		snd.write();
		server_listening = false;
		lock.unlock();

		for(;;) {
			if(error_func.have_error())
				break;

			RenderTile tile;

			lock.lock();
			RPCReceive rcv(socket, &error_func);

			if(rcv.name == "release_tile") {
				rcv.read(tile);
				server_listening = true;
				lock.unlock();

				tile.buffers = tile_buffers[tile.tile_index];

				assert(tile.buffers != NULL);

				/* Don't call full update, we don't want to copy the tile for updating as well as for writing. */
				if(the_task.update_progress_sample) {
					the_task.update_progress_sample(tile.w*tile.h*tile.num_samples, 0);
				}

				the_task.release_tile(tile);

				RenderTile new_tile;
				bool got_tile = the_task.acquire_tile(this, new_tile);

				lock.lock();
				RPCSend snd(socket, &error_func, "next_tile");
				snd.add(got_tile);
				if(got_tile) {
					tile_buffers[new_tile.tile_index] = new_tile.buffers;
					snd.add(new_tile);
				}
				snd.write();
				server_listening = false;
				lock.unlock();
			}
			else if(rcv.name == "task_wait_done") {
				if(the_task.type == DeviceTask::FILM_CONVERT) {
					int size;
					rcv.read(size);
					device_ptr device_pointer = the_task.rgba_half? the_task.rgba_half : the_task.rgba_byte;
					void *host_pointer = (void*) mem_data_pointers[device_pointer];
					assert(host_pointer);
					rcv.read_buffer(host_pointer, size);
				}
				server_listening = true;
				lock.unlock();
				break;
			}
			else {
				assert(false);
				lock.unlock();
			}
		}
	}

	void task_add(DeviceTask& task)
	{
		thread_scoped_lock lock(rpc_lock);

		the_task = task;

		RPCSend snd(socket, &error_func, "task_add");
		snd.add(task);
		snd.write();

		if(task.type == DeviceTask::RENDER) {
			tile_buffers.clear();

			RPCReceive rec(socket, &error_func);
			assert(rec.name == "get_tiles");
			int num_tiles;
			rec.read(num_tiles);
			lock.unlock();

			vector<RenderTile> tiles;
			for(int i = 0; i < num_tiles; i++) {
				RenderTile tile;
				if(!task.acquire_tile(this, tile)) {
					break;
				}
				tile_buffers[tile.tile_index] = tile.buffers;
				tiles.push_back(tile);
			}

			lock.lock();
			RPCSend snd2(socket, &error_func, "tiles");
			snd2.add((int) tiles.size());
			for(int i = 0; i < tiles.size(); i++) {
				snd2.add(tiles[i]);
			}
			snd2.write();
		}

		pool.push(function_bind(&NetworkDevice::listen_task, this));
	}

	void task_wait()
	{
		pool.wait_work();
	}

	void task_cancel()
	{
		thread_scoped_lock lock(rpc_lock);
		RPCSend snd(socket, &error_func, "task_cancel");
		snd.write();
	}

	int get_split_task_count(DeviceTask& /*task*/)
	{
		return 1;
	}

private:
	NetworkError error_func;

	unordered_map<int, RenderBuffers*> tile_buffers;
	unordered_map<device_ptr, void*> mem_data_pointers;
	bool server_listening;
};

Device *device_network_create(DeviceInfo& info, Stats &stats,bool background)
{
	return new NetworkDevice(info, stats, background);
}

void device_network_info(vector<DeviceInfo>& devices, string serverstring)
{
	const char *servers_override = getenv("CYCLES_SERVERS");
	if(servers_override) {
		serverstring = string(servers_override);
	}

	if(serverstring == "") {
		return;
	}

	vector<string> servers;

	if(serverstring.substr(0, 4) == "WAIT") {
		float time = 1.0f;
		sscanf(serverstring.c_str(), "WAIT%f", &time);
		ServerDiscovery discovery(true);
		time_sleep((double) time);
		discovery.get_server_list().swap(servers);
		printf("Remote discovery found %d servers!\n", servers.size());
	}
	else {
		string_split(servers, serverstring, ";");
	}

	int id = 0;
	foreach(string &server, servers) {
		vector<string> parts;
		string_split(parts, server, ":");

		DeviceInfo info;

		info.type = DEVICE_NETWORK;
		info.description = "Network Device";
		info.id = string_printf("NETWORK%d", id);
		info.num = id;
		info.advanced_shading = true; /* todo: get this info from device */
		if(parts.size() == 1) {
			info.address = parts[0];
		}
		else if(parts.size() == 2) {
			info.address = parts[0];
			info.port = atoi(parts[1].c_str());
		}
		else {
			fprintf(stderr, "Invalid server format: %s\n", server.c_str());
			continue;
		}
		devices.push_back(info);
		id++;
	}
}

class DeviceServer {
public:
	thread_mutex rpc_lock;

	void network_error(const string &message) {
		error_func.network_error(message);
	}

	bool have_error() { return error_func.have_error(); }

	DeviceServer(Device *device_, tcp::socket& socket_)
	: device(device_), socket(socket_), task_finished(true), stop(false)
	{
		error_func = NetworkError();

		socket.set_option(tcp::no_delay(true));

		tile_queue_size = (int) sqrt(device->num_active_tiles());
		char *queue_size_override = getenv("CYCLES_TILE_QUEUE");
		if(queue_size_override) {
			tile_queue_size = atoi(queue_size_override);
		}
	}

	void listen()
	{
		RPCSend snd(socket, &error_func, "hello");
		snd.add(device->use_qbvh());
		snd.add(device->num_active_tiles());
		snd.add((int) sizeof(KernelData));
		snd.write();

		/* receive remote function calls */
		for(;;) {
			listen_step();

			if(stop)
				break;
		}
	}

protected:
	void listen_step()
	{
		thread_scoped_lock lock(rpc_lock);
		RPCReceive rcv(socket, &error_func);

		if(rcv.name == "stop" || rcv.name.empty())
			stop = true;
		else
			process(rcv, lock);
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

	device_ptr client_pointer_from_device_ptr(device_ptr real_pointer)
	{
		PtrMap::iterator i = ptr_imap.find(real_pointer);
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

	RPCReceive *listen_for_call(const string call, thread_scoped_lock &lock)
	{
		for(;;) {
			RPCReceive *rcv = new RPCReceive(socket, &error_func);

			if(rcv->name == "stop" || rcv->name.empty()) {
				stop = true;
				return rcv;
			}
			else if(rcv->name == call) {
				return rcv;
			}
			else {
				process(*rcv, lock);
				delete rcv;
				lock.lock();
			}
		}
	}

	void handle_queues()
	{
		for(;;) {
			thread_scoped_lock finished_lock(finished_mutex);
			while(!task_finished && finished_queue.empty()) {
				finished_condition.wait(finished_lock);
			}
			if(finished_queue.empty()) return;
			RenderTile fin_tile = finished_queue.front();
			finished_queue.pop();
			finished_lock.unlock();

			bool need_tile;
			{
				thread_scoped_lock acquired_lock(acquired_mutex);
				need_tile = acquired_queue.size() < tile_queue_size;
			}

			thread_scoped_lock lock(rpc_lock);
			RPCSend snd(socket, &error_func, "release_tile");
			snd.add(fin_tile);
			snd.add(need_tile);
			snd.write();

			RPCReceive *rec = listen_for_call("next_tile", lock);
			bool has_tile;
			rec->read(has_tile);
			if(has_tile) {
				RenderTile tile;
				rec->read(tile);
				delete rec;
				lock.unlock();

				thread_scoped_lock acquired_lock(acquired_mutex);
				acquired_queue.push(tile);
				acquired_condition.notify_one();
			}
			else {
				lock.unlock();
				thread_scoped_lock acquired_lock(acquired_mutex);
				out_of_tiles = true;
				acquired_condition.notify_all();
			}
		}
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
			device->mem_alloc(NULL, mem, type);

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

			size_t offset = elem*y*w;
			size_t size = elem*w*h;

			device_ptr client_pointer = mem.device_pointer;
			mem.device_pointer = device_ptr_from_client_pointer(client_pointer);

			DataVector &data_v = data_vector_find(client_pointer);

			mem.data_pointer = (device_ptr)&(data_v[0]);

			device->mem_copy_from(mem, y, w, h, elem);

			RPCSend snd(socket, &error_func, "mem_copy_from");
			snd.write();
			snd.write_buffer((uint8_t*)mem.data_pointer + offset, size);
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
			rcv.read(mem);
			lock.unlock();

			device_ptr client_pointer = mem.device_pointer;

			DataVector &data_v = data_vector_find(client_pointer);
			mem.data_pointer = (device_ptr)&(data_v[0]);
			mem.device_pointer = device_ptr_from_client_pointer(client_pointer);

			device->mem_free(mem);

			device_ptr_from_client_pointer_erase(client_pointer);
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
			rcv.read(mem);
			lock.unlock();

			device_ptr client_pointer = mem.device_pointer;

			DataVector &data_v = data_vector_find(client_pointer);
			mem.data_pointer = (device_ptr)&(data_v[0]);
			mem.device_pointer = device_ptr_from_client_pointer(client_pointer);

			device->tex_free(mem);

			device_ptr_from_client_pointer_erase(client_pointer);
		}
		else if(rcv.name == "load_kernels") {
			DeviceRequestedFeatures requested_features;
			rcv.read(requested_features.experimental);
			rcv.read(requested_features.max_closure);
			rcv.read(requested_features.max_nodes_group);
			rcv.read(requested_features.nodes_features);

			bool result;
			result = device->load_kernels(requested_features);
			RPCSend snd(socket, &error_func, "load_kernels");
			snd.add(result);
			snd.write();
			lock.unlock();
		}
		else if(rcv.name == "task_add") {
			assert(task_finished);

			rcv.read(task);

			if(task.type == DeviceTask::RENDER) {
				/* Fill the acquired tile queue.
				 * Since that might generate additional calls to memory allocation etc.,
				 * keep listening until we finally get the tiles back. */
				RPCSend snd(socket, &error_func, "get_tiles");
				snd.add(device->num_active_tiles() + tile_queue_size);
				snd.write();

				RPCReceive *rec = listen_for_call("tiles", lock);
				if(rec->name == "tiles") {
					int tiles;
					rec->read(tiles);
					thread_scoped_lock acquired_lock(acquired_mutex);
					if(tiles == 0) {
						out_of_tiles = true;
					}
					else {
						out_of_tiles = false;
						for(int i = 0; i < tiles; i++) {
							RenderTile tile;
							rec->read(tile);
							acquired_queue.push(tile);
						}
					}
					delete rec;
				}
				else {
					assert(false);
					/* TODO */
				}
			}
			lock.unlock();

			task_finished = false;

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

			task.acquire_tile = function_bind(&DeviceServer::task_acquire_tile, this, _1, _2);
			task.release_tile = function_bind(&DeviceServer::task_release_tile, this, _1);
			task.update_progress_sample = function_bind(&DeviceServer::task_update_progress_sample, this);
			task.update_tile_sample = function_bind(&DeviceServer::task_update_tile_sample, this, _1);
			task.get_cancel = function_bind(&DeviceServer::task_get_cancel, this);

			device->task_add(task);
		}
		else if(rcv.name == "task_wait") {
			lock.unlock();

			thread queue_handler(function_bind(&DeviceServer::handle_queues, this));

			device->task_wait();

			/* Poke queue handler thread to stop since no more tiles are coming. */
			thread_scoped_lock finished_lock(finished_mutex);
			task_finished = true;
			finished_condition.notify_all();
			finished_lock.unlock();
			queue_handler.join();

			lock.lock();
			RPCSend snd(socket, &error_func, "task_wait_done");

			if(task.type == DeviceTask::FILM_CONVERT) {
				device_memory mem;
				mem.device_pointer = task.rgba_half? task.rgba_half : task.rgba_byte;
				DataVector &data_v = data_vector_find(client_pointer_from_device_ptr(mem.device_pointer));
				mem.data_pointer = (device_ptr)&(data_v[0]);
				int size = data_v.size();
				/* TODO: This will not work on MultiDevices. */
				device->mem_copy_from(mem, 0, size, 1, 1);

				snd.add(size);
				snd.write();
				snd.write_buffer((void*) mem.data_pointer, size);
			}
			else {
				snd.write();
			}
			lock.unlock();
		}
		else if(rcv.name == "task_cancel") {
			lock.unlock();
			device->task_cancel();
		}
		else {
			cout << "Error: unexpected RPC receive call \"" + rcv.name + "\"\n";
			lock.unlock();
		}
	}

	bool task_acquire_tile(Device * /*device*/, RenderTile& tile)
	{
		/* Todo: The client assumes that all tiles are on the NetworkDevice,
		 * not the device that's passed here.
		 * Therefore, MultiDevices will probably not work on servers yet. */
		double time = 0.0;
		{
			scoped_timer timer(&time);
			thread_scoped_lock acquired_lock(acquired_mutex);

			while(!out_of_tiles && acquired_queue.empty() && !stop && !have_error()) {
				printf("Tile queue ran out!\n");
				acquired_condition.wait(acquired_lock);
			}

			if(acquired_queue.empty()) return false;

			tile = acquired_queue.front();
			acquired_queue.pop();

			acquired_lock.unlock();

			if(tile.buffer) tile.buffer = ptr_map[tile.buffer];
			if(tile.rng_state) tile.rng_state = ptr_map[tile.rng_state];
		}
		if(time > 0.01) {
			printf("Acquired tile in %f msec!\n", time * 1000.0);
		}

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
		if(tile.buffer) tile.buffer = ptr_imap[tile.buffer];
		if(tile.rng_state) tile.rng_state = ptr_imap[tile.rng_state];

		thread_scoped_lock finished_lock(finished_mutex);
		finished_queue.push(tile);
		finished_condition.notify_one();
	}

	bool task_get_cancel()
	{
		return false;
	}

	/* properties */
	Device *device;
	tcp::socket& socket;

	/* mapping of remote to local pointer */
	PtrMap ptr_map;
	PtrMap ptr_imap;
	DataMap mem_data;

	struct TileEntry {
		string name;
		RenderTile tile;
	};

	thread_mutex acquired_mutex;
	thread_mutex finished_mutex;
	queue<RenderTile> acquired_queue;
	queue<RenderTile> finished_queue;
	thread_condition_variable acquired_condition;
	thread_condition_variable finished_condition;

	bool task_finished; /* protected by finished_mutex */
	bool out_of_tiles;  /* protected by acquired_mutex */

	int tile_queue_size;

	DeviceTask task;

	bool stop;
private:
	NetworkError error_func;

	/* todo: free memory and device (osl) on network error */

};

void Device::server_run(string announce_address)
{
	try {
		/* starts thread that responds to discovery requests */
		ServerDiscovery discovery(false, announce_address);

		for(;;) {
			/* accept connection */
			boost::asio::io_service io_service;
			const char *portstring = getenv("CYCLES_PORT");
			int port = portstring? atoi(portstring) : SERVER_PORT;
			tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), port));

			tcp::socket socket(io_service);
			acceptor.accept(socket);

			string remote_address = socket.remote_endpoint().address().to_string();
			printf("Connected to remote client at: %s\n", remote_address.c_str());

			DeviceServer server(this, socket);
			server.listen();

			printf("Disconnected.\n");
		}
	}
	catch(exception& e) {
		fprintf(stderr, "Network server exception: %s\n", e.what());
	}
}

CCL_NAMESPACE_END

#endif


