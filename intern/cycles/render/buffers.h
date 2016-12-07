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

#ifndef __BUFFERS_H__
#define __BUFFERS_H__

#include "device_memory.h"

#include "film.h"

#include "kernel_types.h"

#include "util_half.h"
#include "util_string.h"
#include "util_thread.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

typedef enum DenoiseExtendedTypes {
	EX_TYPE_NONE                      = 0,
	EX_TYPE_DENOISE_NORMAL            = (1 << 0),
	EX_TYPE_DENOISE_NORMAL_VAR        = (1 << 1),
	EX_TYPE_DENOISE_ALBEDO            = (1 << 2),
	EX_TYPE_DENOISE_ALBEDO_VAR        = (1 << 3),
	EX_TYPE_DENOISE_DEPTH             = (1 << 4),
	EX_TYPE_DENOISE_DEPTH_VAR         = (1 << 5),
	EX_TYPE_DENOISE_SHADOW_A          = (1 << 6),
	EX_TYPE_DENOISE_SHADOW_B          = (1 << 7),
	EX_TYPE_DENOISE_NOISY             = (1 << 8),
	EX_TYPE_DENOISE_NOISY_VAR         = (1 << 9),
	EX_TYPE_DENOISE_CLEAN             = (1 << 10),
	EX_TYPE_DENOISE_NOISY_B           = (1 << 11),
	EX_TYPE_DENOISE_NOISY_B_VAR       = (1 << 12),

	EX_TYPE_DENOISE_REQUIRED = (EX_TYPE_DENOISE_NORMAL
	                          | EX_TYPE_DENOISE_NORMAL_VAR
	                          | EX_TYPE_DENOISE_ALBEDO
	                          | EX_TYPE_DENOISE_ALBEDO_VAR
	                          | EX_TYPE_DENOISE_DEPTH
	                          | EX_TYPE_DENOISE_DEPTH_VAR
	                          | EX_TYPE_DENOISE_SHADOW_A
	                          | EX_TYPE_DENOISE_SHADOW_B
	                          | EX_TYPE_DENOISE_NOISY
	                          | EX_TYPE_DENOISE_NOISY_VAR),
	EX_TYPE_DENOISE_ALL = EX_TYPE_DENOISE_REQUIRED | EX_TYPE_DENOISE_CLEAN | EX_TYPE_DENOISE_NOISY_B | EX_TYPE_DENOISE_NOISY_B_VAR,
} DenoiseExtendedTypes;

typedef enum LightGroupExtendedTypes {
	EX_TYPE_LIGHT_GROUP_1             = (1 << 13),
	EX_TYPE_LIGHT_GROUP_2             = (1 << 14),
	EX_TYPE_LIGHT_GROUP_3             = (1 << 15),
	EX_TYPE_LIGHT_GROUP_4             = (1 << 16),
	EX_TYPE_LIGHT_GROUP_5             = (1 << 17),
	EX_TYPE_LIGHT_GROUP_6             = (1 << 18),
	EX_TYPE_LIGHT_GROUP_7             = (1 << 19),
	EX_TYPE_LIGHT_GROUP_8             = (1 << 20),
} LightGroupExtendedTypes;

class Device;
struct DeviceDrawParams;
struct float4;

/* Buffer Parameters
 * Size of render buffer and how it fits in the full image (border render). */

class BufferParams {
public:
	/* width/height of the physical buffer */
	int width;
	int height;

	/* number of frames stored in this buffer (used for standalone denoising) */
	int frames;

	/* offset into and width/height of the full buffer */
	int full_x;
	int full_y;
	int full_width;
	int full_height;

	/* the width/height of the part that will be visible (might be smaller due to overscan). */
	int final_width;
	int final_height;

	/* passes */
	array<Pass> passes;
	bool denoising_passes;
	/* If only some light path types should be denoised, an additional pass is needed. */
	bool selective_denoising;
	/* Generate an additional pass containing only every second sample. */
	bool cross_denoising;
	/* On GPUs, tiles are extended in each direction to include all the info required for denoising. */
	int overscan;

	int light_groups;
	int num_light_groups;

	/* functions */
	BufferParams();

	void get_offset_stride(int& offset, int& stride);
	bool modified(const BufferParams& params);
	void add_pass(PassType type);
	int get_passes_size();
	int get_denoise_offset();
	int get_light_groups_offset();
};

/* Render Buffers */

class RenderBuffers {
public:
	/* buffer parameters */
	BufferParams params;

	/* float buffer */
	device_vector<float> buffer;
	/* random number generator state */
	device_vector<uint> rng_state;

	explicit RenderBuffers(Device *device);
	~RenderBuffers();

	void reset(Device *device, BufferParams& params);

	bool copy_from_device();
	bool copy_to_device();
	bool get_pass_rect(PassType type, float exposure, int sample, int components, int4 rect, float *pixels, bool read_pixels = false, int frame = 0);
	bool get_denoising_rect(int denoising_pass, float exposure, int sample, int components, int4 rect, float *pixels, bool read_pixels = false, int frame = 0);

protected:
	void device_free();
	int4 rect_to_local(int4 rect);

	Device *device;
};

/* Display Buffer
 *
 * The buffer used for drawing during render, filled by converting the render
 * buffers to byte of half float storage */

class DisplayBuffer {
public:
	/* buffer parameters */
	BufferParams params;
	/* dimensions for how much of the buffer is actually ready for display.
	 * with progressive render we can be using only a subset of the buffer.
	 * if these are zero, it means nothing can be drawn yet */
	int draw_width, draw_height;
	/* draw alpha channel? */
	bool transparent;
	/* use half float? */
	bool half_float;
	/* byte buffer for converted result */
	device_vector<uchar4> rgba_byte;
	device_vector<half4> rgba_half;
	/* flip the image while writing? */
	bool flip_image;

	DisplayBuffer(Device *device, bool linear = false);
	~DisplayBuffer();

	void reset(Device *device, BufferParams& params);
	void write(Device *device, const string& filename);

	void draw_set(int width, int height);
	void draw(Device *device, const DeviceDrawParams& draw_params);
	bool draw_ready();

	device_memory& rgba_data();

protected:
	void device_free();

	Device *device;
};

/* Render Tile
 * Rendering task on a buffer */

class RenderTile {
public:
	typedef enum { PATH_TRACE, DENOISE } Task;

	Task task;
	int x, y, w, h;
	int start_sample;
	int num_samples;
	int sample;
	int resolution;
	int offset;
	int stride;
	int tile_index;

	device_ptr buffer;
	device_ptr rng_state;

	RenderBuffers *buffers;

	RenderTile();
};

CCL_NAMESPACE_END

#endif /* __BUFFERS_H__ */

