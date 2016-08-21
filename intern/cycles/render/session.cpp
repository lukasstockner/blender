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

#include <string.h>
#include <limits.h>

#include "buffers.h"
#include "camera.h"
#include "device.h"
#include "graph.h"
#include "integrator.h"
#include "mesh.h"
#include "object.h"
#include "scene.h"
#include "session.h"
#include "bake.h"

#include "util_foreach.h"
#include "util_function.h"
#include "util_logging.h"
#include "util_math.h"
#include "util_opengl.h"
#include "util_task.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

/* Note about  preserve_tile_device option for tile manager:
 * progressive refine and viewport rendering does requires tiles to
 * always be allocated for the same device
 */
Session::Session(const SessionParams& params_)
: params(params_),
  tile_manager(params.progressive, params.samples, params.tile_size, params.start_resolution,
       params.background == false || params.progressive_refine, params.background, params.tile_order,
       max(params.device.multi_devices.size(), 1), params.only_denoise),
  stats()
{
	device_use_gl = ((params.device.type != DEVICE_CPU) && !params.background);

	TaskScheduler::init(params.threads);

	device = Device::create(params.device, stats, params.background);

	if(params.background && params.output_path.empty()) {
		buffers = NULL;
		display = NULL;
	}
	else {
		buffers = new RenderBuffers(device);
		display = new DisplayBuffer(device, params.display_buffer_linear);
	}

	session_thread = NULL;
	scene = NULL;

	start_time = 0.0;
	reset_time = 0.0;
	preview_time = 0.0;
	paused_time = 0.0;
	last_update_time = 0.0;

	delayed_reset.do_reset = false;
	delayed_reset.samples = 0;

	display_outdated = false;
	gpu_draw_ready = false;
	gpu_need_tonemap = false;
	pause = false;
	kernels_loaded = false;

	/* TODO(sergey): Check if it's indeed optimal value for the split kernel. */
	max_closure_global = 1;
}

Session::~Session()
{
	if(session_thread) {
		/* wait for session thread to end */
		progress.set_cancel("Exiting");

		gpu_need_tonemap = false;
		gpu_need_tonemap_cond.notify_all();

		{
			thread_scoped_lock pause_lock(pause_mutex);
			pause = false;
		}
		pause_cond.notify_all();

		wait();
	}

	if(!params.output_path.empty()) {
		/* tonemap and write out image if requested */
		delete display;

		display = new DisplayBuffer(device, params.output_half_float);
		display->flip_image = params.flip_output;
		display->reset(device, buffers->params);
		tonemap(params.samples);

		progress.set_status("Writing Image", params.output_path);
		display->write(device, params.output_path);
	}

	/* clean up */
	foreach(RenderBuffers *buffers, tile_buffers)
		delete buffers;
	tile_manager.free_device();

	delete buffers;
	delete display;
	delete scene;
	delete device;

	TaskScheduler::exit();
}

void Session::start()
{
	session_thread = new thread(function_bind(&Session::run, this));
}

void Session::start_denoise()
{
	session_thread = new thread(function_bind(&Session::run_denoise, this));
}

bool Session::ready_to_reset()
{
	double dt = time_dt() - reset_time;

	if(!display_outdated)
		return (dt > params.reset_timeout);
	else
		return (dt > params.cancel_timeout);
}

/* GPU Session */

void Session::reset_gpu(BufferParams& buffer_params, int samples)
{
	thread_scoped_lock pause_lock(pause_mutex);

	/* block for buffer access and reset immediately. we can't do this
	 * in the thread, because we need to allocate an OpenGL buffer, and
	 * that only works in the main thread */
	thread_scoped_lock display_lock(display_mutex);
	thread_scoped_lock buffers_lock(buffers_mutex);

	display_outdated = true;
	reset_time = time_dt();

	reset_(buffer_params, samples);

	gpu_need_tonemap = false;
	gpu_need_tonemap_cond.notify_all();

	pause_cond.notify_all();
}

