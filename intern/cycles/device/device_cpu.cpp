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
#include <string.h>

/* So ImathMath is included before our kernel_cpu_compat. */
#ifdef WITH_OSL
/* So no context pollution happens from indirectly included windows.h */
#  include "util_windows.h"
#  include <OSL/oslexec.h>
#endif

#include "device.h"
#include "device_intern.h"
#include "device_denoising.h"

#include "kernel.h"
#include "kernel_compat_cpu.h"
#include "kernel_types.h"
#include "kernel_globals.h"

#include "filter.h"

#include "osl_shader.h"
#include "osl_globals.h"

#include "buffers.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_function.h"
#include "util_logging.h"
#include "util_opengl.h"
#include "util_progress.h"
#include "util_system.h"
#include "util_thread.h"

CCL_NAMESPACE_BEGIN

/* Has to be outside of the class to be shared across template instantiations. */
static bool logged_architecture = false;

template<typename F>
class KernelFunctions {
public:
	KernelFunctions(F kernel_default,
	                F kernel_sse2,
	                F kernel_sse3,
	                F kernel_sse41,
	                F kernel_avx,
	                F kernel_avx2)
	{
		string architecture_name = "default";
		kernel = kernel_default;

		/* Silence potential warnings about unused variables
		 * when compiling without some architectures. */
		(void)kernel_sse2;
		(void)kernel_sse3;
		(void)kernel_sse41;
		(void)kernel_avx;
		(void)kernel_avx2;
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			architecture_name = "AVX2";
			kernel = kernel_avx2;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			architecture_name = "AVX";
			kernel = kernel_avx;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
		if(system_cpu_support_sse41()) {
			architecture_name = "SSE4.1";
			kernel = kernel_sse41;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			architecture_name = "SSE3";
			kernel = kernel_sse3;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			architecture_name = "SSE2";
			kernel = kernel_sse2;
		}
#endif

		if(!logged_architecture) {
			VLOG(1) << "Will be using " << architecture_name << " kernels.";
			logged_architecture = true;
		}
	}

	inline F operator()() const {
		return kernel;
	}
protected:
	F kernel;
};

class CPUDevice : public Device
{
public:
	TaskPool task_pool;
	KernelGlobals kernel_globals;

#ifdef WITH_OSL
	OSLGlobals osl_globals;
#endif

	KernelFunctions<void(*)(KernelGlobals *, float *, unsigned int *, int, int, int, int, int)>   path_trace_kernel;
	KernelFunctions<void(*)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int)>       convert_to_half_float_kernel;
	KernelFunctions<void(*)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int)>       convert_to_byte_kernel;
	KernelFunctions<void(*)(KernelGlobals *, uint4 *, float4 *, float*, int, int, int, int, int)> shader_kernel;

	KernelFunctions<void(*)(int, TilesInfo*, int, int, float*, float*, float*, float*, float*, int*, int, int, bool)> filter_divide_shadow_kernel;
	KernelFunctions<void(*)(int, TilesInfo*, int, int, int, int, float*, float*, int*, int, int, bool)>               filter_get_feature_kernel;
	KernelFunctions<void(*)(int, int, float*, float*, float*, float*, int*, int)>                                     filter_combine_halves_kernel;
	KernelFunctions<void(*)(int, int, int, float*, int, int, int, int)>                                               filter_divide_combined_kernel;

	KernelFunctions<void(*)(int, int, float*, float*, float*, int*, int, int, float, float)> filter_nlm_calc_difference_kernel;
	KernelFunctions<void(*)(float*, float*, int*, int, int)>                                 filter_nlm_blur_kernel;
	KernelFunctions<void(*)(float*, float*, int*, int, int)>                                 filter_nlm_calc_weight_kernel;
	KernelFunctions<void(*)(int, int, float*, float*, float*, float*, int*, int, int)>       filter_nlm_update_output_kernel;
	KernelFunctions<void(*)(float*, float*, int*, int)>                                      filter_nlm_normalize_kernel;

	KernelFunctions<void(*)(int, float*, int, int, int, float*, int*, int*, int, float)>                                         filter_construct_transform_kernel;
	KernelFunctions<void(*)(int, int, float*, float*, float*, float*, float*, int*, float*, float3*, int*, int*, int, int, int)> filter_nlm_construct_gramian_kernel;
	KernelFunctions<void(*)(int, int, int, int, int, float*, int*, float*, float3*, int*, int)>                                  filter_finalize_kernel;

