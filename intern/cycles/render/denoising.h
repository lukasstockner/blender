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

#ifndef __DENOISING_H__
#define __DENOISING_H__

#include "buffers.h"
#include "device.h"
#include "tile.h"

#include "util_progress.h"
#include "util_stats.h"
#include "util_thread.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class BufferParams;
class Device;
class Progress;
class RenderBuffers;

/* DenoisingSession Parameters */

class DenoisingSessionParams {
public:
	DeviceInfo device;

	int2 tile_size;
	TileOrder tile_order;
	int threads;

	SessionParams()
	{
		tile_size = make_int2(64, 64);
		threads = 0;

		tile_order = TILE_CENTER;
	}

	bool modified(const SessionParams& params)
	{ return !(device.type == params.device.type
		&& device.id == params.device.id
		&& tile_size == params.tile_size
		&& threads == params.threads
		&& tile_order == params.tile_order); }

};

/* DenoisingSession
 *
 * This is the class that contains the session thread, running the denoising
 * control loop and dispatching tasks. */

class DenoisingSession {
public:
	Device *device;
	RenderBuffers *buffers;
	Progress progress;
	DenoisingSessionParams params;
	TileManager tile_manager;
	Stats stats;

	function<void(RenderTile&)> write_render_tile_cb;
	function<void(RenderTile&, bool)> update_render_tile_cb;

	explicit DenoisingSession(const SessionParams& params);
	~DenoisingSession();

	void start();
	void wait();

	void load_kernels();

	void device_free();

protected:
	void run();

	void update_status_time(bool show_pause = false, bool show_done = false);

	void tonemap(int sample);
	void render();

	bool acquire_tile(Device *tile_device, RenderTile& tile);
	void update_tile_sample(RenderTile& tile);
	void release_tile(RenderTile& tile);
	void get_neighbor_tiles(RenderTile *tiles);

	void update_progress_sample();

	thread *session_thread;

	thread_mutex tile_mutex;
	thread_mutex buffers_mutex;

	bool kernels_loaded;

	double start_time;

	/* progressive refine */
	double last_update_time;
	bool update_progressive_refine(bool cancel);
};

CCL_NAMESPACE_END

#endif /* __SESSION_H__ */

