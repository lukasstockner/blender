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

#include <stdlib.h>

#include "buffers.h"
#include "device.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_hash.h"
#include "util_image.h"
#include "util_math.h"
#include "util_opengl.h"
#include "util_time.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* Buffer Params */

BufferParams::BufferParams()
{
	width = 0;
	height = 0;

	full_x = 0;
	full_y = 0;
	full_width = 0;
	full_height = 0;

	Pass::add(PASS_COMBINED, passes);
}

void BufferParams::get_offset_stride(int& offset, int& stride)
{
	offset = -(full_x + full_y*width);
	stride = width;
}

bool BufferParams::modified(const BufferParams& params)
{
	return !(full_x == params.full_x
		&& full_y == params.full_y
		&& width == params.width
		&& height == params.height
		&& full_width == params.full_width
		&& full_height == params.full_height
		&& Pass::equals(passes, params.passes));
}

int BufferParams::get_passes_size()
{
	int size = 0;

	foreach(Pass& pass, passes)
		size += pass.components;
	
	return align_up(size, 4);
}

/* Render Buffer Task */

RenderTile::RenderTile()
{
	x = 0;
	y = 0;
	w = 0;
	h = 0;

	sample = 0;
	start_sample = 0;
	num_samples = 0;
	resolution = 0;

	offset = 0;
	stride = 0;

	buffer = 0;
	rng_state = 0;

	buffers = NULL;
}

/* Render Buffers */

RenderBuffers::RenderBuffers(Device *device_)
{
	device = device_;
}

RenderBuffers::~RenderBuffers()
{
	device_free();
}

void RenderBuffers::device_free()
{
	if(buffer.device_pointer) {
		device->mem_free(buffer);
		buffer.clear();
	}

	if(rng_state.device_pointer) {
		device->mem_free(rng_state);
		rng_state.clear();
	}
}

void RenderBuffers::reset(Device *device, BufferParams& params_)
{
	params = params_;

	/* free existing buffers */
	device_free();
	
	/* allocate buffer */
	buffer.resize(params.width*params.height*params.get_passes_size());
	device->mem_alloc(buffer, MEM_READ_WRITE);
	device->mem_zero(buffer);

	/* allocate rng state */
	rng_state.resize(params.width, params.height);

	uint *init_state = rng_state.resize(params.width, params.height);
	int x, y, width = params.width, height = params.height;
	
	for(x = 0; x < width; x++)
		for(y = 0; y < height; y++)
			init_state[x + y*width] = hash_int_2d(params.full_x+x, params.full_y+y);

	device->mem_alloc(rng_state, MEM_READ_WRITE);
	device->mem_copy_to(rng_state);
}

bool RenderBuffers::copy_from_device()
{
	if(!buffer.device_pointer)
		return false;

	device->mem_copy_from(buffer, 0, params.width, params.height, params.get_passes_size()*sizeof(float));

	return true;
}

bool RenderBuffers::copy_to_device()
{
	if(!buffer.device_pointer)
		return false;

	device->mem_copy_to(buffer);

	return true;
}