#define KERNEL_FUNCTIONS(name) \
	      KERNEL_NAME_EVAL(cpu, name), \
	      KERNEL_NAME_EVAL(cpu_sse2, name), \
	      KERNEL_NAME_EVAL(cpu_sse3, name), \
	      KERNEL_NAME_EVAL(cpu_sse41, name), \
	      KERNEL_NAME_EVAL(cpu_avx, name), \
	      KERNEL_NAME_EVAL(cpu_avx2, name)

	CPUDevice(DeviceInfo& info, Stats &stats, bool background)
	: Device(info, stats, background),
	  path_trace_kernel(KERNEL_FUNCTIONS(path_trace)),
	  convert_to_half_float_kernel(KERNEL_FUNCTIONS(convert_to_half_float)),
	  convert_to_byte_kernel(KERNEL_FUNCTIONS(convert_to_byte)),
	  shader_kernel(KERNEL_FUNCTIONS(shader)),
	  filter_divide_shadow_kernel(KERNEL_FUNCTIONS(filter_divide_shadow)),
	  filter_get_feature_kernel(KERNEL_FUNCTIONS(filter_get_feature)),
	  filter_combine_halves_kernel(KERNEL_FUNCTIONS(filter_combine_halves)),
	  filter_divide_combined_kernel(KERNEL_FUNCTIONS(filter_divide_combined)),
	  filter_nlm_calc_difference_kernel(KERNEL_FUNCTIONS(filter_nlm_calc_difference)),
	  filter_nlm_blur_kernel(KERNEL_FUNCTIONS(filter_nlm_blur)),
	  filter_nlm_calc_weight_kernel(KERNEL_FUNCTIONS(filter_nlm_calc_weight)),
	  filter_nlm_update_output_kernel(KERNEL_FUNCTIONS(filter_nlm_update_output)),
	  filter_nlm_normalize_kernel(KERNEL_FUNCTIONS(filter_nlm_normalize)),
	  filter_construct_transform_kernel(KERNEL_FUNCTIONS(filter_construct_transform)),
	  filter_nlm_construct_gramian_kernel(KERNEL_FUNCTIONS(filter_nlm_construct_gramian)),
	  filter_finalize_kernel(KERNEL_FUNCTIONS(filter_finalize))
	{
#ifdef WITH_OSL
		kernel_globals.osl = &osl_globals;
#endif
		system_enable_ftz();
	}

	~CPUDevice()
	{
		task_pool.stop();
	}

	virtual bool show_samples() const
	{
		return (TaskScheduler::num_threads() == 1);
	}

	void mem_alloc(device_memory& mem, MemoryType /*type*/)
	{
		mem.device_pointer = mem.data_pointer? mem.data_pointer : ((device_ptr) new char[mem.memory_size()]);
		mem.device_size = mem.memory_size();
		stats.mem_alloc(mem.device_size);
	}

	void mem_copy_to(device_memory& /*mem*/)
	{
		/* no-op */
	}

	void mem_copy_from(device_memory& /*mem*/,
	                   int /*y*/, int /*w*/, int /*h*/,
	                   int /*elem*/)
	{
		/* no-op */
	}

	void mem_zero(device_memory& mem)
	{
		memset((void*)mem.device_pointer, 0, mem.memory_size());
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			if(!mem.data_pointer) {
				delete[] (char*) mem.device_pointer;
			}
			mem.device_pointer = 0;
			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
	}

	virtual device_ptr mem_get_offset_ptr(device_memory& mem, int offset)
	{
		return (device_ptr) (((char*) mem.device_pointer) + mem.memory_offset(offset));
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		kernel_const_copy(&kernel_globals, name, host, size);
	}

	void tex_alloc(const char *name,
	               device_memory& mem,
	               InterpolationType interpolation,
	               ExtensionType extension)
	{
		VLOG(1) << "Texture allocate: " << name << ", "
		        << string_human_readable_number(mem.memory_size()) << " bytes. ("
		        << string_human_readable_size(mem.memory_size()) << ")";
		kernel_tex_copy(&kernel_globals,
		                name,
		                mem.data_pointer,
		                mem.data_width,
		                mem.data_height,
		                mem.data_depth,
		                interpolation,
		                extension);
		mem.device_pointer = mem.data_pointer;
		mem.device_size = mem.memory_size();
		stats.mem_alloc(mem.device_size);
	}

	void tex_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			mem.device_pointer = 0;
			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
	}

	void *osl_memory()
	{
#ifdef WITH_OSL
		return &osl_globals;
#else
		return NULL;
#endif
	}

	void thread_run(DeviceTask *task)
	{
		if(task->type == DeviceTask::RENDER)
			thread_render(*task);
		else if(task->type == DeviceTask::FILM_CONVERT)
			thread_film_convert(*task);
		else if(task->type == DeviceTask::SHADER)
			thread_shader(*task);
	}

	class CPUDeviceTask : public DeviceTask {
	public:
		CPUDeviceTask(CPUDevice *device, DeviceTask& task)
		: DeviceTask(task)
		{
			run = function_bind(&CPUDevice::thread_run, device, this);
		}
	};

	bool denoising_non_local_means(device_ptr image_ptr, device_ptr guide_ptr, device_ptr variance_ptr, device_ptr out_ptr,
	                               DenoisingTask *task)
	{
		int4 rect = task->rect;
		int   r   = task->nlm_state.r;
		int   f   = task->nlm_state.f;
		float a   = task->nlm_state.a;
		float k_2 = task->nlm_state.k_2;

		int w = align_up(rect.z-rect.x, 4);
		int h = rect.w-rect.y;

		float *blurDifference = (float*) task->nlm_state.temporary_1_ptr;
		float *difference     = (float*) task->nlm_state.temporary_2_ptr;
		float *weightAccum    = (float*) task->nlm_state.temporary_3_ptr;

		memset(weightAccum, 0, sizeof(float)*w*h);
		memset((float*) out_ptr, 0, sizeof(float)*w*h);

		for(int i = 0; i < (2*r+1)*(2*r+1); i++) {
			int dy = i / (2*r+1) - r;
			int dx = i % (2*r+1) - r;

			int local_rect[4] = {max(0, -dx), max(0, -dy), rect.z-rect.x - max(0, dx), rect.w-rect.y - max(0, dy)};
			filter_nlm_calc_difference_kernel()(dx, dy,
			                                    (float*) guide_ptr,
			                                    (float*) variance_ptr,
			                                    difference,
			                                    local_rect,
			                                    w, 0,
			                                    a, k_2);

			filter_nlm_blur_kernel()       (difference, blurDifference, local_rect, w, f);
			filter_nlm_calc_weight_kernel()(blurDifference, difference, local_rect, w, f);
			filter_nlm_blur_kernel()       (difference, blurDifference, local_rect, w, f);

			filter_nlm_update_output_kernel()(dx, dy,
			                                  blurDifference,
			                                  (float*) image_ptr,
			                                  (float*) out_ptr,
			                                  weightAccum,
			                                  local_rect,
			                                  w, f);
		}

		int local_rect[4] = {0, 0, rect.z-rect.x, rect.w-rect.y};
		filter_nlm_normalize_kernel()((float*) out_ptr, weightAccum, local_rect, w);

		return true;
	}

	bool denoising_construct_transform(DenoisingTask *task)
	{
		for(int y = 0; y < task->filter_area.w; y++) {
			for(int x = 0; x < task->filter_area.z; x++) {
				filter_construct_transform_kernel()(task->render_buffer.samples,
				                                    (float*) task->buffer.mem.device_pointer,
				                                    x + task->filter_area.x,
				                                    y + task->filter_area.y,
				                                    y*task->filter_area.z + x,
				                                    (float*) task->storage.transform.device_pointer,
				                                    (int*)   task->storage.rank.device_pointer,
				                                    &task->rect.x,
				                                    task->half_window,
				                                    task->pca_threshold);
			}
		}
		return true;
	}

	bool denoising_reconstruct(device_ptr color_ptr,
	                           device_ptr color_variance_ptr,
	                           device_ptr guide_ptr,
	                           device_ptr guide_variance_ptr,
	                           device_ptr output_ptr,
	                           DenoisingTask *task)
	{
		mem_zero(task->storage.XtWX);
		mem_zero(task->storage.XtWY);

		float *difference     = (float*) task->reconstruction_state.temporary_1_ptr;
		float *blurDifference = (float*) task->reconstruction_state.temporary_2_ptr;

		int r = task->half_window;
		for(int i = 0; i < (2*r+1)*(2*r+1); i++) {
			int dy = i / (2*r+1) - r;
			int dx = i % (2*r+1) - r;

			int local_rect[4] = {max(0, -dx), max(0, -dy),
			                     task->reconstruction_state.source_w - max(0, dx),
			                     task->reconstruction_state.source_h - max(0, dy)};
			filter_nlm_calc_difference_kernel()(dx, dy,
			                                    (float*) guide_ptr,
			                                    (float*) guide_variance_ptr,
			                                    difference,
			                                    local_rect,
			                                    task->buffer.w,
			                                    task->buffer.pass_stride,
			                                    1.0f,
			                                    task->nlm_k_2);
			filter_nlm_blur_kernel()(difference, blurDifference, local_rect, task->buffer.w, 4);
			filter_nlm_calc_weight_kernel()(blurDifference, difference, local_rect, task->buffer.w, 4);
			filter_nlm_blur_kernel()(difference, blurDifference, local_rect, task->buffer.w, 4);
			filter_nlm_construct_gramian_kernel()(dx, dy,
			                                      blurDifference,
			                                      (float*)  task->buffer.mem.device_pointer,
			                                      (float*)  color_ptr,
			                                      (float*)  color_variance_ptr,
			                                      (float*)  task->storage.transform.device_pointer,
			                                      (int*)    task->storage.rank.device_pointer,
			                                      (float*)  task->storage.XtWX.device_pointer,
			                                      (float3*) task->storage.XtWY.device_pointer,
			                                      local_rect,
			                                      &task->reconstruction_state.filter_rect.x,
			                                      task->buffer.w,
			                                      task->buffer.h,
			                                      4);
		}
		for(int y = 0; y < task->filter_area.w; y++) {
			for(int x = 0; x < task->filter_area.z; x++) {
				filter_finalize_kernel()(x,
				                         y,
				                         y*task->filter_area.z + x,
				                         task->buffer.w,
				                         task->buffer.h,
				                         (float*)  output_ptr,
				                         (int*)    task->storage.rank.device_pointer,
				                         (float*)  task->storage.XtWX.device_pointer,
				                         (float3*) task->storage.XtWY.device_pointer,
				                         &task->reconstruction_state.buffer_params.x,
				                         task->render_buffer.samples);
			}
		}
		return true;
	}

	bool denoising_combine_halves(device_ptr a_ptr, device_ptr b_ptr,
	                              device_ptr mean_ptr, device_ptr variance_ptr,
	                              int r, int4 rect, DenoisingTask *task)
	{
		(void) task;
		for(int y = rect.y; y < rect.w; y++) {
			for(int x = rect.x; x < rect.z; x++) {
				filter_combine_halves_kernel()(x, y,
				                               (float*) mean_ptr,
				                               (float*) variance_ptr,
				                               (float*) a_ptr,
				                               (float*) b_ptr,
				                               &rect.x,
				                               r);
			}
		}
		return true;
	}

	bool denoising_divide_shadow(device_ptr a_ptr, device_ptr b_ptr,
	                             device_ptr sample_variance_ptr, device_ptr sv_variance_ptr,
	                             device_ptr buffer_variance_ptr, DenoisingTask *task)
	{
		for(int y = task->rect.y; y < task->rect.w; y++) {
			for(int x = task->rect.x; x < task->rect.z; x++) {
				filter_divide_shadow_kernel()(task->render_buffer.samples,
				                              task->tiles,
				                              x, y,
				                              (float*) a_ptr,
				                              (float*) b_ptr,
				                              (float*) sample_variance_ptr,
				                              (float*) sv_variance_ptr,
				                              (float*) buffer_variance_ptr,
				                              &task->rect.x,
				                              task->render_buffer.pass_stride,
				                              task->render_buffer.denoising_data_offset,
				                              task->use_gradients);
			}
		}
		return true;
	}

	bool denoising_get_feature(int mean_offset,
	                           int variance_offset,
	                           device_ptr mean_ptr,
	                           device_ptr variance_ptr,
	                           DenoisingTask *task)
	{
		for(int y = task->rect.y; y < task->rect.w; y++) {
			for(int x = task->rect.x; x < task->rect.z; x++) {
				filter_get_feature_kernel()(task->render_buffer.samples,
				                            task->tiles,
				                            mean_offset,
				                            variance_offset,
				                            x, y,
				                            (float*) mean_ptr,
				                            (float*) variance_ptr,
				                            &task->rect.x,
				                            task->render_buffer.pass_stride,
				                            task->render_buffer.denoising_data_offset,
				                            task->use_cross_denoising);
			}
		}
		return true;
	}

	void thread_render(DeviceTask& task)
	{
		if(task_pool.canceled()) {
			if(task.need_finish_queue == false)
				return;
		}

		KernelGlobals kg = thread_kernel_globals_init();
		RenderTile tile;

		while(task.acquire_tile(this, tile)) {
			float *render_buffer = (float*)tile.buffer;

			if(tile.task == RenderTile::PATH_TRACE) {
				uint *rng_state = (uint*)tile.rng_state;
				int start_sample = tile.start_sample;
				int end_sample = tile.start_sample + tile.num_samples;

				for(int sample = start_sample; sample < end_sample; sample++) {
#ifdef WITH_CYCLES_DEBUG_FPE
					scoped_fpe fpe(FPE_ENABLED);
#endif
					if(task.get_cancel() || task_pool.canceled()) {
						if(task.need_finish_queue == false)
							break;
					}

					for(int y = tile.y; y < tile.y + tile.h; y++) {
						for(int x = tile.x; x < tile.x + tile.w; x++) {
							path_trace_kernel()(&kg, render_buffer, rng_state,
							                    sample, x, y, tile.offset, tile.stride);
						}
					}

					tile.sample = sample + 1;

#ifdef WITH_CYCLES_DEBUG_FPE
					fpe.restore();
#endif
					task.update_progress(&tile, tile.w*tile.h);
				}

				if(tile.buffers->params.overscan && !task.get_cancel()) {
					DenoisingTask denoising(this);

					int overscan = tile.buffers->params.overscan;
					denoising.filter_area = make_int4(tile.x + overscan, tile.y + overscan, tile.w - 2*overscan, tile.h - 2*overscan);
					denoising.render_buffer.samples = end_sample;

					denoising.tiles_from_single_tile(tile);
					denoising.init_from_devicetask(task);

					denoising.functions.construct_transform = function_bind(&CPUDevice::denoising_construct_transform, this, &denoising);
					denoising.functions.reconstruct = function_bind(&CPUDevice::denoising_reconstruct, this, _1, _2, _3, _4, _5, &denoising);
					denoising.functions.divide_shadow = function_bind(&CPUDevice::denoising_divide_shadow, this, _1, _2, _3, _4, _5, &denoising);
					denoising.functions.non_local_means = function_bind(&CPUDevice::denoising_non_local_means, this, _1, _2, _3, _4, &denoising);
					denoising.functions.combine_halves = function_bind(&CPUDevice::denoising_combine_halves, this, _1, _2, _3, _4, _5, _6, &denoising);
					denoising.functions.get_feature = function_bind(&CPUDevice::denoising_get_feature, this, _1, _2, _3, _4, &denoising);

					denoising.run_denoising();
				}
			}
			else if(tile.task == RenderTile::DENOISE) {
				tile.sample = tile.start_sample + tile.num_samples;

				DenoisingTask denoising(this);
				denoising.filter_area = make_int4(tile.x, tile.y, tile.w, tile.h);
				denoising.render_buffer.samples = tile.sample;

				RenderTile rtiles[9];
				rtiles[4] = tile;
				task.get_neighbor_tiles(rtiles);
				denoising.tiles_from_rendertiles(rtiles);

				denoising.init_from_devicetask(task);
				denoising.functions.construct_transform = function_bind(&CPUDevice::denoising_construct_transform, this, &denoising);
				denoising.functions.reconstruct = function_bind(&CPUDevice::denoising_reconstruct, this, _1, _2, _3, _4, _5, &denoising);
				denoising.functions.divide_shadow = function_bind(&CPUDevice::denoising_divide_shadow, this, _1, _2, _3, _4, _5, &denoising);
				denoising.functions.non_local_means = function_bind(&CPUDevice::denoising_non_local_means, this, _1, _2, _3, _4, &denoising);
				denoising.functions.combine_halves = function_bind(&CPUDevice::denoising_combine_halves, this, _1, _2, _3, _4, _5, _6, &denoising);
				denoising.functions.get_feature = function_bind(&CPUDevice::denoising_get_feature, this, _1, _2, _3, _4, &denoising);

				denoising.run_denoising();

				task.update_progress(&tile, tile.w*tile.h);
			}

			task.release_tile(tile);

			if(task_pool.canceled()) {
				if(task.need_finish_queue == false)
					break;
			}
		}

		thread_kernel_globals_free(&kg);
	}

	void thread_film_convert(DeviceTask& task)
	{
		float sample_scale = 1.0f/(task.sample + 1);

		if(task.rgba_half) {
			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					convert_to_half_float_kernel()(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
					                               sample_scale, x, y, task.offset, task.stride);
		}
		else {
			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					convert_to_byte_kernel()(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
					                         sample_scale, x, y, task.offset, task.stride);

		}
	}

	void thread_shader(DeviceTask& task)
	{
		KernelGlobals kg = kernel_globals;

#ifdef WITH_OSL
		OSLShader::thread_init(&kg, &kernel_globals, &osl_globals);
#endif
		for(int sample = 0; sample < task.num_samples; sample++) {
			for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
				shader_kernel()(&kg,
				                (uint4*)task.shader_input,
				                (float4*)task.shader_output,
				                (float*)task.shader_output_luma,
				                task.shader_eval_type,
				                task.shader_filter,
				                x,
				                task.offset,
				                sample);

			if(task.get_cancel() || task_pool.canceled())
				break;

			task.update_progress(NULL);

		}

#ifdef WITH_OSL
		OSLShader::thread_free(&kg);
#endif
	}

	int get_split_task_count(DeviceTask& task)
	{
		if(task.type == DeviceTask::SHADER)
			return task.get_subtask_count(TaskScheduler::num_threads(), 256);
		else
			return task.get_subtask_count(TaskScheduler::num_threads());
	}

	void task_add(DeviceTask& task)
	{
		/* split task into smaller ones */
		list<DeviceTask> tasks;

		if(task.type == DeviceTask::SHADER)
			task.split(tasks, TaskScheduler::num_threads(), 256);
		else
			task.split(tasks, TaskScheduler::num_threads());

		foreach(DeviceTask& task, tasks)
			task_pool.push(new CPUDeviceTask(this, task));
	}

	void task_wait()
	{
		task_pool.wait_work();
	}

	void task_cancel()
	{
		task_pool.cancel();
	}

