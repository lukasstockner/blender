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
#include <fenv.h>

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
	Pass::add(PASS_SAMPLES, passes);
	Pass::add(PASS_HALF, passes);
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

	resolution = 0;

	offset = 0;
	stride = 0;

	buffer = 0;
	rng_state = 0;

	buffers = NULL;
	num_samples = NULL;

	total_samples = 0;
	max_samples = 0;
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

	/* allocate samples buffer */
	samples.resize(params.width*params.height);
	for(y = 0; y < height; y++)
		for(x = 0; x < width; x++)
			samples[y*width + x] = 0;
}

bool RenderBuffers::copy_from_device()
{
	if(!buffer.device_pointer)
		return false;

	device->mem_copy_from(buffer, 0, params.width, params.height, params.get_passes_size()*sizeof(float));

	return true;
}

void RenderBuffers::set_tile(RenderTile *rtile) {
	rtile->buffer = buffer.device_pointer;
	rtile->rng_state = rng_state.device_pointer;

	rtile->total_samples = 0;
	rtile->max_samples = 0;
	rtile->num_samples = &samples[0];

	for(int y = rtile->y; y < rtile->y+rtile->h; y++) {
		for(int x = rtile->x; x < rtile->x+rtile->w; x++) {
			int val = samples[rtile->offset + y*rtile->stride + x];
			rtile->max_samples = max(rtile->max_samples, val);
			rtile->total_samples += val;
		}
	}
}

void RenderBuffers::set_samples_constant(int num_samples) {
	for(int y = 0; y < params.height; y++)
		for(int x = 0; x < params.width; x++)
			samples[y*params.width + x] = num_samples;
}

void RenderBuffers::set_samples_adaptive(vector<int4> &blocks, int num_samples, float tolerance) {
	feenableexcept(FE_INVALID | FE_OVERFLOW | FE_DIVBYZERO);
	copy_from_device();

	int half_offset = 0, samples_offset = 0;
	foreach(Pass& pass, params.passes) {
		if(pass.type == PASS_HALF)
			break;
		half_offset += pass.components;
	}
	foreach(Pass& pass, params.passes) {
		if(pass.type == PASS_SAMPLES)
			break;
		samples_offset += pass.components;
	}
	int pass_stride = params.get_passes_size();

	if(blocks.empty())
		blocks.push_back(make_int4(params.full_x, params.full_y, params.full_x+params.width, params.full_y+params.height));

	vector<int4> new_blocks;
	for(int i = 0; i < blocks.size(); i++) {
		int4 block = blocks[i];

		bool split_y = (block.w > block.z); /* Along which axis would the block be split? */
		int longer_axis = split_y? block.w: block.z;
		float *marginal = new float[longer_axis];
		for(int i = 0; i < longer_axis; i++)
			marginal[i] = 0.0f;

		/* Offset from block-local coords to buffer-local coords */
		int2 offset = make_int2(block.x - params.full_x, block.y - params.full_y);
		for(int y = 0; y < block.w; y++) {
			for(int x = 0; x < block.z; x++) {
				float *buf = (float*) buffer.data_pointer + ((y + offset.y)*params.width + x + offset.x)*pass_stride;

				if(buf[half_offset + 3] < 4.0f) {
					marginal[split_y? y: x] += 1e30f; /* Not enough half samples yet, force further sampling of this block */
					continue;
				}

				float3 full_c = *((float3*) buf) / *((int*) (buf + samples_offset));
				float3 half_c = *((float3*) (buf + half_offset));
				half_c /= buf[half_offset + 3];
				float err = linear_rgb_to_gray(fabs(full_c-half_c));
				err /= sqrtf(max(1e-4f, linear_rgb_to_gray(full_c)));
				marginal[split_y? y: x] += err;
			}
		}
		for(int i = 1; i < longer_axis; i++)
			marginal[i] += marginal[i-1];
		/* (Error/Ablock)*sqrt(Ablock / Aimg) = Error*sqrt((Ablock / Aimg) / Ablock^2) = Error / sqrt(Aimg*Ablock) */
		float tile_error = marginal[longer_axis-1] / sqrtf((float)block.z*block.w * params.full_width*params.full_height);

		printf("Block at %d %d, size %d %d, has error %f\n", block.x, block.y, block.z, block.w, (double) tile_error);

		if(tile_error < tolerance)
			continue; /* Block is done. */
		else if(tile_error < 256*tolerance && longer_axis > 8) {
			printf("Marginal values are (%d):", longer_axis);
			for(int i = 0; i < longer_axis; i++)
				printf(" %f", (double) marginal[i]);
			int split;
			if(longer_axis == 2)
				split = 1;
			else
				split = std::upper_bound(marginal+1, marginal+longer_axis-1, 0.5f*marginal[longer_axis-1]) - marginal;
			printf(", splitting at %d\n", split);

/*			for(split = 1; split < longer_axis-1; split++)
				if(marginal[split]*2.0f > marginal[longer_axis-1])
					break;*/

			if(split_y) {
				new_blocks.push_back(make_int4(block.x, block.y, block.z, split));
				new_blocks.push_back(make_int4(block.x, block.y+split, block.z, block.w-split));
			}
			else {
				new_blocks.push_back(make_int4(block.x, block.y, split, block.w));
				new_blocks.push_back(make_int4(block.x+split, block.y, block.z-split, block.w));
			}
		}
		else {
			new_blocks.push_back(block);
		}
	}

	blocks.swap(new_blocks);

	for(int i = 0; i < blocks.size(); i++) {
		int4 block = blocks[i];
		/* Offset from block-local coords to buffer-local coords */
		int2 offset = make_int2(block.x - params.full_x, block.y - params.full_y);
		for(int y = 0; y < block.w; y++)
			for(int x = 0; x < block.z; x++)
				samples[(y+offset.y)*params.width + x + offset.x] = num_samples;
	}

	fedisableexcept(FE_INVALID | FE_OVERFLOW | FE_DIVBYZERO);
}

