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

#include "kernel.h"
#include "kernel_compat_cpu.h"
#include "kernel_types.h"
#include "kernel_globals.h"

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

class CPUDevice : public Device
{
public:
	TaskPool task_pool;
	KernelGlobals kernel_globals;

#ifdef WITH_OSL
	OSLGlobals osl_globals;
#endif
	
	CPUDevice(DeviceInfo& info, Stats &stats, bool background)
	: Device(info, stats, background)
	{
#ifdef WITH_OSL
		kernel_globals.osl = &osl_globals;
#endif

		/* do now to avoid thread issues */
		system_cpu_support_sse2();
		system_cpu_support_sse3();
		system_cpu_support_sse41();
		system_cpu_support_avx();
		system_cpu_support_avx2();

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			VLOG(1) << "Will be using AVX2 kernels.";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			VLOG(1) << "Will be using AVX kernels.";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
		if(system_cpu_support_sse41()) {
			VLOG(1) << "Will be using SSE4.1 kernels.";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			VLOG(1) << "Will be using SSE3kernels.";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			VLOG(1) << "Will be using SSE2 kernels.";
		}
		else
#endif
		{
			VLOG(1) << "Will be using regular kernels.";
		}
	}

	~CPUDevice()
	{
		task_pool.stop();
	}