bool Session::draw_gpu(BufferParams& buffer_params, DeviceDrawParams& draw_params)
{
	/* block for buffer access */
	thread_scoped_lock display_lock(display_mutex);

	/* first check we already rendered something */
	if(gpu_draw_ready) {
		/* then verify the buffers have the expected size, so we don't
		 * draw previous results in a resized window */
		if(!buffer_params.modified(display->params)) {
			/* for CUDA we need to do tonemapping still, since we can
			 * only access GL buffers from the main thread */
			if(gpu_need_tonemap) {
				thread_scoped_lock buffers_lock(buffers_mutex);
				tonemap(tile_manager.state.sample);
				gpu_need_tonemap = false;
				gpu_need_tonemap_cond.notify_all();
			}

			display->draw(device, draw_params);

			if(display_outdated && (time_dt() - reset_time) > params.text_timeout)
				return false;

			return true;
		}
	}

	return false;
}

void Session::run_gpu()
{
	bool tiles_written = false;

	start_time = time_dt();
	reset_time = time_dt();
	paused_time = 0.0;
	last_update_time = time_dt();

	progress.set_render_start_time(start_time + paused_time);

	while(!progress.get_cancel()) {
		/* advance to next tile */
		bool no_tiles = !tile_manager.next();

		if(params.background) {
			/* if no work left and in background mode, we can stop immediately */
			if(no_tiles) {
				progress.set_status("Finished");
				break;
			}
		}
		else {
			/* if in interactive mode, and we are either paused or done for now,
			 * wait for pause condition notify to wake up again */
			thread_scoped_lock pause_lock(pause_mutex);

			if(!pause && !tile_manager.done()) {
				/* reset could have happened after no_tiles was set, before this lock.
				 * in this case we shall not wait for pause condition
				 */
			}
			else if(pause || no_tiles) {
				update_status_time(pause, no_tiles);

				while(1) {
					double pause_start = time_dt();
					pause_cond.wait(pause_lock);
					paused_time += time_dt() - pause_start;

					if(!params.background)
						progress.set_start_time(start_time + paused_time);
					progress.set_render_start_time(start_time + paused_time);

					update_status_time(pause, no_tiles);
					progress.set_update();

					if(!pause)
						break;
				}
			}

			if(progress.get_cancel())
				break;
		}

		if(!no_tiles) {
			/* update scene */
			update_scene();

			if(!device->error_message().empty())
				progress.set_error(device->error_message());

			if(progress.get_cancel())
				break;
		}

		if(!no_tiles) {
			/* buffers mutex is locked entirely while rendering each
			 * sample, and released/reacquired on each iteration to allow
			 * reset and draw in between */
			thread_scoped_lock buffers_lock(buffers_mutex);

			/* update status and timing */
			update_status_time();

			/* render */
			render();

			device->task_wait();

			if(!device->error_message().empty())
				progress.set_cancel(device->error_message());

			/* update status and timing */
			update_status_time();

			gpu_need_tonemap = true;
			gpu_draw_ready = true;
			progress.set_update();

			/* wait for tonemap */
			if(!params.background) {
				while(gpu_need_tonemap) {
					if(progress.get_cancel())
						break;

					gpu_need_tonemap_cond.wait(buffers_lock);
				}
			}

			if(!device->error_message().empty())
				progress.set_error(device->error_message());

			tiles_written = update_progressive_refine(progress.get_cancel());

			if(progress.get_cancel())
				break;
		}
	}

	if(!tiles_written)
		update_progressive_refine(true);
}

/* CPU Session */

void Session::reset_cpu(BufferParams& buffer_params, int samples)
{
	thread_scoped_lock reset_lock(delayed_reset.mutex);
	thread_scoped_lock pause_lock(pause_mutex);

	display_outdated = true;
	reset_time = time_dt();

	delayed_reset.params = buffer_params;
	delayed_reset.samples = samples;
	delayed_reset.do_reset = true;
	device->task_cancel();

	pause_cond.notify_all();
}

