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

void DenoisingTiming::add(const DenoisingTiming *other)
{
	/* REALLY hacky, should do this properly somehow. */
	static_assert(sizeof(DenoisingTiming) == 19*sizeof(double), "Number of denoising timings incorrect");
	for(int i = 0; i < 19; i++) {
		((double*) this)[i] += ((const double*) other)[i];
	}
}

void DenoisingTiming::print(string prefix)
{
	const char *p = prefix.c_str();
	printf("%sDenoising:           %f sec\n", p, denoising);
	printf("%sPrefiltering:        %f sec\n", p, prefiltering);
	printf("%sFiltering:           %f sec\n", p, filtering);
	printf("%sShadowing:           %f sec\n", p, shadowing);
	printf("%s  Dividing:          %f sec\n", p, shadowing_divide);
	printf("%s  Smooth Variance:   %f sec\n", p, shadowing_smooth_variance);
	printf("%s  First Cross Pass:  %f sec\n", p, shadowing_first_cross);
	printf("%s  Estimate Residual: %f sec\n", p, shadowing_estimate_residual);
	printf("%s  Second Cross Pass: %f sec\n", p, shadowing_second_cross);
	printf("%s  Combining:         %f sec\n", p, shadowing_combining);
	printf("%sPrefilter Features:  %f sec\n", p, prefilter_features);
	printf("%sPrefilter Color:     %f sec\n", p, prefilter_color);
	printf("%s  Outlier Detection: %f sec\n", p, outlier_detection);
	printf("%sWrite Buffer:        %f sec\n", p, write_buffer);
	printf("%sLoad Buffer:         %f sec\n", p, load_buffer);
	printf("%sConstruct Transform: %f sec\n", p, transform);
	printf("%sReconstruct:         %f sec\n", p, reconstruct);
	printf("%s  Accumulate:        %f sec\n", p, reconstruct_accumulate);
	printf("%s  Solve:             %f sec\n", p, reconstruct_solve);
}

DenoisingTask::DenoisingTask(Device *device, const DeviceTask &task)
: tiles_mem(device, "denoising tiles_mem", MEM_READ_WRITE),
  storage(device),
  buffer(device),
  device(device)
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
	render_buffer.frame_stride = task.denoising_frame_stride;
	render_buffer.offset = task.pass_denoising_data;
	render_buffer.from_render = task.denoising_from_render;

	for(int i = 0; i < min(task.denoising_frames.size(), MAX_SECONDARY_FRAMES); i++) {
		assert(task.denoising_frames[i] != 0);
		render_buffer.frames.push_back(task.denoising_frames[i]);
	}

	target_buffer.pass_stride = task.target_pass_stride;
	target_buffer.denoising_clean_offset = task.pass_denoising_clean;

	type = task.denoising_type;
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

void DenoisingTask::set_render_buffer(RenderTile *rtiles)
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

	tiles->from_render = render_buffer.from_render? 1 : 0;

	assert(render_buffer.frames.size() <= MAX_SECONDARY_FRAMES);
	tiles->num_frames = render_buffer.frames.size()+1;

	tiles->frames[0] = 0;
	for(int i = 0; i < render_buffer.frames.size(); i++) {
		tiles->frames[i+1] = render_buffer.frames[i];
	}

	target_buffer.offset = rtiles[9].offset;
	target_buffer.stride = rtiles[9].stride;
	target_buffer.ptr    = rtiles[9].buffer;

	functions.set_tiles(buffers);
}