	void mem_alloc(device_memory& mem, MemoryType /*type*/)
	{
		mem.device_pointer = mem.data_pointer;
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
			mem.device_pointer = 0;
			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
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

	void thread_render(DeviceTask& task)
	{
		if(task_pool.canceled()) {
			if(task.need_finish_queue == false)
				return;
		}

		KernelGlobals kg = thread_kernel_globals_init();
		RenderTile tile;

		void(*path_trace_kernel)(KernelGlobals*, float*, unsigned int*, int, int, int, int, int);
		void(*filter_estimate_params_kernel)(KernelGlobals*, int, float**, int, int, int*, int*, int*, int*, void*, int4);
		void(*filter_final_pass_kernel)(KernelGlobals*, int, float**, int, int, int*, int*, int*, int*, void*, int4);

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			path_trace_kernel = kernel_cpu_avx2_path_trace;
			filter_estimate_params_kernel = kernel_cpu_avx2_filter_estimate_params;
			filter_final_pass_kernel = kernel_cpu_avx2_filter_final_pass;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			path_trace_kernel = kernel_cpu_avx_path_trace;
			filter_estimate_params_kernel = kernel_cpu_avx_filter_estimate_params;
			filter_final_pass_kernel = kernel_cpu_avx_filter_final_pass;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
		if(system_cpu_support_sse41()) {
			path_trace_kernel = kernel_cpu_sse41_path_trace;
			filter_estimate_params_kernel = kernel_cpu_sse41_filter_estimate_params;
			filter_final_pass_kernel = kernel_cpu_sse41_filter_final_pass;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			path_trace_kernel = kernel_cpu_sse3_path_trace;
			filter_estimate_params_kernel = kernel_cpu_sse3_filter_estimate_params;
			filter_final_pass_kernel = kernel_cpu_sse3_filter_final_pass;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			path_trace_kernel = kernel_cpu_sse2_path_trace;
			filter_estimate_params_kernel = kernel_cpu_sse2_filter_estimate_params;
			filter_final_pass_kernel = kernel_cpu_sse2_filter_final_pass;
		}
		else
#endif
		{
			path_trace_kernel = kernel_cpu_path_trace;
			filter_estimate_params_kernel = kernel_cpu_filter_estimate_params;
			filter_final_pass_kernel = kernel_cpu_filter_final_pass;
		}
		
		while(task.acquire_tile(this, tile)) {
			float *render_buffer = (float*)tile.buffer;

			if(tile.task == RenderTile::PATH_TRACE) {
				uint *rng_state = (uint*)tile.rng_state;
				int start_sample = tile.start_sample;
				int end_sample = tile.start_sample + tile.num_samples;

				for(int sample = start_sample; sample < end_sample; sample++) {
					if(task.get_cancel() || task_pool.canceled()) {
						if(task.need_finish_queue == false)
							break;
					}

					for(int y = tile.y; y < tile.y + tile.h; y++) {
						for(int x = tile.x; x < tile.x + tile.w; x++) {
							path_trace_kernel(&kg, render_buffer, rng_state,
							                  sample, x, y, tile.offset, tile.stride);
						}
					}

					tile.sample = sample + 1;

					task.update_progress(&tile);
				}

				if(tile.buffers->params.overscan) {
					int tile_x[4] = {tile.x, tile.x, tile.x+tile.w, tile.x+tile.w};
					int tile_y[4] = {tile.y, tile.y, tile.y+tile.h, tile.y+tile.h};
					int offsets[9] = {0, 0, 0, 0, tile.offset, 0, 0, 0, 0};
					int strides[9] = {0, 0, 0, 0, tile.stride, 0, 0, 0, 0};
					float *buffers[9] = {NULL, NULL, NULL, NULL, (float*) tile.buffer, NULL, NULL, NULL, NULL};
					FilterStorage *storages = new FilterStorage[tile.buffers->params.final_width*tile.buffers->params.final_height];

					int overscan = tile.buffers->params.overscan;
					int4 filter_rect = make_int4(tile.x + overscan, tile.y + overscan, tile.x + tile.w - overscan, tile.y + tile.h - overscan);
					for(int y = filter_rect.y; y < filter_rect.w; y++) {
						for(int x = filter_rect.x; x < filter_rect.z; x++) {
							filter_estimate_params_kernel(&kg, end_sample, buffers, x, y, tile_x, tile_y, offsets, strides, storages, filter_rect);
						}
					}
					for(int y = filter_rect.y; y < filter_rect.w; y++) {
						for(int x = filter_rect.x; x < filter_rect.z; x++) {
							filter_final_pass_kernel(&kg, end_sample, buffers, x, y, tile_x, tile_y, offsets, strides, storages, filter_rect);
						}
					}

#ifdef WITH_CYCLES_DEBUG_FILTER
#define WRITE_DEBUG(name, var) debug_write_pfm(string_printf("debug_%dx%d_%s.pfm", tile.x, tile.y, name).c_str(), &storages[0].var, tile.buffers->params.final_width, tile.buffers->params.final_height, sizeof(FilterStorage)/sizeof(float), tile.buffers->params.final_width);
					for(int i = 0; i < DENOISE_FEATURES; i++) {
						WRITE_DEBUG(string_printf("mean_%d", i).c_str(), means[i]);
						WRITE_DEBUG(string_printf("scale_%d", i).c_str(), scales[i]);
						WRITE_DEBUG(string_printf("singular_%d", i).c_str(), singular[i]);
						WRITE_DEBUG(string_printf("bandwidth_%d", i).c_str(), bandwidth[i]);
					}
					WRITE_DEBUG("singular_threshold", singular_threshold);
					WRITE_DEBUG("feature_matrix_norm", feature_matrix_norm);
					WRITE_DEBUG("global_bandwidth", global_bandwidth);
					WRITE_DEBUG("filtered_global_bandwidth", filtered_global_bandwidth);
					WRITE_DEBUG("sum_weight", sum_weight);
					WRITE_DEBUG("log_rmse_per_sample", log_rmse_per_sample);
#undef WRITE_DEBUG
#endif
					delete[] storages;
				}
			}
			else if(tile.task == RenderTile::DENOISE) {
				int sample = tile.start_sample + tile.num_samples;

				RenderTile rtiles[9];
				rtiles[4] = tile;
				task.get_neighbor_tiles(rtiles);
				float *buffers[9];
				int offsets[9], strides[9];
				for(int i = 0; i < 9; i++) {
					buffers[i] = (float*) rtiles[i].buffer;
					offsets[i] = rtiles[i].offset;
					strides[i] = rtiles[i].stride;
				}
				int tile_x[4] = {rtiles[3].x, rtiles[4].x, rtiles[5].x, rtiles[5].x+rtiles[5].w};
				int tile_y[4] = {rtiles[1].y, rtiles[4].y, rtiles[7].y, rtiles[7].y+rtiles[7].h};
				FilterStorage *storages = new FilterStorage[tile.w*tile.h];

				int4 filter_rect = make_int4(tile.x, tile.y, tile.x + tile.w, tile.y + tile.h);
				for(int y = filter_rect.y; y < filter_rect.w; y++) {
					for(int x = filter_rect.x; x < filter_rect.z; x++) {
						filter_estimate_params_kernel(&kg, sample, buffers, x, y, tile_x, tile_y, offsets, strides, storages, filter_rect);
					}
				}
				for(int y = filter_rect.y; y < filter_rect.w; y++) {
					for(int x = filter_rect.x; x < filter_rect.z; x++) {
						filter_final_pass_kernel(&kg, sample, buffers, x, y, tile_x, tile_y, offsets, strides, storages, filter_rect);
					}
				}
#ifdef WITH_CYCLES_DEBUG_FILTER
#define WRITE_DEBUG(name, var) debug_write_pfm(string_printf("debug_%dx%d_%s.pfm", tile.x, tile.y, name).c_str(), &storages[0].var, tile.w, tile.h, sizeof(FilterStorage)/sizeof(float), tile.w);
				for(int i = 0; i < DENOISE_FEATURES; i++) {
					WRITE_DEBUG(string_printf("mean_%d", i).c_str(), means[i]);
					WRITE_DEBUG(string_printf("scale_%d", i).c_str(), scales[i]);
					WRITE_DEBUG(string_printf("singular_%d", i).c_str(), singular[i]);
					WRITE_DEBUG(string_printf("bandwidth_%d", i).c_str(), bandwidth[i]);
				}
				WRITE_DEBUG("singular_threshold", singular_threshold);
				WRITE_DEBUG("feature_matrix_norm", feature_matrix_norm);
				WRITE_DEBUG("global_bandwidth", global_bandwidth);
				WRITE_DEBUG("filtered_global_bandwidth", filtered_global_bandwidth);
				WRITE_DEBUG("sum_weight", sum_weight);
				WRITE_DEBUG("log_rmse_per_sample", log_rmse_per_sample);
#undef WRITE_DEBUG
#endif
				delete[] storages;
				tile.sample = sample;

				task.update_progress(&tile);
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
			void(*convert_to_half_float_kernel)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int);
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
			if(system_cpu_support_avx2()) {
				convert_to_half_float_kernel = kernel_cpu_avx2_convert_to_half_float;
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
			if(system_cpu_support_avx()) {
				convert_to_half_float_kernel = kernel_cpu_avx_convert_to_half_float;
			}
			else
#endif	
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
			if(system_cpu_support_sse41()) {
				convert_to_half_float_kernel = kernel_cpu_sse41_convert_to_half_float;
			}
			else
#endif		
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3		
			if(system_cpu_support_sse3()) {
				convert_to_half_float_kernel = kernel_cpu_sse3_convert_to_half_float;
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
			if(system_cpu_support_sse2()) {
				convert_to_half_float_kernel = kernel_cpu_sse2_convert_to_half_float;
			}
			else
#endif
			{
				convert_to_half_float_kernel = kernel_cpu_convert_to_half_float;
			}

			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					convert_to_half_float_kernel(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
						sample_scale, x, y, task.offset, task.stride);
		}
		else {
			void(*convert_to_byte_kernel)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int);
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
			if(system_cpu_support_avx2()) {
				convert_to_byte_kernel = kernel_cpu_avx2_convert_to_byte;
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
			if(system_cpu_support_avx()) {
				convert_to_byte_kernel = kernel_cpu_avx_convert_to_byte;
			}
			else
#endif		
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
			if(system_cpu_support_sse41()) {
				convert_to_byte_kernel = kernel_cpu_sse41_convert_to_byte;
			}
			else
#endif			
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
			if(system_cpu_support_sse3()) {
				convert_to_byte_kernel = kernel_cpu_sse3_convert_to_byte;
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
			if(system_cpu_support_sse2()) {
				convert_to_byte_kernel = kernel_cpu_sse2_convert_to_byte;
			}
			else
#endif
			{
				convert_to_byte_kernel = kernel_cpu_convert_to_byte;
			}

			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					convert_to_byte_kernel(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
						sample_scale, x, y, task.offset, task.stride);

		}
	}

	void thread_shader(DeviceTask& task)
	{
		KernelGlobals kg = kernel_globals;

#ifdef WITH_OSL
		OSLShader::thread_init(&kg, &kernel_globals, &osl_globals);
#endif
		void(*shader_kernel)(KernelGlobals*, uint4*, float4*, float*, int, int, int, int, int);

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			shader_kernel = kernel_cpu_avx2_shader;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			shader_kernel = kernel_cpu_avx_shader;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
		if(system_cpu_support_sse41()) {
			shader_kernel = kernel_cpu_sse41_shader;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			shader_kernel = kernel_cpu_sse3_shader;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			shader_kernel = kernel_cpu_sse2_shader;
		}
		else
#endif
		{
			shader_kernel = kernel_cpu_shader;
		}

		for(int sample = 0; sample < task.num_samples; sample++) {
			for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
				shader_kernel(&kg,
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