bool Session::draw_cpu(BufferParams& buffer_params, DeviceDrawParams& draw_params)
{
	thread_scoped_lock display_lock(display_mutex);

	/* first check we already rendered something */
	if(display->draw_ready()) {
		/* then verify the buffers have the expected size, so we don't
		 * draw previous results in a resized window */
		if(!buffer_params.modified(display->params)) {
			display->draw(device, draw_params);

			if(display_outdated && (time_dt() - reset_time) > params.text_timeout)
				return false;

			return true;
		}
	}

	return false;
}

bool Session::acquire_tile(Device *tile_device, RenderTile& rtile)
{
	if(progress.get_cancel()) {
		if(params.progressive_refine == false) {
			/* for progressive refine current sample should be finished for all tiles */
			return false;
		}
	}

	thread_scoped_lock tile_lock(tile_mutex);

	/* get next tile from manager */
	Tile *tile;
	int device_num = device->device_number(tile_device);

	if(!tile_manager.next_tile(tile, device_num))
		return false;
	
	/* fill render tile */
	rtile.x = tile_manager.state.buffer.full_x + tile->x;
	rtile.y = tile_manager.state.buffer.full_y + tile->y;
	rtile.w = tile->w;
	rtile.h = tile->h;
	rtile.start_sample = tile_manager.state.sample;
	rtile.num_samples = tile_manager.state.num_samples;
	rtile.resolution = tile_manager.state.resolution_divider;
	rtile.tile_index = tile->index;
	rtile.task = (tile->state == Tile::DENOISE)? RenderTile::DENOISE: RenderTile::PATH_TRACE;

	int overscan = 0;
	const bool is_gpu = params.device.type == DEVICE_CUDA || params.device.type == DEVICE_OPENCL || getenv("CPU_OVERSCAN");
	if(params.denoise_result && is_gpu) {
		overscan = scene->integrator->half_window;
		rtile.x -= overscan;
		rtile.y -= overscan;
		rtile.w += 2*overscan;
		rtile.h += 2*overscan;
	}

	tile_lock.unlock();

	/* in case of a permanent buffer, return it, otherwise we will allocate
	 * a new temporary buffer */
	if(!(params.background && params.output_path.empty())) {
		tile_manager.state.buffer.get_offset_stride(rtile.offset, rtile.stride);

		rtile.buffer = buffers->buffer.device_pointer;
		rtile.rng_state = buffers->rng_state.device_pointer;
		rtile.buffers = buffers;
		tile->buffers = buffers;

		device->map_tile(tile_device, rtile);

		return true;
	}

	if(tile->buffers == NULL) {
		/* fill buffer parameters */
		BufferParams buffer_params = tile_manager.params;
		buffer_params.full_x = rtile.x;
		buffer_params.full_y = rtile.y;
		buffer_params.width = rtile.w;
		buffer_params.height = rtile.h;
		buffer_params.overscan = overscan;
		buffer_params.final_width = rtile.w - 2*overscan;
		buffer_params.final_height = rtile.h - 2*overscan;

		/* allocate buffers */
		if(params.progressive_refine) {
			tile_lock.lock();

			if(tile_buffers.size() == 0)
				tile_buffers.resize(tile_manager.state.num_tiles, NULL);

			/* In certain circumstances number of tiles in the tile manager could
			 * be changed. This is not supported by the progressive refine feature.
			 */
			assert(tile_buffers.size() == tile_manager.state.num_tiles);

			tile->buffers = tile_buffers[tile->index];
			if(tile->buffers == NULL) {
				tile->buffers = new RenderBuffers(tile_device);
				tile_buffers[tile->index] = tile->buffers;

				tile->buffers->reset(tile_device, buffer_params);
			}

			tile_lock.unlock();
		}
		else {
			tile->buffers = new RenderBuffers(tile_device);

			tile->buffers->reset(tile_device, buffer_params);
		}
	}

	tile->buffers->params.get_offset_stride(rtile.offset, rtile.stride);

	rtile.buffer = tile->buffers->buffer.device_pointer;
	rtile.rng_state = tile->buffers->rng_state.device_pointer;
	rtile.buffers = tile->buffers;

	/* this will tag tile as IN PROGRESS in blender-side render pipeline,
	 * which is needed to highlight currently rendering tile before first
	 * sample was processed for it
	 */
	update_tile_sample(rtile);

	return true;
}