protected:
	inline KernelGlobals thread_kernel_globals_init()
	{
		KernelGlobals kg = kernel_globals;
		kg.transparent_shadow_intersections = NULL;
		const int decoupled_count = sizeof(kg.decoupled_volume_steps) /
		                            sizeof(*kg.decoupled_volume_steps);
		for(int i = 0; i < decoupled_count; ++i) {
			kg.decoupled_volume_steps[i] = NULL;
		}
		kg.decoupled_volume_steps_index = 0;
#ifdef WITH_OSL
		OSLShader::thread_init(&kg, &kernel_globals, &osl_globals);
#endif
		return kg;
	}

	inline void thread_kernel_globals_free(KernelGlobals *kg)
	{
		if(kg->transparent_shadow_intersections != NULL) {
			free(kg->transparent_shadow_intersections);
		}
		const int decoupled_count = sizeof(kg->decoupled_volume_steps) /
		                            sizeof(*kg->decoupled_volume_steps);
		for(int i = 0; i < decoupled_count; ++i) {
			if(kg->decoupled_volume_steps[i] != NULL) {
				free(kg->decoupled_volume_steps[i]);
			}
		}
#ifdef WITH_OSL
		OSLShader::thread_free(kg);
#endif
	}
};

Device *device_cpu_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new CPUDevice(info, stats, background);
}

void device_cpu_info(vector<DeviceInfo>& devices)
{
	DeviceInfo info;

	info.type = DEVICE_CPU;
	info.description = system_cpu_brand_string();
	info.id = "CPU";
	info.num = 0;
	info.advanced_shading = true;
	info.pack_images = false;

	devices.insert(devices.begin(), info);
}

string device_cpu_capabilities(void)
{
	string capabilities = "";
	capabilities += system_cpu_support_sse2() ? "SSE2 " : "";
	capabilities += system_cpu_support_sse3() ? "SSE3 " : "";
	capabilities += system_cpu_support_sse41() ? "SSE41 " : "";
	capabilities += system_cpu_support_avx() ? "AVX " : "";
	capabilities += system_cpu_support_avx2() ? "AVX2" : "";
	if(capabilities[capabilities.size() - 1] == ' ')
		capabilities.resize(capabilities.size() - 1);
	return capabilities;
}

CCL_NAMESPACE_END
