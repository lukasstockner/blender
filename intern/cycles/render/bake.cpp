/*
 * Copyright 2011-2014 Blender Foundation
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

#include "render/bake.h"
#include "render/buffers.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/shader.h"
#include "render/integrator.h"

#include "util/util_foreach.h"

CCL_NAMESPACE_BEGIN

BakeData::BakeData(const int object, const size_t tri_offset, const size_t num_pixels):
m_object(object),
m_tri_offset(tri_offset),
m_num_pixels(num_pixels)
{
	m_primitive.resize(num_pixels);
	m_u.resize(num_pixels);
	m_v.resize(num_pixels);
	m_dudx.resize(num_pixels);
	m_dudy.resize(num_pixels);
	m_dvdx.resize(num_pixels);
	m_dvdy.resize(num_pixels);
}

BakeData::~BakeData()
{
	m_primitive.clear();
	m_u.clear();
	m_v.clear();
	m_dudx.clear();
	m_dudy.clear();
	m_dvdx.clear();
	m_dvdy.clear();
}

void BakeData::set(int i, int prim, float uv[2], float dudx, float dudy, float dvdx, float dvdy)
{
	m_primitive[i] = (prim == -1 ? -1 : m_tri_offset + prim);
	m_u[i] = uv[0];
	m_v[i] = uv[1];
	m_dudx[i] = dudx;
	m_dudy[i] = dudy;
	m_dvdx[i] = dvdx;
	m_dvdy[i] = dvdy;
}

void BakeData::set_null(int i)
{
	m_primitive[i] = -1;
}

int BakeData::object()
{
	return m_object;
}

size_t BakeData::size()
{
	return m_num_pixels;
}

bool BakeData::is_valid(int i)
{
	return m_primitive[i] != -1;
}

uint4 BakeData::data(int i)
{
	return make_uint4(
		m_object,
		m_primitive[i],
		__float_as_int(m_u[i]),
		__float_as_int(m_v[i])
		);
}

uint4 BakeData::differentials(int i)
{
	return make_uint4(
		  __float_as_int(m_dudx[i]),
		  __float_as_int(m_dudy[i]),
		  __float_as_int(m_dvdx[i]),
		  __float_as_int(m_dvdy[i])
		  );
}

BakeManager::BakeManager()
{
	m_bake_data = NULL;
	m_is_baking = false;
	need_update = true;
	m_shader_limit = 512 * 512;

	use_denoising = false;
	denoising_radius = 8;
	denoising_strength = 0.5f;
	denoising_feature_strength = 0.5f;
	denoising_relative_pca = false;
	denoising_tile_size = make_int2(64, 64);
	denoising_channels = 0;
}

BakeManager::~BakeManager()
{
	if(m_bake_data)
		delete m_bake_data;
}

bool BakeManager::get_baking()
{
	return m_is_baking;
}

void BakeManager::set_baking(const bool value)
{
	m_is_baking = value;
}

BakeData *BakeManager::init(const int object, const size_t tri_offset, const size_t num_pixels)
{
	m_bake_data = new BakeData(object, tri_offset, num_pixels);
	return m_bake_data;
}

void BakeManager::set_shader_limit(const size_t x, const size_t y)
{
	m_shader_limit = x * y;
	m_shader_limit = (size_t)pow(2, ceil(log(m_shader_limit)/log(2)));
}

bool BakeManager::bake(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress, ShaderEvalType shader_type, const int pass_filter, BakeData *bake_data, float result[], device_vector<float> &denoising_data)
{
	size_t num_pixels = bake_data->size();

	num_samples = aa_samples(scene, bake_data, shader_type);

	/* calculate the total pixel samples for the progress bar */
	total_pixel_samples = 0;
	for(size_t shader_offset = 0; shader_offset < num_pixels; shader_offset += m_shader_limit) {
		size_t shader_size = (size_t)fminf(num_pixels - shader_offset, m_shader_limit);
		total_pixel_samples += shader_size * num_samples;
	}
	progress.reset_sample();
	progress.set_total_pixel_samples(total_pixel_samples);


	/* If a single device is used, the denoising data never has to be copied to the host.
	 * However, if multiple devices are used, all devices need to have all denoising data,
	 * so we copy each devices' results to the host and then copy the result back to all devices. */
	bool is_multi_device = device->info.type == DEVICE_MULTI;
	device_vector<float> local_denoising_data(device, "bake_local_denoising", MEM_READ_WRITE);

	if(use_denoising) {
		/*  0 -  5: Position with Variance
		 *  6 - 11: Normals with Variance
		 * 12 - 17: Shadow A and B
		 * 18 - 23: Color with Variance
		 * 24     : Radius */
		denoising_channels = 25;

		/* Store albedo unless the color is what is being baked or is already divided out during baking. */
		if((pass_filter & BAKE_FILTER_COLOR) && (pass_filter & (BAKE_FILTER_DIRECT | BAKE_FILTER_INDIRECT))) {
			denoising_channels += 6;
		}

		denoising_data.alloc(num_pixels * denoising_channels);
		if(is_multi_device) {
			local_denoising_data.alloc(m_shader_limit * denoising_channels);
		}
		else {
			denoising_data.zero_to_device();
		}
	}
	else {
		denoising_channels = 0;
	}

	/* Update kernel constants. */
	dscene->data.integrator.aa_samples = num_samples;
	dscene->data.film.denoising_radius = 8;
	dscene->data.film.pass_stride = denoising_channels;
	device->const_copy_to("__data", &dscene->data, sizeof(dscene->data));

	for(size_t shader_offset = 0; shader_offset < num_pixels; shader_offset += m_shader_limit) {
		size_t shader_size = (size_t)fminf(num_pixels - shader_offset, m_shader_limit);

		/* setup input for device task */
		device_vector<uint4> d_input(device, "bake_input", MEM_READ_ONLY);
		uint4 *d_input_data = d_input.alloc(shader_size * 2);
		size_t d_input_size = 0;

		for(size_t i = shader_offset; i < (shader_offset + shader_size); i++) {
			d_input_data[d_input_size++] = bake_data->data(i);
			d_input_data[d_input_size++] = bake_data->differentials(i);
		}

		if(d_input_size == 0) {
			m_is_baking = false;
			return false;
		}

		/* run device task */
		device_vector<float4> d_output(device, "bake_output", MEM_READ_WRITE);
		d_output.alloc(shader_size);
		d_output.zero_to_device();
		d_input.copy_to_device();

		/* Get denoising data memory - use a subpointer into the full vector
		 * for single devices or the local memory for multiple devices. */
		int denoising_data_offset = shader_offset * denoising_channels;
		int denoising_data_size = shader_size * denoising_channels;
		device_sub_ptr *d_denoising = NULL;
		device_ptr denoising_ptr = 0;

		if(use_denoising) {
			if(is_multi_device) {
				local_denoising_data.zero_to_device();
				denoising_ptr = local_denoising_data.device_pointer;
			}
			else {
				d_denoising = new device_sub_ptr(denoising_data, denoising_data_offset, denoising_data_size);
				denoising_ptr = d_denoising->get();
			}
		}

		DeviceTask task(DeviceTask::SHADER);
		task.shader_input = d_input.device_pointer;
		task.shader_output = d_output.device_pointer;
		task.shader_denoising = denoising_ptr;
		task.shader_eval_type = shader_type;
		task.shader_filter = pass_filter;
		task.shader_x = 0;
		task.offset = shader_offset;
		task.shader_w = d_output.size();
		task.num_samples = num_samples;
		task.get_cancel = function_bind(&Progress::get_cancel, &progress);
		task.update_progress_sample = function_bind(&Progress::add_samples_update, &progress, _1, _2);

		device->task_add(task);
		device->task_wait();

		if(progress.get_cancel()) {
			d_input.free();
			d_output.free();
			m_is_baking = false;
			return false;
		}

		d_output.copy_from_device(0, 1, d_output.size());
		d_input.free();

		if(use_denoising) {
			/* Copy local denoising memory back into the full vector if it's used, free the subpointer otherwise. */
			if(is_multi_device) {
				local_denoising_data.copy_from_device(0, denoising_channels, d_output.size());
				memcpy(&denoising_data[denoising_data_offset], local_denoising_data.data(), denoising_data_size*sizeof(float));
			}
			else {
				delete d_denoising;
			}
		}

		/* read result */
		int k = 0;

		float4 *offset = d_output.data();

		size_t depth = 4;
		for(size_t i=shader_offset; i < (shader_offset + shader_size); i++) {
			size_t index = i * depth;
			float4 out = offset[k++];

			if(bake_data->is_valid(i)) {
				for(size_t j=0; j < 4; j++) {
					result[index + j] = out[j];
				}
			}
		}

		d_output.free();
	}

	if(use_denoising && is_multi_device) {
		local_denoising_data.free();
		denoising_data.copy_to_device();
	}

	m_is_baking = false;
	return true;
}