#define get_scale(in) (pass_exposure * (pass.filter? (1.0f / ((int*) in)[samples_pass - pass_offset]): 1.0f))
bool RenderBuffers::get_pass_rect(PassType type, float exposure, int components, float *pixels)
{
	int pass_offset = 0;

	int samples_pass = 0;
	foreach(Pass& pass, params.passes) {
		if(pass.type == PASS_SAMPLES)
			break;
		samples_pass += pass.components;
	}

	foreach(Pass& pass, params.passes) {
		if(pass.type != type) {
			pass_offset += pass.components;
			continue;
		}

		float *in = (float*)buffer.data_pointer + pass_offset;
		int pass_stride = params.get_passes_size();

		float pass_exposure = pass.exposure? exposure: 1.0f;

		int size = params.width*params.height;

		if(components == 1) {
			assert(pass.components == components);

			/* scalar */
			if(type == PASS_DEPTH) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					float f = *in;
					pixels[0] = ((int*) in)[samples_pass - pass_offset];//(f == 0.0f)? 1e10f: f*get_scale(in);
				}
			}
			else if(type == PASS_MIST) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					float f = *in;
					pixels[0] = saturate(f*get_scale(in));
				}
			}
#ifdef WITH_CYCLES_DEBUG
			else if(type == PASS_BVH_TRAVERSAL_STEPS) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					float f = *in;
					pixels[0] = f;
				}
			}
			else if(type == PASS_RAY_BOUNCES) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					float f = *in;
					pixels[0] = f;
				}
			}
#endif
			else {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					float f = *in;
					pixels[0] = f*get_scale(in);
				}
			}
		}
		else if(components == 3) {
			assert(pass.components == 4);

			/* RGBA */
			if(type == PASS_SHADOW) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 3) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);
					float invw = (f.w > 0.0f)? 1.0f/f.w: 1.0f;

					pixels[0] = f.x*invw;
					pixels[1] = f.y*invw;
					pixels[2] = f.z*invw;
				}
			}
			else if(pass.divide_type != PASS_NONE) {
				/* RGB lighting passes that need to divide out color */
				pass_offset = 0;
				foreach(Pass& color_pass, params.passes) {
					if(color_pass.type == pass.divide_type)
						break;
					pass_offset += color_pass.components;
				}

				float *in_divide = (float*)buffer.data_pointer + pass_offset;

				for(int i = 0; i < size; i++, in += pass_stride, in_divide += pass_stride, pixels += 3) {
					float3 f = make_float3(in[0], in[1], in[2]);
					float3 f_divide = make_float3(in_divide[0], in_divide[1], in_divide[2]);

					f = safe_divide_even_color(f*exposure, f_divide);

					pixels[0] = f.x;
					pixels[1] = f.y;
					pixels[2] = f.z;
				}
			}
			else {
				/* RGB/vector */
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 3) {
					float3 f = make_float3(in[0], in[1], in[2]);

					float scale = get_scale(in);
					pixels[0] = f.x*scale;
					pixels[1] = f.y*scale;
					pixels[2] = f.z*scale;
				}
			}
		}
		else if(components == 4) {
			assert(pass.components == components);

			/* RGBA */
			if(type == PASS_SHADOW) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);
					float invw = (f.w > 0.0f)? 1.0f/f.w: 1.0f;

					pixels[0] = f.x*invw;
					pixels[1] = f.y*invw;
					pixels[2] = f.z*invw;
					pixels[3] = 1.0f;
				}
			}
			else if(type == PASS_MOTION) {
				/* need to normalize by number of samples accumulated for motion */
				pass_offset = 0;
				foreach(Pass& color_pass, params.passes) {
					if(color_pass.type == PASS_MOTION_WEIGHT)
						break;
					pass_offset += color_pass.components;
				}

				float *in_weight = (float*)buffer.data_pointer + pass_offset;

				for(int i = 0; i < size; i++, in += pass_stride, in_weight += pass_stride, pixels += 4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);
					float w = in_weight[0];
					float invw = (w > 0.0f)? 1.0f/w: 0.0f;

					pixels[0] = f.x*invw;
					pixels[1] = f.y*invw;
					pixels[2] = f.z*invw;
					pixels[3] = f.w*invw;
				}
			}
			else {
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);

					float scale = get_scale(in);
					pixels[0] = f.x*scale;
					pixels[1] = f.y*scale;
					pixels[2] = f.z*scale;

					/* clamp since alpha might be > 1.0 due to russian roulette */
					pixels[3] = saturate(f.w*scale/pass_exposure);
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

