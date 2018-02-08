/*
 * Copyright 2011-2017 Blender Foundation
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

#include "device/device_denoising.h"

#include "kernel/filter/filter_defines.h"

#include "util/util_time.h"

CCL_NAMESPACE_BEGIN

DenoisingTask::DenoisingTask(Device *device)
: tiles_mem(device, "denoising tiles_mem", MEM_READ_WRITE),
  storage(device),
  buffer(device),
  device(device)
{
}

DenoisingTask::~DenoisingTask()
{
	storage.XtWX.free();
	storage.XtWY.free();
	storage.transform.free();
	storage.rank.free();
	storage.temporary_1.free();
	storage.temporary_2.free();
	storage.temporary_color.free();
	buffer.mem.free();
	tiles_mem.free();
}

void DenoisingTask::init_from_devicetask(const DeviceTask &task)
{
	radius = task.denoising_radius;
	nlm_k_2 = powf(2.0f, lerp(-5.0f, 3.0f, task.denoising_strength));
	if(task.denoising_relative_pca) {
		pca_threshold = -powf(10.0f, lerp(-8.0f, 0.0f, task.denoising_feature_strength));
	}
	else {
		pca_threshold = powf(10.0f, lerp(-5.0f, 3.0f, task.denoising_feature_strength));
	}

	render_buffer.pass_stride = task.pass_stride;
	render_buffer.denoising_data_offset  = task.pass_denoising_data;

	target_buffer.pass_stride = task.target_pass_stride;
	target_buffer.denoising_clean_offset = task.pass_denoising_clean;

	/* Expand filter_area by radius pixels and clamp the result to the extent of the neighboring tiles */
	rect = rect_from_shape(filter_area.x, filter_area.y, filter_area.z, filter_area.w);
	rect = rect_expand(rect, radius);
	rect = rect_clip(rect, make_int4(tiles->x[0], tiles->y[0], tiles->x[3], tiles->y[3]));
}

void DenoisingTask::tiles_from_rendertiles(RenderTile *rtiles)
{
	tiles = (TilesInfo*) tiles_mem.alloc(sizeof(TilesInfo)/sizeof(int));

	device_ptr buffers[9];
	for(int i = 0; i < 9; i++) {
		buffers[i] = rtiles[i].buffer;
		tiles->offsets[i] = rtiles[i].offset;
		tiles->strides[i] = rtiles[i].stride;
	}
	tiles->x[0] = rtiles[3].x;
	tiles->x[1] = rtiles[4].x;
	tiles->x[2] = rtiles[5].x;
	tiles->x[3] = rtiles[5].x + rtiles[5].w;
	tiles->y[0] = rtiles[1].y;
	tiles->y[1] = rtiles[4].y;
	tiles->y[2] = rtiles[7].y;
	tiles->y[3] = rtiles[7].y + rtiles[7].h;

	target_buffer.offset = rtiles[9].offset;
	target_buffer.stride = rtiles[9].stride;
	target_buffer.ptr    = rtiles[9].buffer;

	functions.set_tiles(buffers);
}