void DenoisingTask::setup_denoising_buffer()
{
	/* Expand filter_area by radius pixels and clamp the result to the extent of the neighboring tiles */
	rect = rect_from_shape(filter_area.x, filter_area.y, filter_area.z, filter_area.w);
	rect = rect_expand(rect, radius);
	rect = rect_clip(rect, make_int4(tiles->x[0], tiles->y[0], tiles->x[3], tiles->y[3]));

	buffer.passes = 15;
	buffer.width = rect.z - rect.x;
	buffer.stride = align_up(buffer.width, 4);
	buffer.h = rect.w - rect.y;
	int alignment_floats = divide_up(device->mem_sub_ptr_alignment(), sizeof(float));
	buffer.pass_stride = align_up(buffer.stride * buffer.h, alignment_floats);
	buffer.frame_stride = buffer.pass_stride * buffer.passes;
	int mem_size = buffer.frame_stride * tiles->num_frames + max(4, alignment_floats);
	buffer.mem.alloc_to_device(mem_size, false);
	buffer.mode = (tiles->num_frames > 1)? FEATURE_MODE_MULTIFRAME : FEATURE_MODE_RENDER;
}

void DenoisingTask::prefilter_shadowing()
{
	scoped_timer timer(&timing.shadowing, true);
	device_ptr null_ptr = (device_ptr) 0;

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
	nlm_state.is_color = false;

	{
		scoped_timer subtimer(&timing.shadowing_divide, true);
		/* Get the A/B unfiltered passes, the combined sample variance, the estimated variance of the sample variance and the buffer variance. */
		functions.divide_shadow(*unfiltered_a, *unfiltered_b, *sample_var, *sample_var_var, *buffer_var);
	}

	{
		scoped_timer subtimer(&timing.shadowing_smooth_variance, true);
		/* Smooth the (generally pretty noisy) buffer variance using the spatial information from the sample variance. */
		nlm_state.set_parameters(6, 3, 4.0f, 1.0f);
		functions.non_local_means(*buffer_var, *sample_var, *sample_var_var, *filtered_var);
	}

	device_ptr filtered_a = *buffer_var,
	           filtered_b = *sample_var;
	{
		scoped_timer subtimer(&timing.shadowing_first_cross, true);
		/* Reuse memory, the previous data isn't needed anymore. */
		/* Use the smoothed variance to filter the two shadow half images using each other for weight calculation. */
		nlm_state.set_parameters(5, 3, 1.0f, 0.25f);
		functions.non_local_means(*unfiltered_a, *unfiltered_b, *filtered_var, filtered_a);
		functions.non_local_means(*unfiltered_b, *unfiltered_a, *filtered_var, filtered_b);
	}

	device_ptr residual_var = *sample_var_var;
	{
		scoped_timer subtimer(&timing.shadowing_estimate_residual, true);
		/* Estimate the residual variance between the two filtered halves. */
		functions.combine_halves(filtered_a, filtered_b, null_ptr, residual_var, 2, rect);
	}

	device_ptr final_a = *unfiltered_a,
	           final_b = *unfiltered_b;
	{
		scoped_timer subtimer(&timing.shadowing_second_cross, true);
		/* Use the residual variance for a second filter pass. */
		nlm_state.set_parameters(4, 2, 1.0f, 0.5f);
		functions.non_local_means(filtered_a, filtered_b, residual_var, final_a);
		functions.non_local_means(filtered_b, filtered_a, residual_var, final_b);
	}

	{
		scoped_timer subtimer(&timing.shadowing_combining, true);
		/* Combine the two double-filtered halves to a final shadow feature. */
		device_sub_ptr shadow_pass(buffer.mem, 4*buffer.pass_stride, buffer.pass_stride);
		functions.combine_halves(final_a, final_b, *shadow_pass, null_ptr, 0, rect);
	}
}