void Session::update_tile_sample(RenderTile& rtile)
{
	thread_scoped_lock tile_lock(tile_mutex);

	if(update_render_tile_cb) {
		if(params.progressive_refine == false) {
			/* todo: optimize this by making it thread safe and removing lock */

			update_render_tile_cb(rtile, true);
		}
	}

	update_status_time();
}

void Session::release_tile(RenderTile& rtile)
{
	thread_scoped_lock tile_lock(tile_mutex);

	bool delete_tile;

	if(tile_manager.return_tile(rtile.tile_index, delete_tile)) {
		if(write_render_tile_cb && params.progressive_refine == false) {
			write_render_tile_cb(rtile);
			if(delete_tile) {
				delete rtile.buffers;
				tile_manager.state.tiles[rtile.tile_index].buffers = NULL;
			}
		}
	}
	else {
		if(update_render_tile_cb && params.progressive_refine == false) {
			update_render_tile_cb(rtile, false);
		}
	}

	update_status_time();
}

void Session::get_neighbor_tiles(RenderTile *tiles)
{
	int center_idx = tiles[4].tile_index;
	assert(tile_manager.state.tiles[center_idx].state == Tile::DENOISE);
	BufferParams buffer_params = tile_manager.params;
	int4 image_region = make_int4(buffer_params.full_x, buffer_params.full_y,
	                              buffer_params.full_x + buffer_params.width, buffer_params.full_y + buffer_params.height);

	for(int dy = -1, i = 0; dy <= 1; dy++) {
		for(int dx = -1; dx <= 1; dx++, i++) {
			int px = tiles[4].x + dx*params.tile_size.x;
			int py = tiles[4].y + dy*params.tile_size.y;
			if(px >= image_region.x && py >= image_region.y &&
			   px <  image_region.z && py <  image_region.w) {
				int tile_index = center_idx + dy*tile_manager.state.tile_stride + dx;
				Tile *tile = &tile_manager.state.tiles[tile_index];
				assert(tile->buffers);

				tiles[i].buffer = tile->buffers->buffer.device_pointer;
				tiles[i].x = tile_manager.state.buffer.full_x + tile->x;
				tiles[i].y = tile_manager.state.buffer.full_y + tile->y;
				tiles[i].w = tile->w;
				tiles[i].h = tile->h;
				tiles[i].buffers = tile->buffers;

				tile->buffers->params.get_offset_stride(tiles[i].offset, tiles[i].stride);
			}
			else {
				tiles[i].buffer = (device_ptr)NULL;
				tiles[i].x = clamp(px, image_region.x, image_region.z);
				tiles[i].y = clamp(py, image_region.y, image_region.w);
				tiles[i].w = tiles[i].h = 0;
			}
		}
	}

	assert(tiles[4].buffers);
}

