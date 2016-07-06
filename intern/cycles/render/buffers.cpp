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
	final_width = 0;
	final_height = 0;

	denoising_passes = false;
	selective_denoising = false;
	overscan = 0;

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
		&& final_width == params.final_width
		&& final_height == params.final_height
	        && overscan == params.overscan
		&& Pass::equals(passes, params.passes));
}

int BufferParams::get_passes_size()
{
	int size = 0;

	for(size_t i = 0; i < passes.size(); i++)
		size += passes[i].components;

	if(denoising_passes) {
		/* Feature passes: 7 Channels (3 Color, 3 Normal, 1 Depth) + 7 Variance
		 * Color passes: 3 Noisy (RGB) + 3 Variance [+ 3 Skip (RGB)] */
		size += selective_denoising? 23: 20;
	}

	return align_up(size, 4);
}

int BufferParams::get_denoise_offset()
{
	int offset = 0;

	for(size_t i = 0; i < passes.size(); i++)
		offset += passes[i].components;

	return offset;
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
	
	for(y = 0; y < height; y++)
		for(x = 0; x < width; x++)
			init_state[y*width + x] = hash_int_2d(params.full_x+x, params.full_y+y);

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

/* When calling from the BlenderSession, rect is in final image coordinates.
 * To make addressing the buffer easier, rect is brought to "buffer coordinates"
 * where the buffer starts at (0, 0) and ends at (width, height). */
int4 RenderBuffers::rect_to_local(int4 rect) {
	rect.x -= params.full_x;
	rect.y -= params.full_y;
	rect.z -= params.full_x;
	rect.w -= params.full_y;
	assert(rect.x >= 0 && rect.y >= 0 && rect.z <= params.width && rect.w <= params.height);
	return rect;
}

/* Helper macro that loops over all the pixels in the rect.
 * First, the buffer pointer is shifted to the starting point of the rect.
 * Then, after each line, the buffer pointer is shifted to the start of the next one. */
#define FOREACH_PIXEL in += (rect.y*params.width + rect.x)*pass_stride; \
                      for(int y = rect.y; y < rect.w; y++, in += (params.width + rect.x - rect.z)*pass_stride) \
                          for(int x = rect.x; x < rect.z; x++, in += pass_stride, pixels += components)

bool RenderBuffers::get_denoising_rect(int type, float exposure, int sample, int components, int4 rect, float *pixels, bool read_pixels)
{
	if(!params.denoising_passes)
		/* The RenderBuffer doesn't have denoising passes. */
		return false;
	if(!(type & EX_TYPE_DENOISE_ALL))
		/* The type doesn't correspond to any denoising pass. */
		return false;

	rect = rect_to_local(rect);

	float scale = 1.0f;
	int type_offset = 0;
	switch(type) {
		case EX_TYPE_NONE: assert(0); break;
		case EX_TYPE_DENOISE_NORMAL:     type_offset =  0; scale = 1.0f/sample; break;
		case EX_TYPE_DENOISE_NORMAL_VAR: type_offset =  3; scale = 1.0f/sample; break;
		case EX_TYPE_DENOISE_ALBEDO:     type_offset =  6; scale = 1.0f/sample; break;
		case EX_TYPE_DENOISE_ALBEDO_VAR: type_offset =  9; scale = 1.0f/sample; break;
		case EX_TYPE_DENOISE_DEPTH:      type_offset = 12; scale = 1.0f/sample; break;
		case EX_TYPE_DENOISE_DEPTH_VAR:  type_offset = 13; scale = 1.0f/sample; break;
		case EX_TYPE_DENOISE_NOISY:      type_offset = 14; scale = exposure/sample; break;
		case EX_TYPE_DENOISE_NOISY_VAR:  type_offset = 17; scale = exposure*exposure/sample; break;
		case EX_TYPE_DENOISE_CLEAN:      type_offset = 20; scale = exposure/sample; break;
	}

	if(read_pixels) {
		scale = sample;
	}

	int pass_offset = params.get_denoise_offset() + type_offset;

	float *in = (float*)buffer.data_pointer + pass_offset;
	int pass_stride = params.get_passes_size();

	if(components == 1) {
		assert(type & (EX_TYPE_DENOISE_DEPTH | EX_TYPE_DENOISE_DEPTH_VAR));
		if(read_pixels) {
			FOREACH_PIXEL
				in[0] = pixels[0] * scale;
		}
		else {
			FOREACH_PIXEL
				pixels[0] = in[0] * scale;
		}
	}
	else {
		assert(components == 3);
		assert(!(type & (EX_TYPE_DENOISE_DEPTH | EX_TYPE_DENOISE_DEPTH_VAR)));

		if(read_pixels) {
			FOREACH_PIXEL {
				in[0] = pixels[0] * scale;
				in[1] = pixels[1] * scale;
				in[2] = pixels[2] * scale;
			}
		}
		else {
			FOREACH_PIXEL {
				pixels[0] = in[0] * scale;
				pixels[1] = in[1] * scale;
				pixels[2] = in[2] * scale;
			}
		}
	}

	return true;
}

bool RenderBuffers::get_pass_rect(PassType type, float exposure, int sample, int components, int4 rect, float *pixels, bool read_pixels)
{
	rect = rect_to_local(rect);

	int pass_offset = 0;

	for(size_t j = 0; j < params.passes.size(); j++) {
		Pass& pass = params.passes[j];

		if(pass.type != type) {
			pass_offset += pass.components;
			continue;
		}

		float *in = (float*)buffer.data_pointer + pass_offset;
		int pass_stride = params.get_passes_size();

		float scale = (pass.filter)? 1.0f/(float)sample: 1.0f;
		float scale_exposure = (pass.exposure)? scale*exposure: scale;

		if(read_pixels) {
			scale = scale_exposure = sample;
		}

		if(components == 1) {
			assert(pass.components == components);

			/* scalar */
			if(type == PASS_DEPTH) {
				if(read_pixels) {
					FOREACH_PIXEL
						in[0] = (pixels[0] == 1e10f)? 0.0f: pixels[0]*scale_exposure;
				}
				else {
					FOREACH_PIXEL
						pixels[0] = (in[0] == 0.0f)? 1e10f: in[0]*scale_exposure;
				}
			}
			else if(type == PASS_MIST) {
				if(read_pixels) {
					FOREACH_PIXEL
						in[0] = pixels[0]*scale_exposure;
				}
				else {
					FOREACH_PIXEL
						pixels[0] = saturate(in[0]*scale_exposure);
				}
			}
#ifdef WITH_CYCLES_DEBUG
			else if(type == PASS_BVH_TRAVERSAL_STEPS || type == PASS_RAY_BOUNCES) {
				if(read_pixels) {
					FOREACH_PIXEL
						in[0] = pixels[0]*scale;
				}
				else {
					FOREACH_PIXEL
						pixels[0] = in[0]*scale;
				}
			}
#endif
			else {
				if(read_pixels) {
					FOREACH_PIXEL
						in[0] = pixels[0]*scale_exposure;
				}
				else {
					FOREACH_PIXEL
						pixels[0] = in[0]*scale_exposure;
				}
			}
		}
		else if(components == 3) {
			assert(pass.components == 4);

			/* RGBA */
			if(type == PASS_SHADOW) {
				if(read_pixels) {
					FOREACH_PIXEL {
						in[0] = pixels[0];
						in[1] = pixels[1];
						in[2] = pixels[2];
						in[3] = 1.0f;
					}
				}
				else {
					FOREACH_PIXEL {
						float w = in[3];
						float invw = (w > 0.0f)? 1.0f/w: 1.0f;

						pixels[0] = in[0]*invw;
						pixels[1] = in[1]*invw;
						pixels[2] = in[2]*invw;
					}
				}
			}
			else if(pass.divide_type != PASS_NONE) {
				int divide_offset = -pass_offset;
				/* RGB lighting passes that need to divide out color */
				for(size_t k = 0; k < params.passes.size(); k++) {
					Pass& color_pass = params.passes[k];
					if(color_pass.type == pass.divide_type)
						break;
					divide_offset += color_pass.components;
				}

				if(read_pixels) {
					FOREACH_PIXEL {
						in[0] = pixels[0] * pixels[divide_offset];
						in[1] = pixels[1] * pixels[divide_offset + 1];
						in[2] = pixels[2] * pixels[divide_offset + 2];
					}
				}
				else {
					FOREACH_PIXEL {
						float3 f = make_float3(in[0], in[1], in[2]);
						float3 f_divide = make_float3(in[divide_offset], in[divide_offset+1], in[divide_offset+2]);

						f = safe_divide_even_color(f*exposure, f_divide);

						pixels[0] = f.x;
						pixels[1] = f.y;
						pixels[2] = f.z;
					}
				}
			}
			else {
				/* RGB/vector */
				if(read_pixels) {
					FOREACH_PIXEL {
						in[0] = pixels[0]*scale_exposure;
						in[1] = pixels[1]*scale_exposure;
						in[2] = pixels[2]*scale_exposure;
					}
				}
				else {
					FOREACH_PIXEL {
						pixels[0] = in[0]*scale_exposure;
						pixels[1] = in[1]*scale_exposure;
						pixels[2] = in[2]*scale_exposure;
					}
				}
			}
		}
		else if(components == 4) {
			assert(pass.components == components);

			/* RGBA */
			if(type == PASS_SHADOW) {
				if(read_pixels) {
					FOREACH_PIXEL {
						in[0] = pixels[0];
						in[1] = pixels[1];
						in[2] = pixels[2];
						in[3] = 1.0f;
					}
				}
				else {
					FOREACH_PIXEL {
						float w = in[3];
						float invw = (w > 0.0f)? 1.0f/w: 1.0f;

						pixels[0] = in[0]*invw;
						pixels[1] = in[1]*invw;
						pixels[2] = in[2]*invw;
						pixels[3] = 1.0f;
					}
				}
			}
			else if(type == PASS_MOTION) {
				int weight_offset = -pass_offset;
				/* need to normalize by number of samples accumulated for motion */
				for(size_t k = 0; k < params.passes.size(); k++) {
					Pass& color_pass = params.passes[k];
					if(color_pass.type == PASS_MOTION_WEIGHT)
						break;
					weight_offset += color_pass.components;
				}

				if(read_pixels) {
					FOREACH_PIXEL {
						float w = in[weight_offset];
						in[0] = pixels[0]*w;
						in[1] = pixels[1]*w;
						in[2] = pixels[2]*w;
						in[3] = pixels[3]*w;
					}
				}
				else {
					FOREACH_PIXEL {
						float w = in[weight_offset];
						float invw = (w > 0.0f)? 1.0f/w: 0.0f;

						pixels[0] = in[0]*invw;
						pixels[1] = in[1]*invw;
						pixels[2] = in[2]*invw;
						pixels[3] = in[3]*invw;
					}
				}
			}
			else {
				if(read_pixels) {
					FOREACH_PIXEL {
						in[0] = pixels[0]*scale_exposure;
						in[1] = pixels[1]*scale_exposure;
						in[2] = pixels[2]*scale_exposure;
						in[3] = pixels[3]*scale;
					}
				}
				else {
					FOREACH_PIXEL {
						pixels[0] = in[0]*scale_exposure;
						pixels[1] = in[1]*scale_exposure;
						pixels[2] = in[2]*scale_exposure;

						/* clamp since alpha might be > 1.0 due to russian roulette */
						pixels[3] = saturate(in[3]*scale);
					}
				}
			}
		}
#undef FOREACH_PIXEL

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