bool BakeManager::acquire_tile(Device *device, Device *tile_device, RenderTile &tile)
{
	thread_scoped_lock tile_lock(denoising_tiles_mutex);

	if(denoising_tiles.empty()) {
		return false;
	}

	tile = denoising_tiles.front();
	denoising_tiles.pop_front();

	device->map_tile(tile_device, tile);

	return true;
}

/* Mapping tiles is required for regular rendering since each tile has its separate memory
 * which may be allocated on a different device.
 * For Baking, there is a single memory that is present on all devices, so the only
 * thing that needs to be done here is to specify the surrounding tile geometry.
 *
 * However, since there is only one large memory, the denoised result has to be written to
 * a different buffer to avoid having to copy an entire horizontal slice of the image. */
void BakeManager::map_neighboring_tiles(RenderTile *tiles, Device *tile_device, int width, int height)
{
	for(int i = 0; i < 9; i++) {
		if(i == 4) {
			continue;
		}

		int dx = (i%3)-1;
		int dy = (i/3)-1;
		tiles[i].x = clamp(tiles[4].x +  dx   *denoising_tile_size.x, 0,  width);
		tiles[i].w = clamp(tiles[4].x + (dx+1)*denoising_tile_size.x, 0,  width) - tiles[i].x;
		tiles[i].y = clamp(tiles[4].y +  dy   *denoising_tile_size.y, 0, height);
		tiles[i].h = clamp(tiles[4].y + (dy+1)*denoising_tile_size.y, 0, height) - tiles[i].y;

		tiles[i].buffer = tiles[4].buffer;
		tiles[i].offset = tiles[4].offset;
		tiles[i].stride = width;
	}

	device_vector<float> *target_mem = new device_vector<float>(tile_device, "bake_denoising_target", MEM_READ_WRITE);
	target_mem->alloc(3*width*height);
	target_mem->zero_to_device();

	tiles[9] = tiles[4];
	tiles[9].buffer = target_mem->device_pointer;
	tiles[9].stride = tiles[9].w;
	tiles[9].offset -= tiles[9].x + tiles[9].y*tiles[9].stride;

	thread_scoped_lock target_lock(denoising_targets_mutex);
	assert(denoising_targets.count(tiles[4].tile_index) == 0);
	denoising_targets[tiles[9].tile_index] = target_mem;
}