void Session::run_cpu()
{
	bool tiles_written = false;

	last_update_time = time_dt();

	{
		/* reset once to start */
		thread_scoped_lock reset_lock(delayed_reset.mutex);
		thread_scoped_lock buffers_lock(buffers_mutex);
		thread_scoped_lock display_lock(display_mutex);

		reset_(delayed_reset.params, delayed_reset.samples);
		delayed_reset.do_reset = false;
	}

	while(!progress.get_cancel()) {
		/* advance to next tile */
		bool no_tiles = !tile_manager.next();
		bool need_tonemap = false;

		if(params.background) {
			/* if no work left and in background mode, we can stop immediately */
			if(no_tiles) {
				progress.set_status("Finished");
				break;
			}
		}
		else {
			/* if in interactive mode, and we are either paused or done for now,
			 * wait for pause condition notify to wake up again */
			thread_scoped_lock pause_lock(pause_mutex);

			if(!pause && delayed_reset.do_reset) {
				/* reset once to start */
				thread_scoped_lock reset_lock(delayed_reset.mutex);
				thread_scoped_lock buffers_lock(buffers_mutex);
				thread_scoped_lock display_lock(display_mutex);

				reset_(delayed_reset.params, delayed_reset.samples);
				delayed_reset.do_reset = false;
			}
			else if(pause || no_tiles) {
				update_status_time(pause, no_tiles);

				while(1) {
					double pause_start = time_dt();
					pause_cond.wait(pause_lock);
					paused_time += time_dt() - pause_start;

					if(!params.background)
						progress.set_start_time(start_time + paused_time);
					progress.set_render_start_time(start_time + paused_time);

					update_status_time(pause, no_tiles);
					progress.set_update();

					if(!pause)
						break;
				}
			}

			if(progress.get_cancel())
				break;
		}

		if(!no_tiles) {
			/* buffers mutex is locked entirely while rendering each
			 * sample, and released/reacquired on each iteration to allow
			 * reset and draw in between */
			thread_scoped_lock buffers_lock(buffers_mutex);

			/* update scene */
			update_scene();

			if(!device->error_message().empty())
				progress.set_error(device->error_message());

			if(progress.get_cancel())
				break;

			/* update status and timing */
			update_status_time();

			/* render */
			render();

			/* update status and timing */
			update_status_time();

			if(!params.background)
				need_tonemap = true;

			if(!device->error_message().empty())
				progress.set_error(device->error_message());
		}

		device->task_wait();

		{
			thread_scoped_lock reset_lock(delayed_reset.mutex);
			thread_scoped_lock buffers_lock(buffers_mutex);
			thread_scoped_lock display_lock(display_mutex);

			if(delayed_reset.do_reset) {
				/* reset rendering if request from main thread */
				delayed_reset.do_reset = false;
				reset_(delayed_reset.params, delayed_reset.samples);
			}
			else if(need_tonemap) {
				/* tonemap only if we do not reset, we don't we don't
				 * want to show the result of an incomplete sample */
				tonemap(tile_manager.state.sample);
			}

			if(!device->error_message().empty())
				progress.set_error(device->error_message());

			tiles_written = update_progressive_refine(progress.get_cancel());
		}

		progress.set_update();
	}

	if(!tiles_written)
		update_progressive_refine(true);
}

DeviceRequestedFeatures Session::get_requested_device_features()
{
	/* TODO(sergey): Consider moving this to the Scene level. */
	DeviceRequestedFeatures requested_features;
	requested_features.experimental = params.experimental;

	requested_features.max_closure = get_max_closure_count();
	scene->shader_manager->get_requested_features(
	        scene,
	        &requested_features);
	if(!params.background) {
		/* Avoid too much re-compilations for viewport render. */
		requested_features.max_closure = 64;
		requested_features.max_nodes_group = NODE_GROUP_LEVEL_MAX;
		requested_features.nodes_features = NODE_FEATURE_ALL;
	}

	/* This features are not being tweaked as often as shaders,
	 * so could be done selective magic for the viewport as well.
	 */
	requested_features.use_hair = false;
	requested_features.use_object_motion = false;
	requested_features.use_camera_motion = scene->camera->use_motion;
	foreach(Object *object, scene->objects) {
		Mesh *mesh = object->mesh;
		if(mesh->num_curves()) {
			requested_features.use_hair = true;
		}
		requested_features.use_object_motion |= object->use_motion | mesh->use_motion_blur;
		requested_features.use_camera_motion |= mesh->use_motion_blur;
#ifdef WITH_OPENSUBDIV
		if(mesh->subdivision_type != Mesh::SUBDIVISION_NONE) {
			requested_features.use_patch_evaluation = true;
		}
#endif
	}

	BakeManager *bake_manager = scene->bake_manager;
	requested_features.use_baking = bake_manager->get_baking();
	requested_features.use_integrator_branched = (scene->integrator->method == Integrator::BRANCHED_PATH);

	return requested_features;
}