bool DenoisingTask::run_denoising()
{
	bool is_baking = (render_buffer.denoising_data_offset == 0);
	bool baking_has_albedo = is_baking && (render_buffer.pass_stride == 31);

	/* Allocate denoising buffer. */
	buffer.passes = baking_has_albedo? 17 : 14;
	buffer.width = rect.z - rect.x;
	buffer.stride = align_up(buffer.width, 4);
	buffer.h = rect.w - rect.y;
	buffer.pass_stride = align_up(buffer.stride * buffer.h, divide_up(device->mem_sub_ptr_alignment(), sizeof(float)));
	buffer.mem.alloc_to_device(buffer.pass_stride * buffer.passes, false);
	buffer.mode = is_baking? (baking_has_albedo? FEATURE_MODE_BAKING_ALBEDO : FEATURE_MODE_BAKING) : FEATURE_MODE_RENDER;

	device_ptr null_ptr = (device_ptr) 0;

	/* Prefilter shadow feature. */
	{
		device_sub_ptr unfiltered_a   (buffer.mem, 0,                    buffer.pass_stride);
		device_sub_ptr unfiltered_b   (buffer.mem, 1*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr sample_var     (buffer.mem, 2*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr sample_var_var (buffer.mem, 3*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr buffer_var     (buffer.mem, 5*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr filtered_var   (buffer.mem, 6*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr nlm_temporary_1(buffer.mem, 7*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr nlm_temporary_2(buffer.mem, 8*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr nlm_temporary_3(buffer.mem, 9*buffer.pass_stride, buffer.pass_stride);

		nlm_state.temporary_1_ptr = *nlm_temporary_1;
		nlm_state.temporary_2_ptr = *nlm_temporary_2;
		nlm_state.temporary_3_ptr = *nlm_temporary_3;

		/* Get the A/B unfiltered passes, the combined sample variance, the estimated variance of the sample variance and the buffer variance. */
		functions.divide_shadow(*unfiltered_a, *unfiltered_b, *sample_var, *sample_var_var, *buffer_var);

		/* Smooth the (generally pretty noisy) buffer variance using the spatial information from the sample variance. */
		nlm_state.set_parameters(6, 3, 4.0f, 1.0f);
		functions.non_local_means(*buffer_var, *sample_var, *sample_var_var, *filtered_var);

		/* Reuse memory, the previous data isn't needed anymore. */
		device_ptr filtered_a = *buffer_var,
		           filtered_b = *sample_var;
		/* Use the smoothed variance to filter the two shadow half images using each other for weight calculation. */
		nlm_state.set_parameters(5, 3, 1.0f, 0.25f);
		functions.non_local_means(*unfiltered_a, *unfiltered_b, *filtered_var, filtered_a);
		functions.non_local_means(*unfiltered_b, *unfiltered_a, *filtered_var, filtered_b);

		device_ptr residual_var = *sample_var_var;
		/* Estimate the residual variance between the two filtered halves. */
		functions.combine_halves(filtered_a, filtered_b, null_ptr, residual_var, 2, rect);

		device_ptr final_a = *unfiltered_a,
		           final_b = *unfiltered_b;
		/* Use the residual variance for a second filter pass. */
		nlm_state.set_parameters(4, 2, 1.0f, 0.5f);
		functions.non_local_means(filtered_a, filtered_b, residual_var, final_a);
		functions.non_local_means(filtered_b, filtered_a, residual_var, final_b);

		/* Combine the two double-filtered halves to a final shadow feature. */
		device_sub_ptr shadow_pass(buffer.mem, (is_baking? 6 : 4)*buffer.pass_stride, buffer.pass_stride);
		functions.combine_halves(final_a, final_b, *shadow_pass, null_ptr, 0, rect);
	}

	/* Prefilter general features. */
	{
		device_sub_ptr unfiltered     (buffer.mem,  8*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr variance       (buffer.mem,  9*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr nlm_temporary_1(buffer.mem, 10*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr nlm_temporary_2(buffer.mem, 11*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr nlm_temporary_3(buffer.mem, 12*buffer.pass_stride, buffer.pass_stride);

		nlm_state.temporary_1_ptr = *nlm_temporary_1;
		nlm_state.temporary_2_ptr = *nlm_temporary_2;
		nlm_state.temporary_3_ptr = *nlm_temporary_3;

		/*                     <--- Regular Render ---><---    Baking   ---><BakeAlbedo> */
		int mean_from[]     = { 0, 1, 2, 12, 6,  7, 8 , 0, 1, 2, 6,  7,  8, 25, 26, 27};
		int variance_from[] = { 3, 4, 5, 13, 9, 10, 11, 3, 4, 5, 9, 10, 11, 28, 29, 30};
		int pass_to[]       = { 1, 2, 3, 0,  5,  6,  7, 0, 1, 2, 3,  4,  5, 14, 15, 16};
		for(int pass = is_baking? 7 : 0; pass < (is_baking? (baking_has_albedo? 16 : 13) : 7); pass++) {
			device_sub_ptr feature_pass(buffer.mem, pass_to[pass]*buffer.pass_stride, buffer.pass_stride);
			/* Get the unfiltered pass and its variance from the RenderBuffers. */
			functions.get_feature(mean_from[pass], variance_from[pass], *unfiltered, *variance);
			/* Smooth the pass and store the result in the denoising buffers. */
			nlm_state.set_parameters(2, 2, 1.0f, 0.25f);
			functions.non_local_means(*unfiltered, *unfiltered, *variance, *feature_pass);
		}
	}

	/* Copy color passes. */
	{
		/*                     <- Render -><- Bake -> */
		int mean_from[]     = {20, 21, 22, 18, 19, 20};
		int variance_from[] = {23, 24, 25, 21, 22, 23};
		int mean_to[]       = { 8,  9, 10,  7,  8,  9};
		int variance_to[]   = {11, 12, 13, 10, 11, 12};

		storage.temporary_color.alloc_to_device(3*buffer.pass_stride, false);

		for(int pass = is_baking? 3 : 0, comp = 0; pass < (is_baking? 6 : 3); pass++, comp++) {
			device_sub_ptr color_pass(storage.temporary_color, comp*buffer.pass_stride, buffer.pass_stride);
			device_sub_ptr color_var_pass(buffer.mem, variance_to[pass]*buffer.pass_stride, buffer.pass_stride);
			functions.get_feature(mean_from[pass], variance_from[pass], *color_pass, *color_var_pass);
		}

		{
			device_sub_ptr shadowing_pass(buffer.mem, (is_baking? 6 : 4)*buffer.pass_stride,   buffer.pass_stride);
			device_sub_ptr color_var_pass(buffer.mem,     variance_to[is_baking? 3 : 0]*buffer.pass_stride, 3*buffer.pass_stride);
			device_sub_ptr output_pass   (buffer.mem,         mean_to[is_baking? 3 : 0]*buffer.pass_stride, 3*buffer.pass_stride);
			functions.detect_outliers(storage.temporary_color.device_pointer, *color_var_pass, *shadowing_pass, *output_pass);
		}
	}

	if(is_baking) {
		device_sub_ptr radius_pass(buffer.mem, 13*buffer.pass_stride, buffer.pass_stride);
		functions.get_feature(24, -1, *radius_pass, null_ptr);
	}

	storage.w = filter_area.z;
	storage.h = filter_area.w;
	storage.transform.alloc_to_device(storage.w*storage.h*TRANSFORM_SIZE, false);
	storage.rank.alloc_to_device(storage.w*storage.h, false);

	functions.construct_transform();

	device_only_memory<float> temporary_1(device, "Denoising NLM temporary 1");
	device_only_memory<float> temporary_2(device, "Denoising NLM temporary 2");
	temporary_1.alloc_to_device(buffer.pass_stride, false);
	temporary_2.alloc_to_device(buffer.pass_stride, false);
	reconstruction_state.temporary_1_ptr = temporary_1.device_pointer;
	reconstruction_state.temporary_2_ptr = temporary_2.device_pointer;

	storage.XtWX.alloc_to_device(storage.w*storage.h*XTWX_SIZE, false);
	storage.XtWY.alloc_to_device(storage.w*storage.h*XTWY_SIZE, false);

	reconstruction_state.filter_window = rect_from_shape(filter_area.x-rect.x, filter_area.y-rect.y, storage.w, storage.h);
	int tile_coordinate_offset = filter_area.y*target_buffer.stride + filter_area.x;
	reconstruction_state.buffer_params = make_int4(target_buffer.offset + tile_coordinate_offset,
	                                               target_buffer.stride,
	                                               target_buffer.pass_stride,
	                                               is_baking? -1 : target_buffer.denoising_clean_offset);
	reconstruction_state.source_w = rect.z-rect.x;
	reconstruction_state.source_h = rect.w-rect.y;

	{
		device_sub_ptr color_ptr    (buffer.mem, (is_baking?  7 :  8)*buffer.pass_stride, 3*buffer.pass_stride);
		device_sub_ptr color_var_ptr(buffer.mem, (is_baking? 10 : 11)*buffer.pass_stride, 3*buffer.pass_stride);
		functions.reconstruct(*color_ptr, *color_var_ptr, target_buffer.ptr);
	}

	return true;
}

CCL_NAMESPACE_END