void BakeManager::unmap_neighboring_tiles(RenderTile *tiles, int width, float *result)
{
	thread_scoped_lock target_lock(denoising_targets_mutex);
	assert(denoising_targets.count(tiles[4].tile_index) == 1);
	device_vector<float> *target_mem = denoising_targets[tiles[9].tile_index];
	denoising_targets.erase(tiles[4].tile_index);
	target_lock.unlock();

	target_mem->copy_from_device(0, 3*tiles[9].w, tiles[9].h);

	float *out = target_mem->data();
	result += 4*(tiles[9].y*width + tiles[9].x);
	for(int y = 0; y < tiles[9].h; y++) {
		for(int x = 0; x < tiles[9].w; x++, out += 3) {
			result[4*x+0] = out[0];
			result[4*x+1] = out[1];
			result[4*x+2] = out[2];
			result[4*x+3] = 1.0f;
		}
		result += 4*width;
	}

	target_mem->free();
	delete target_mem;
}

void BakeManager::release_tile()
{
}

bool BakeManager::denoise(Device *device, Progress& progress, BakeData *bake_data, device_vector<float> &denoising_data, float result[], int offset, int width, int height)
{
	denoising_tiles.clear();
	denoising_targets.clear();

	int tiles_x = divide_up(width, denoising_tile_size.x), tiles_y = divide_up(height, denoising_tile_size.y);
	for(int ty = 0; ty < tiles_y; ty++) {
		for(int tx = 0; tx < tiles_x; tx++) {
			RenderTile tile;
			tile.x = tx * denoising_tile_size.x;
			tile.y = ty * denoising_tile_size.y;
			tile.w = min(width - tile.x, denoising_tile_size.x);
			tile.h = min(height - tile.y, denoising_tile_size.y);
			tile.start_sample = 0;
			tile.num_samples = num_samples;
			tile.sample = 0;
			tile.offset = offset;
			tile.stride = width;
			tile.tile_index = ty*tiles_x + tx;
			tile.task = RenderTile::DENOISE;
			tile.buffers = NULL;
			tile.buffer = denoising_data.device_pointer;
			denoising_tiles.push_back(tile);
		}
	}

	DeviceTask task(DeviceTask::RENDER);
	task.acquire_tile = function_bind(&BakeManager::acquire_tile, this, device, _1, _2);
	task.map_neighbor_tiles = function_bind(&BakeManager::map_neighboring_tiles, this, _1, _2, width, height);
	task.unmap_neighbor_tiles = function_bind(&BakeManager::unmap_neighboring_tiles, this, _1, width, result);
	task.release_tile = function_bind(&BakeManager::release_tile, this);
	task.get_cancel = function_bind(&Progress::get_cancel, &progress);
	task.denoising_radius = denoising_radius;
	task.denoising_feature_strength = denoising_feature_strength;
	task.denoising_strength = denoising_strength;
	task.denoising_relative_pca = denoising_relative_pca;
	task.pass_stride = denoising_channels;
	task.target_pass_stride = 3;
	task.pass_denoising_data = 0;
	task.pass_denoising_clean = 0;

	device->task_add(task);
	device->task_wait();

	assert(denoising_targets.empty());

	return !progress.get_cancel();
}