void Session::load_kernels()
{
	thread_scoped_lock scene_lock(scene->mutex);

	if(!kernels_loaded) {
		progress.set_status("Loading render kernels (may take a few minutes the first time)");

		DeviceRequestedFeatures requested_features = get_requested_device_features();
		VLOG(2) << "Requested features:\n" << requested_features;
		if(!device->load_kernels(requested_features)) {
			string message = device->error_message();
			if(message.empty())
				message = "Failed loading render kernel, see console for errors";

			progress.set_error(message);
			progress.set_status("Error", message);
			progress.set_update();
			return;
		}

		kernels_loaded = true;
	}
}

void Session::run()
{
	/* load kernels */
	load_kernels();

	/* session thread loop */
	progress.set_status("Waiting for render to start");

	/* run */
	if(!progress.get_cancel()) {
		/* reset number of rendered samples */
		progress.reset_sample();

		if(device_use_gl)
			run_gpu();
		else
			run_cpu();
	}

	/* progress update */
	if(progress.get_cancel())
		progress.set_status("Cancel", progress.get_cancel_message());
	else
		progress.set_update();
}

void Session::run_denoise()
{
	if(!progress.get_cancel()) {
		if(!kernels_loaded) {
			progress.set_status("Loading render kernels (may take a few minutes the first time)");

			DeviceRequestedFeatures requested_features;
			if(!device->load_kernels(requested_features)) {
				string message = device->error_message();
				if(message.empty())
					message = "Failed loading render kernel, see console for errors";

				progress.set_error(message);
				progress.set_status("Error", message);
				progress.set_update();
				return;
			}

			kernels_loaded = true;
		}

		progress.reset_sample();
		tile_manager.reset(buffers->params, params.samples);
		tile_manager.state.global_buffers = buffers;
		start_time = time_dt();
		progress.set_render_start_time(start_time);

		/* Set up KernelData. */
		KernelData kernel_data;
		kernel_data.integrator.half_window = params.half_window;
		kernel_data.film.pass_stride = buffers->params.get_passes_size();
		kernel_data.film.pass_denoising = buffers->params.get_denoise_offset();
		kernel_data.film.pass_no_denoising = buffers->params.selective_denoising? kernel_data.film.pass_denoising+20 : 0;
		kernel_data.film.exposure = 1.0f;
		kernel_data.film.num_frames = buffers->params.frames;
		kernel_data.film.prev_frames = params.prev_frames;
		device->const_copy_to("__data", &kernel_data, sizeof(kernel_data));

		/* Generate tiles. */
		tile_manager.next();
		{
			thread_scoped_lock buffers_lock(buffers_mutex);

			if(!device->error_message().empty())
				progress.set_error(device->error_message());

			/* update status and timing */
			update_status_time();

			/* render */
			render();

			/* update status and timing */
			update_status_time();


			if(!device->error_message().empty())
				progress.set_error(device->error_message());
		}

		device->task_wait();

		progress.set_update();
	}

	if(progress.get_cancel())
		progress.set_status("Cancel", progress.get_cancel_message());
	else
		progress.set_update();
}

bool Session::draw(BufferParams& buffer_params, DeviceDrawParams &draw_params)
{
	if(device_use_gl)
		return draw_gpu(buffer_params, draw_params);
	else
		return draw_cpu(buffer_params, draw_params);
}

void Session::reset_(BufferParams& buffer_params, int samples)
{
	if(buffers) {
		if(buffer_params.modified(buffers->params)) {
			gpu_draw_ready = false;
			buffers->reset(device, buffer_params);
			display->reset(device, buffer_params);
		}
	}

	tile_manager.reset(buffer_params, samples);

	start_time = time_dt();
	preview_time = 0.0;
	paused_time = 0.0;

	if(!params.background)
		progress.set_start_time(start_time);
	progress.set_render_start_time(start_time);
}

void Session::reset(BufferParams& buffer_params, int samples)
{
	if(device_use_gl)
		reset_gpu(buffer_params, samples);
	else
		reset_cpu(buffer_params, samples);

	if(params.progressive_refine) {
		thread_scoped_lock buffers_lock(buffers_mutex);

		foreach(RenderBuffers *buffers, tile_buffers)
			delete buffers;

		tile_buffers.clear();
	}
}