void DenoisingTask::prefilter_features()
{
	scoped_timer timer(&timing.prefilter_features, true);

	device_sub_ptr unfiltered     (buffer.mem,  8*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr variance       (buffer.mem,  9*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_1(buffer.mem, 10*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_2(buffer.mem, 11*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_3(buffer.mem, 12*buffer.pass_stride, buffer.pass_stride);

	nlm_state.temporary_1_ptr = *nlm_temporary_1;
	nlm_state.temporary_2_ptr = *nlm_temporary_2;
	nlm_state.temporary_3_ptr = *nlm_temporary_3;
	nlm_state.is_color = false;

	int mean_from[]     = { 0, 1, 2, 12, 6,  7, 8 };
	int variance_from[] = { 3, 4, 5, 13, 9, 10, 11};
	int pass_to[]       = { 1, 2, 3, 0,  5,  6,  7};
	for(int pass = 0; pass < 7; pass++) {
		device_sub_ptr feature_pass(buffer.mem, pass_to[pass]*buffer.pass_stride, buffer.pass_stride);
		/* Get the unfiltered pass and its variance from the RenderBuffers. */
		functions.get_feature(mean_from[pass], variance_from[pass], *unfiltered, *variance);
		/* Smooth the pass and store the result in the denoising buffers. */
		nlm_state.set_parameters(2, 2, 1.0f, 0.25f);
		functions.non_local_means(*unfiltered, *unfiltered, *variance, *feature_pass);
	}
}

void DenoisingTask::prefilter_color()
{
	scoped_timer timer(&timing.prefilter_color, true);

	int mean_from[]     = {20, 21, 22};
	int variance_from[] = {23, 24, 25};
	int mean_to[]       = { 8,  9, 10};
	int variance_to[]   = {11, 12, 13};
	int num_color_passes = 3;

	storage.temporary_color.alloc_to_device(3*buffer.pass_stride, false);
	device_sub_ptr nlm_temporary_1(storage.temporary_color, 0*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_2(storage.temporary_color, 1*buffer.pass_stride, buffer.pass_stride);
	device_sub_ptr nlm_temporary_3(storage.temporary_color, 2*buffer.pass_stride, buffer.pass_stride);

	nlm_state.temporary_1_ptr = *nlm_temporary_1;
	nlm_state.temporary_2_ptr = *nlm_temporary_2;
	nlm_state.temporary_3_ptr = *nlm_temporary_3;
	nlm_state.is_color = true;

	for(int pass = 0; pass < num_color_passes; pass++) {
		device_sub_ptr color_pass(storage.temporary_color, pass*buffer.pass_stride, buffer.pass_stride);
		device_sub_ptr color_var_pass(buffer.mem, variance_to[pass]*buffer.pass_stride, buffer.pass_stride);
		functions.get_feature(mean_from[pass], variance_from[pass], *color_pass, *color_var_pass);
	}

	{
		scoped_timer subtimer(&timing.outlier_detection, true);
		device_sub_ptr depth_pass    (buffer.mem,                                 0,   buffer.pass_stride);
		device_sub_ptr color_var_pass(buffer.mem, variance_to[0]*buffer.pass_stride, 3*buffer.pass_stride);
		device_sub_ptr output_pass   (buffer.mem,     mean_to[0]*buffer.pass_stride, 3*buffer.pass_stride);
		functions.detect_outliers(storage.temporary_color.device_pointer, *color_var_pass, *depth_pass, *output_pass);

		device_sub_ptr intensity_pass(buffer.mem, 14*buffer.pass_stride, buffer.pass_stride);
		nlm_state.set_parameters(radius, 4, 2.0f, nlm_k_2*4.0f);
		functions.non_local_means(*output_pass, *output_pass, *color_var_pass, *intensity_pass);
	}

	storage.temporary_color.free();
}

void DenoisingTask::construct_transform()
{
	scoped_timer timer(&timing.transform, true);

	storage.w = filter_area.z;
	storage.h = filter_area.w;

	storage.transform.alloc_to_device(storage.w*storage.h*TRANSFORM_SIZE, false);
	storage.rank.alloc_to_device(storage.w*storage.h, false);

	functions.construct_transform();
}

void DenoisingTask::reconstruct()
{
	scoped_timer timer(&timing.reconstruct, true);

	device_only_memory<float> temporary_1(device, "Denoising NLM temporary 1");
	device_only_memory<float> temporary_2(device, "Denoising NLM temporary 2");
	temporary_1.alloc_to_device(buffer.pass_stride, false);
	temporary_2.alloc_to_device(buffer.pass_stride, false);
	reconstruction_state.temporary_1_ptr = temporary_1.device_pointer;
	reconstruction_state.temporary_2_ptr = temporary_2.device_pointer;

	storage.XtWX.alloc_to_device(storage.w*storage.h*XTWX_SIZE, false);
	storage.XtWY.alloc_to_device(storage.w*storage.h*XTWY_SIZE, false);
	storage.XtWX.zero_to_device();
	storage.XtWY.zero_to_device();

	reconstruction_state.filter_window = rect_from_shape(filter_area.x-rect.x, filter_area.y-rect.y, storage.w, storage.h);
	int tile_coordinate_offset = filter_area.y*target_buffer.stride + filter_area.x;
	reconstruction_state.buffer_params = make_int4(target_buffer.offset + tile_coordinate_offset,
	                                               target_buffer.stride,
	                                               target_buffer.pass_stride,
	                                               target_buffer.denoising_clean_offset);
	reconstruction_state.source_w = rect.z-rect.x;
	reconstruction_state.source_h = rect.w-rect.y;

	{
		scoped_timer subtimer(&timing.reconstruct_accumulate, true);
		device_sub_ptr color_ptr    (buffer.mem, 8*buffer.pass_stride, 3*buffer.pass_stride);
		device_sub_ptr color_var_ptr(buffer.mem, 11*buffer.pass_stride, 3*buffer.pass_stride);
		device_sub_ptr scale_ptr    (buffer.mem, 14*buffer.pass_stride, 3*buffer.pass_stride);
		for(int f = 0; f < tiles->num_frames; f++) {
			functions.accumulate(*color_ptr, *color_var_ptr, *scale_ptr, f);
		}
	}

	{
		scoped_timer subtimer(&timing.reconstruct_solve, true);
		functions.solve(target_buffer.ptr);
	}
}

void DenoisingTask::load_buffer()
{
	scoped_timer timer(&timing.load_buffer, true);

	device_ptr null_ptr = (device_ptr) 0;

	int original_offset = render_buffer.offset;

	for(int i = 0; i < tiles->num_frames; i++) {
		for(int pass = 0; pass < 15; pass++) {
			device_sub_ptr to_pass(buffer.mem, i*buffer.frame_stride + pass*buffer.pass_stride, buffer.pass_stride);
			functions.get_feature(pass, -1, *to_pass, null_ptr);
		}
		render_buffer.offset += render_buffer.frame_stride;
	}

	render_buffer.offset = original_offset;
}

void DenoisingTask::write_buffer()
{
	scoped_timer timer(&timing.write_buffer, true);

	reconstruction_state.buffer_params = make_int4(target_buffer.offset,
	                                               target_buffer.stride,
	                                               target_buffer.pass_stride,
	                                               target_buffer.denoising_clean_offset);
	for(int pass = 0; pass < 15; pass++) {
		device_sub_ptr from_pass(buffer.mem, pass*buffer.pass_stride, buffer.pass_stride);
		functions.write_feature(pass, *from_pass, target_buffer.ptr);
	}
}

bool DenoisingTask::run_prefiltering()
{
	scoped_timer timer(&timing.prefiltering, true);

	setup_denoising_buffer();

	prefilter_shadowing();
	prefilter_features();
	prefilter_color();

	write_buffer();

	return true;
}

bool DenoisingTask::run_filtering()
{
	scoped_timer timer(&timing.filtering, true);

	setup_denoising_buffer();

	load_buffer();

	construct_transform();
	reconstruct();

	return true;
}

bool DenoisingTask::run_denoising()
{
	scoped_timer timer(&timing.denoising, true);

	setup_denoising_buffer();

	prefilter_shadowing();
	prefilter_features();
	prefilter_color();

	construct_transform();
	reconstruct();

	return true;
}

bool DenoisingTask::run()
{
	if(type == DeviceTask::DENOISE_BOTH) {
		return run_denoising();
	}
	else if(type == DeviceTask::DENOISE_PREFILTER) {
		return run_prefiltering();
	}
	else if(type == DeviceTask::DENOISE_FILTER) {
		return run_filtering();
	}
	else {
		return false;
	}
}

CCL_NAMESPACE_END