void BakeManager::device_update(Device * /*device*/,
                                DeviceScene * /*dscene*/,
                                Scene * /*scene*/,
                                Progress& progress)
{
	if(!need_update)
		return;

	if(progress.get_cancel()) return;

	need_update = false;
}

void BakeManager::device_free(Device * /*device*/, DeviceScene * /*dscene*/)
{
}

int BakeManager::aa_samples(Scene *scene, BakeData *bake_data, ShaderEvalType type)
{
	if(type == SHADER_EVAL_UV) {
		return 1;
	}
	else if(type == SHADER_EVAL_NORMAL) {
		/* Only antialias normal if mesh has bump mapping. */
		Object *object = scene->objects[bake_data->object()];

		if(object->mesh) {
			foreach(Shader *shader, object->mesh->used_shaders) {
				if(shader->has_bump) {
					return scene->integrator->aa_samples;
				}
			}
		}

		return 1;
	}
	else {
		return scene->integrator->aa_samples;
	}
}

/* Keep it synced with kernel_bake.h logic */
int BakeManager::shader_type_to_pass_filter(ShaderEvalType type, const int pass_filter)
{
	const int component_flags = pass_filter & (BAKE_FILTER_DIRECT | BAKE_FILTER_INDIRECT | BAKE_FILTER_COLOR);

	switch(type) {
		case SHADER_EVAL_AO:
			return BAKE_FILTER_AO;
		case SHADER_EVAL_SHADOW:
			return BAKE_FILTER_DIRECT;
		case SHADER_EVAL_DIFFUSE:
			return BAKE_FILTER_DIFFUSE | component_flags;
		case SHADER_EVAL_GLOSSY:
			return BAKE_FILTER_GLOSSY | component_flags;
		case SHADER_EVAL_TRANSMISSION:
			return BAKE_FILTER_TRANSMISSION | component_flags;
		case SHADER_EVAL_SUBSURFACE:
			return BAKE_FILTER_SUBSURFACE | component_flags;
		case SHADER_EVAL_COMBINED:
			return pass_filter;
		default:
			return 0;
	}
}

CCL_NAMESPACE_END