void Session::set_samples(int samples)
{
	if(samples != params.samples) {
		params.samples = samples;
		tile_manager.set_samples(samples);

		{
			thread_scoped_lock pause_lock(pause_mutex);
		}
		pause_cond.notify_all();
	}
}

void Session::set_pause(bool pause_)
{
	bool notify = false;

	{
		thread_scoped_lock pause_lock(pause_mutex);

		if(pause != pause_) {
			pause = pause_;
			notify = true;
		}
	}

	if(notify)
		pause_cond.notify_all();
}

void Session::wait()
{
	session_thread->join();
	delete session_thread;

	session_thread = NULL;
}

void Session::update_scene()
{
	thread_scoped_lock scene_lock(scene->mutex);

	/* update camera if dimensions changed for progressive render. the camera
	 * knows nothing about progressive or cropped rendering, it just gets the
	 * image dimensions passed in */
	Camera *cam = scene->camera;
	int width = tile_manager.state.buffer.full_width;
	int height = tile_manager.state.buffer.full_height;
	int resolution = tile_manager.state.resolution_divider;

	if(width != cam->width || height != cam->height) {
		cam->width = width;
		cam->height = height;
		cam->resolution = resolution;
		cam->tag_update();
	}

	/* number of samples is needed by multi jittered
	 * sampling pattern and by baking */
	Integrator *integrator = scene->integrator;
	BakeManager *bake_manager = scene->bake_manager;

	if(integrator->sampling_pattern == SAMPLING_PATTERN_CMJ ||
	   bake_manager->get_baking())
	{
		int aa_samples = tile_manager.num_samples;

		if(aa_samples != integrator->aa_samples) {
			integrator->aa_samples = aa_samples;
			integrator->tag_update(scene);
		}
	}

	/* update scene */
	if(scene->need_update()) {
		progress.set_status("Updating Scene");
		MEM_GUARDED_CALL(&progress, scene->device_update, device, progress);
	}
}

void Session::update_status_time(bool show_pause, bool show_done)
{
	int sample = tile_manager.state.sample;
	int resolution = tile_manager.state.resolution_divider;
	int num_tiles = tile_manager.state.num_tiles;
	int tile = tile_manager.state.num_rendered_tiles;

	/* update status */
	string status, substatus;

	if(!params.progressive) {
		const int progress_sample = progress.get_sample(),
		          num_samples = tile_manager.get_num_effective_samples();
		const bool is_gpu = params.device.type == DEVICE_CUDA || params.device.type == DEVICE_OPENCL;
		const bool is_multidevice = params.device.multi_devices.size() > 1;
		const bool is_cpu = params.device.type == DEVICE_CPU;
		const bool is_last_tile = (num_samples * num_tiles - progress_sample) < num_samples;

		substatus = string_printf("Path Tracing Tile %d/%d", tile, num_tiles);

		if((is_gpu && !is_multidevice && !device->info.use_split_kernel) ||
		   (is_cpu && (num_tiles == 1 || is_last_tile)))
		{
			/* When using split-kernel (OpenCL) each thread in a tile will be working on a different
			 * sample. Can't display sample number when device uses split-kernel
			 */

			/* when rendering on GPU multithreading happens within single tile, as in
			 * tiles are handling sequentially and in this case we could display
			 * currently rendering sample number
			 * this helps a lot from feedback point of view.
			 * also display the info on CPU, when using 1 tile only
			 */

			int status_sample = progress_sample;
			if(tile > 1) {
				/* sample counter is global for all tiles, subtract samples
				 * from already finished tiles to get sample counter for
				 * current tile only
				 */
				if(is_cpu && is_last_tile && num_tiles > 1) {
					status_sample = num_samples - (num_samples * num_tiles - progress_sample);
				}
				else {
					status_sample -= (tile - 1) * num_samples;
				}
			}

			substatus += string_printf(", Sample %d/%d", status_sample, num_samples);
		}
	}
	else if(tile_manager.num_samples == INT_MAX)
		substatus = string_printf("Path Tracing Sample %d", sample+1);
	else
		substatus = string_printf("Path Tracing Sample %d/%d",
		                          sample+1,
		                          tile_manager.get_num_effective_samples());
	
	if(show_pause) {
		status = "Paused";
	}
	else if(show_done) {
		status = "Done";
	}
	else {
		status = substatus;
		substatus.clear();
	}

	progress.set_status(status, substatus);

	/* update timing */
	if(preview_time == 0.0 && resolution == 1)
		preview_time = time_dt();
	
	double tile_time = (tile == 0 || sample == 0)? 0.0: (time_dt() - preview_time - paused_time) / sample;

	/* negative can happen when we pause a bit before rendering, can discard that */
	if(preview_time < 0.0) preview_time = 0.0;

	progress.set_tile(tile, tile_time);
}

