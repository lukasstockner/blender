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

#ifdef WITH_OPENCL

#include "opencl.h"

#include "buffers.h"

#include "kernel_types.h"

#include "util_md5.h"
#include "util_path.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

class OpenCLDeviceMegaKernel : public OpenCLDeviceBase
{
public:
	cl_kernel ckPathTraceKernel;
	cl_program path_trace_program;

	OpenCLDeviceMegaKernel(DeviceInfo& info, Stats &stats, bool background_)
	: OpenCLDeviceBase(info, stats, background_)
	{
		ckPathTraceKernel = NULL;
		path_trace_program = NULL;
	}

	bool load_kernels(const DeviceRequestedFeatures& requested_features)
	{
		/* Get Shader, bake and film convert kernels.
		 * It'll also do verification of OpenCL actually initialized.
		 */
		if(!OpenCLDeviceBase::load_kernels(requested_features)) {
			return false;
		}

		/* Try to use cached kernel. */
		thread_scoped_lock cache_locker;
		path_trace_program = OpenCLCache::get_program(cpPlatform,
		                                              cdDevice,
		                                              OpenCLCache::OCL_DEV_MEGAKERNEL_PROGRAM,
		                                              cache_locker);

		if(!path_trace_program) {
			/* Verify we have right opencl version. */
			if(!opencl_version_check())
				return false;

			/* Calculate md5 hash to detect changes. */
			string kernel_path = path_get("kernel");
			string kernel_md5 = path_files_md5_hash(kernel_path);
			string custom_kernel_build_options = "-D__COMPILE_ONLY_MEGAKERNEL__ ";
			string device_md5 = device_md5_hash(custom_kernel_build_options);

			/* Path to cached binary. */
			string clbin = string_printf("cycles_kernel_%s_%s.clbin",
			                             device_md5.c_str(),
			                             kernel_md5.c_str());
			clbin = path_cache_get(path_join("kernels", clbin));

			/* Path to preprocessed source for debugging. */
			string clsrc, *debug_src = NULL;
			if(opencl_kernel_use_debug()) {
				clsrc = string_printf("cycles_kernel_%s_%s.cl",
				                      device_md5.c_str(),
				                      kernel_md5.c_str());
				clsrc = path_cache_get(path_join("kernels", clsrc));
				debug_src = &clsrc;
			}

			/* If exists already, try use it. */
			if(path_exists(clbin) && load_binary(kernel_path,
			                                     clbin,
			                                     custom_kernel_build_options,
			                                     &path_trace_program,
			                                     debug_src))
			{
				/* Kernel loaded from binary, nothing to do. */
			}
			else {
				string init_kernel_source = "#include \"kernels/opencl/kernel.cl\" // " +
				                            kernel_md5 + "\n";
				/* If does not exist or loading binary failed, compile kernel. */
				if(!compile_kernel("mega_kernel",
				                   kernel_path,
				                   init_kernel_source,
				                   custom_kernel_build_options,
				                   &path_trace_program,
				                   debug_src))
				{
					return false;
				}
				/* Save binary for reuse. */
				if(!save_binary(&path_trace_program, clbin)) {
					return false;
				}
			}
			/* Cache the program. */
			OpenCLCache::store_program(cpPlatform,
			                           cdDevice,
			                           path_trace_program,
			                           OpenCLCache::OCL_DEV_MEGAKERNEL_PROGRAM,
			                           cache_locker);
		}

		/* Find kernels. */
		ckPathTraceKernel = clCreateKernel(path_trace_program,
		                                   "kernel_ocl_path_trace",
		                                   &ciErr);
		if(opencl_error(ciErr))
			return false;
		return true;
	}

	~OpenCLDeviceMegaKernel()
	{
		task_pool.stop();
		release_kernel_safe(ckPathTraceKernel);
		release_program_safe(path_trace_program);
	}

	void path_trace(RenderTile& rtile, int sample)
	{
		/* Cast arguments to cl types. */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_buffer = CL_MEM_PTR(rtile.buffer);
		cl_mem d_rng_state = CL_MEM_PTR(rtile.rng_state);
		cl_int d_x = rtile.x;
		cl_int d_y = rtile.y;
		cl_int d_w = rtile.w;
		cl_int d_h = rtile.h;
		cl_int d_offset = rtile.offset;
		cl_int d_stride = rtile.stride;

		/* Sample arguments. */
		cl_int d_sample = sample;

		cl_uint start_arg_index =
			kernel_set_args(ckPathTraceKernel,
			                0,
			                d_data,
			                d_buffer,
			                d_rng_state);

#define KERNEL_TEX(type, ttype, name) \
		set_kernel_arg_mem(ckPathTraceKernel, &start_arg_index, #name);
#include "kernel_textures.h"
#undef KERNEL_TEX

		start_arg_index += kernel_set_args(ckPathTraceKernel,
		                                   start_arg_index,
		                                   d_sample,
		                                   d_x,
		                                   d_y,
		                                   d_w,
		                                   d_h,
		                                   d_offset,
		                                   d_stride);

		enqueue_kernel(ckPathTraceKernel, d_w, d_h);
	}

	void thread_run(DeviceTask *task)
	{
		if(task->type == DeviceTask::FILM_CONVERT) {
			film_convert(*task, task->buffer, task->rgba_byte, task->rgba_half);
		}
		else if(task->type == DeviceTask::SHADER) {
			shader(*task);
		}
		else if(task->type == DeviceTask::PATH_TRACE) {
			RenderTile tile;
			/* Keep rendering tiles until done. */
			while(task->acquire_tile(this, tile)) {
				int start_sample = tile.start_sample;
				int end_sample = tile.start_sample + tile.num_samples;

				for(int sample = start_sample; sample < end_sample; sample++) {
					if(task->get_cancel()) {
						if(task->need_finish_queue == false)
							break;
					}

					path_trace(tile, sample);

					tile.sample = sample + 1;

					task->update_progress(&tile);
				}

				/* Complete kernel execution before release tile */
				/* This helps in multi-device render;
				 * The device that reaches the critical-section function
				 * release_tile waits (stalling other devices from entering
				 * release_tile) for all kernels to complete. If device1 (a
				 * slow-render device) reaches release_tile first then it would
				 * stall device2 (a fast-render device) from proceeding to render
				 * next tile.
				 */
				clFinish(cqCommandQueue);

				task->release_tile(tile);
			}
		}
	}
};

Device* opencl_create_mega_device(DeviceInfo& info, Stats &stats, bool background)
{
	return new OpenCLDeviceMegaKernel(info, stats, background);
}

CCL_NAMESPACE_END

#endif