bool RenderBuffers::get_pass_rect(PassType type, float exposure, int sample, int components, float *pixels)
{
	int oS = 0;
	foreach(Pass& pass, params.passes) {
		if(pass.type == PASS_MIST)
			break;
		oS += pass.components;
	}

	int pass_offset = 0;
	foreach(Pass& pass, params.passes) {
		if(pass.type != type) {
			pass_offset += pass.components;
			continue;
		}

		float *in = (float*)buffer.data_pointer + pass_offset;
		int pass_stride = params.get_passes_size();

		int size = params.width*params.height;

		if(components == 1) {
			assert(pass.components == components);

			/* scalar */
			if(type == PASS_MATERIAL_ID || type == PASS_MIST) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					pixels[0] = *in;
				}
			}
			else {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					pixels[0] = *in / in[oS-pass_offset];
				}
			}
		}
		else if(components == 3) {
			assert(pass.components == 4);

			/* RGBA */
			{
				/* RGB/vector */
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 3) {
					float3 f = make_float3(in[0], in[1], in[2]);

					float scale = 1.0f / in[oS-pass_offset];
					pixels[0] = f.x*scale;
					pixels[1] = f.y*scale;
					pixels[2] = f.z*scale;
				}
			}
		}
		else if(components == 4) {
			assert(pass.components == components);

			/* RGBA */
			if(type == PASS_COMBINED) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 4) {
					pixels[0] = in[0];
					pixels[1] = in[1];
					pixels[2] = in[2];

					/* clamp since alpha might be > 1.0 due to russian roulette */
					pixels[3] = saturate(in[3]);
				}
			} else {
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);

					float scale = 1.0f / in[oS-pass_offset];
					pixels[0] = f.x*scale;
					pixels[1] = f.y*scale;
					pixels[2] = f.z*scale;

					/* clamp since alpha might be > 1.0 due to russian roulette */
					pixels[3] = saturate(f.w*scale);
				}
			}
		}

		return true;
	}

	return false;
}

/* Display Buffer */

DisplayBuffer::DisplayBuffer(Device *device_, bool linear)
{
	device = device_;
	draw_width = 0;
	draw_height = 0;
	transparent = true; /* todo: determine from background */
	half_float = linear;
}

DisplayBuffer::~DisplayBuffer()
{
	device_free();
}

void DisplayBuffer::device_free()
{
	if(rgba_byte.device_pointer) {
		device->pixels_free(rgba_byte);
		rgba_byte.clear();
	}
	if(rgba_half.device_pointer) {
		device->pixels_free(rgba_half);
		rgba_half.clear();
	}
}

void DisplayBuffer::reset(Device *device, BufferParams& params_)
{
	draw_width = 0;
	draw_height = 0;

	params = params_;

	/* free existing buffers */
	device_free();

	/* allocate display pixels */
	if(half_float) {
		rgba_half.resize(params.width, params.height);
		device->pixels_alloc(rgba_half);
	}
	else {
		rgba_byte.resize(params.width, params.height);
		device->pixels_alloc(rgba_byte);
	}
}

void DisplayBuffer::draw_set(int width, int height)
{
	assert(width <= params.width && height <= params.height);

	draw_width = width;
	draw_height = height;
}

void DisplayBuffer::draw(Device *device, const DeviceDrawParams& draw_params)
{
	if(draw_width != 0 && draw_height != 0) {
		device_memory& rgba = rgba_data();

		device->draw_pixels(rgba, 0, draw_width, draw_height, params.full_x, params.full_y, params.width, params.height, transparent, draw_params);
	}
}

bool DisplayBuffer::draw_ready()
{
	return (draw_width != 0 && draw_height != 0);
}

void DisplayBuffer::write(Device *device, const string& filename)
{
	int w = draw_width;
	int h = draw_height;

	if(w == 0 || h == 0)
		return;
	
	if(half_float)
		return;

	/* read buffer from device */
	device_memory& rgba = rgba_data();
	device->pixels_copy_from(rgba, 0, w, h);

	/* write image */
	ImageOutput *out = ImageOutput::create(filename);
	ImageSpec spec(w, h, 4, TypeDesc::UINT8);
	int scanlinesize = w*4*sizeof(uchar);

	out->open(filename, spec);

	/* conversion for different top/bottom convention */
	out->write_image(TypeDesc::UINT8,
		(uchar*)rgba.data_pointer + (h-1)*scanlinesize,
		AutoStride,
		-scanlinesize,
		AutoStride);

	out->close();

	delete out;
}

device_memory& DisplayBuffer::rgba_data()
{
	if(half_float)
		return rgba_half;
	else
		return rgba_byte;
}

CCL_NAMESPACE_END