void Session::update_progress_sample()
{
	progress.increment_sample();
}

void Session::render()
{
	/* add path trace task */
	DeviceTask task(DeviceTask::RENDER);
	
	task.acquire_tile = function_bind(&Session::acquire_tile, this, _1, _2);
	task.release_tile = function_bind(&Session::release_tile, this, _1);
	task.get_neighbor_tiles = function_bind(&Session::get_neighbor_tiles, this, _1);
	task.get_cancel = function_bind(&Progress::get_cancel, &this->progress);
	task.update_tile_sample = function_bind(&Session::update_tile_sample, this, _1);
	task.update_progress_sample = function_bind(&Session::update_progress_sample, this);
	task.need_finish_queue = params.progressive_refine;
	task.integrator_branched = !params.only_denoise && (scene->integrator->method == Integrator::BRANCHED_PATH);
	task.requested_tile_size = params.tile_size;

	device->task_add(task);
}

void Session::tonemap(int sample)
{
	/* add tonemap task */
	DeviceTask task(DeviceTask::FILM_CONVERT);

	task.x = tile_manager.state.buffer.full_x;
	task.y = tile_manager.state.buffer.full_y;
	task.w = tile_manager.state.buffer.width;
	task.h = tile_manager.state.buffer.height;
	task.rgba_byte = display->rgba_byte.device_pointer;
	task.rgba_half = display->rgba_half.device_pointer;
	task.buffer = buffers->buffer.device_pointer;
	task.sample = sample;
	tile_manager.state.buffer.get_offset_stride(task.offset, task.stride);

	if(task.w > 0 && task.h > 0) {
		device->task_add(task);
		device->task_wait();

		/* set display to new size */
		display->draw_set(task.w, task.h);
	}

	display_outdated = false;
}

bool Session::update_progressive_refine(bool cancel)
{
	int sample = tile_manager.state.sample + 1;
	bool write = sample == tile_manager.num_samples || cancel;

	double current_time = time_dt();

	if(current_time - last_update_time < params.progressive_update_timeout) {
		/* if last sample was processed, we need to write buffers anyway  */
		if(!write && sample != 1)
			return false;
	}

	if(params.progressive_refine) {
		foreach(RenderBuffers *buffers, tile_buffers) {
			RenderTile rtile;
			rtile.buffers = buffers;
			rtile.sample = sample;

			if(write) {
				if(write_render_tile_cb)
					write_render_tile_cb(rtile);
			}
			else {
				if(update_render_tile_cb)
					update_render_tile_cb(rtile, true);
			}
		}
	}

	last_update_time = current_time;

	return write;
}

void Session::device_free()
{
	scene->device_free();

	foreach(RenderBuffers *buffers, tile_buffers)
		delete buffers;
	tile_manager.free_device();

	tile_buffers.clear();

	/* used from background render only, so no need to
	 * re-create render/display buffers here
	 */
}

int Session::get_max_closure_count()
{
	int max_closures = 0;
	for(int i = 0; i < scene->shaders.size(); i++) {
		int num_closures = scene->shaders[i]->graph->get_num_closures();
		max_closures = max(max_closures, num_closures);
	}
	max_closure_global = max(max_closure_global, max_closures);
	return max_closure_global;
}

CCL_NAMESPACE_END
