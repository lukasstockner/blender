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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "device_intern.h"

#include "buffers.h"

#include "cuew.h"
#include "util_debug.h"
#include "util_logging.h"
#include "util_map.h"
#include "util_md5.h"
#include "util_opengl.h"
#include "util_path.h"
#include "util_string.h"
#include "util_system.h"
#include "util_types.h"
#include "util_time.h"

/* use feature-adaptive kernel compilation.
 * Requires CUDA toolkit to be installed and currently only works on Linux.
 */
/* #define KERNEL_USE_ADAPTIVE */

CCL_NAMESPACE_BEGIN

#define SPLIT_BLOCK_X 32
#define SPLIT_BLOCK_Y 8
#define PATH_ITER_INC_FACTOR 8

enum CUDAKernel {
	CUDA_KERNEL = 0,
	CUDA_KERNEL_DATA_INIT,
	CUDA_KERNEL_SCENE_INTERSECT,
	CUDA_KERNEL_LAMP_EMISSION,
	CUDA_KERNEL_QUEUE_ENQUEUE,
	CUDA_KERNEL_BACKGROUND_BUFFER,
	CUDA_KERNEL_SHADER_EVAL,
	CUDA_KERNEL_SHADER_STUFF,
	CUDA_KERNEL_DIRECT_LIGHTING,
	CUDA_KERNEL_SHADOW_BLOCKED,
	CUDA_KERNEL_NEXT_ITERATION,
	CUDA_KERNEL_SUM_ALL_RADIANCE,
	CUDA_NUM_KERNELS,
};

string kernel_file_names[] = {
"kernel",
"kernel_data_init",
"kernel_scene_intersect",
"kernel_lamp_emission",
"kernel_queue_enqueue",
"kernel_background_buffer_update",
"kernel_shader_eval",
"kernel_holdout_emission_blurring_pathtermination_ao",
"kernel_direct_lighting",
"kernel_shadow_blocked",
"kernel_next_iteration_setup",
"kernel_sum_all_radiance"};

string kernel_names[] = {
"",
"kernel_cuda_path_trace_data_init",
"kernel_cuda_path_trace_scene_intersect",
"kernel_cuda_path_trace_lamp_emission",
"kernel_cuda_path_trace_queue_enqueue",
"kernel_cuda_path_trace_background_buffer_update",
"kernel_cuda_path_trace_shader_eval",
"kernel_cuda_path_trace_holdout_emission_blurring_pathtermination_ao",
"kernel_cuda_path_trace_direct_lighting",
"kernel_cuda_path_trace_shadow_blocked",
"kernel_cuda_path_trace_next_iteration_setup",
"kernel_cuda_path_trace_sum_all_radiance"};

class CUDASplitDevice : public Device
{
public:
	DedicatedTaskPool task_pool;
	CUdevice cuDevice;
	CUcontext cuContext;
	CUmodule cuModules[CUDA_NUM_KERNELS];
	map<device_ptr, bool> tex_interp_map;
	int cuDevId;
	int cuDevArchitecture;
	bool first_error;
	bool use_texture_storage;
	bool first_tile;

#define SPLIT_BUF(name, type) CUdeviceptr name;
#include "../kernel/split/kernel_split_bufs.h"
	CUdeviceptr Queue_index;
	CUdeviceptr use_queues_flag;
	CUdeviceptr sd;
	CUdeviceptr sd_DL_shadow;
	CUdeviceptr Queue_data;
	CUdeviceptr per_sample_output_buffers;
	int num_path_iteration;

	struct PixelMem {
		GLuint cuPBO;
		CUgraphicsResource cuPBOresource;
		GLuint cuTexId;
		int w, h;
	};

	map<device_ptr, PixelMem> pixel_mem_map;

	CUdeviceptr cuda_device_ptr(device_ptr mem)
	{
		return (CUdeviceptr)mem;
	}

	static bool have_precompiled_kernels()
	{
		string cubins_path = path_get("lib");
		return path_exists(cubins_path);
	}

/*#ifdef NDEBUG
#define cuda_abort()
#else
#define cuda_abort() abort()
#endif*/
	void cuda_error_documentation()
	{
		if(first_error) {
			fprintf(stderr, "\nRefer to the Cycles GPU rendering documentation for possible solutions:\n");
			fprintf(stderr, "http://www.blender.org/manual/render/cycles/gpu_rendering.html\n\n");
			first_error = false;
		}
	}

#define cuda_assert(stmt) \
	{ \
		CUresult result = stmt; \
		\
		if(result != CUDA_SUCCESS) { \
			string message = string_printf("CUDA error: %s in %s", cuewErrorString(result), #stmt); \
			if(error_msg == "") \
				error_msg = message; \
			fprintf(stderr, "%s\n", message.c_str()); \
			/*cuda_abort();*/ \
			cuda_error_documentation(); \
		} \
	} (void)0

	bool cuda_error_(CUresult result, const string& stmt)
	{
		if(result == CUDA_SUCCESS)
			return false;

		string message = string_printf("CUDA error at %s: %s", stmt.c_str(), cuewErrorString(result));
		if(error_msg == "")
			error_msg = message;
		fprintf(stderr, "%s\n", message.c_str());
		cuda_error_documentation();
		return true;
	}

#define cuda_error(stmt) cuda_error_(stmt, #stmt)

	void cuda_error_message(const string& message)
	{
		if(error_msg == "")
			error_msg = message;
		fprintf(stderr, "%s\n", message.c_str());
		cuda_error_documentation();
	}

	void cuda_push_context()
	{
		cuda_assert(cuCtxSetCurrent(cuContext));
	}

	void cuda_pop_context()
	{
		cuda_assert(cuCtxSetCurrent(NULL));
	}

	CUDASplitDevice(DeviceInfo& info, Stats &stats, bool background_)
	: Device(info, stats, background_)
	{
		first_error = true;
		background = background_;
		use_texture_storage = true;

		cuDevId = info.num;
		cuDevice = 0;
		cuContext = 0;

		/* intialize */
		if(cuda_error(cuInit(0)))
			return;

		/* setup device and context */
		if(cuda_error(cuDeviceGet(&cuDevice, cuDevId)))
			return;

		CUresult result;

		if(background) {
			result = cuCtxCreate(&cuContext, 0, cuDevice);
		}
		else {
			result = cuGLCtxCreate(&cuContext, 0, cuDevice);

			if(result != CUDA_SUCCESS) {
				result = cuCtxCreate(&cuContext, 0, cuDevice);
				background = true;
			}
		}

		if(cuda_error_(result, "cuCtxCreate"))
			return;

		int major, minor;
		cuDeviceComputeCapability(&major, &minor, cuDevId);
		cuDevArchitecture = major*100 + minor*10;

		/* In order to use full 6GB of memory on Titan cards, use arrays instead
		 * of textures. On earlier cards this seems slower, but on Titan it is
		 * actually slightly faster in tests. */
		use_texture_storage = (cuDevArchitecture < 300);

		first_tile = true;

#define SPLIT_BUF(name, type) name = (CUdeviceptr)NULL;
#include "../kernel/split/kernel_split_bufs.h"
		Queue_index = (CUdeviceptr)NULL;
		use_queues_flag = (CUdeviceptr)NULL;
		sd = (CUdeviceptr)NULL;
		sd_DL_shadow = (CUdeviceptr)NULL;
		Queue_data = (CUdeviceptr)NULL;
		per_sample_output_buffers = (CUdeviceptr)NULL;
		num_path_iteration = PATH_ITER_INC_FACTOR;

		cuda_pop_context();
	}

	~CUDASplitDevice()
	{
		task_pool.stop();

#define SPLIT_BUF(name, type) if((void*) name) cuda_assert(cuMemFree(name));
#include "../kernel/split/kernel_split_bufs.h"
		if((void*) Queue_index) cuda_assert(cuMemFree(Queue_index));
		if((void*) use_queues_flag) cuda_assert(cuMemFree(use_queues_flag));
		if((void*) sd) cuda_assert(cuMemFree(sd));
		if((void*) sd_DL_shadow) cuda_assert(cuMemFree(sd_DL_shadow));
		if((void*) Queue_data) cuda_assert(cuMemFree(Queue_data));
		if((void*) per_sample_output_buffers) cuda_assert(cuMemFree(per_sample_output_buffers));

		cuda_assert(cuCtxDestroy(cuContext));
	}

	bool support_device(const DeviceRequestedFeatures& /*requested_features*/)
	{
		int major, minor;
		cuDeviceComputeCapability(&major, &minor, cuDevId);

		/* We only support sm_20 and above */
		if(major < 2) {
			cuda_error_message(string_printf("CUDA device supported only with compute capability 2.0 or up, found %d.%d.", major, minor));
			return false;
		}

		return true;
	}

	string compile_kernel(string name, const DeviceRequestedFeatures& requested_features)
	{
		/* compute cubin name */
		int major, minor;
		cuDeviceComputeCapability(&major, &minor, cuDevId);
		string cubin;

		/* attempt to use kernel provided with blender */
		if(requested_features.experimental) {
			cuda_error_message("No experimental support for the split kernel!");
			return "";
		}
		else
			cubin = path_get(string_printf("lib/%s_sm_%d%d.cubin", name.c_str(), major, minor));
		VLOG(1) << "Testing for pre-compiled kernel " << cubin;
		if(path_exists(cubin)) {
			VLOG(1) << "Using precompiled kernel";
			return cubin;
		}
		cuda_error_message(string_printf("No runtime compilation support for the split kernel and %s was not found!", cubin.c_str()));
		return "";
	}

	bool load_kernels(const DeviceRequestedFeatures& requested_features)
	{
		CUresult result = CUDA_SUCCESS;

		/* check if cuda init succeeded */
		if(cuContext == 0)
			return false;

		/* check if GPU is supported */
		if(!support_device(requested_features))
			return false;

		for(int kernel = 0; kernel < CUDA_NUM_KERNELS; kernel++) {
			/* get kernel */
			string cubin = compile_kernel(kernel_file_names[kernel], requested_features);

			if(cubin == "")
				return false;

			/* open module */
			cuda_push_context();

			string cubin_data;

			if(path_read_text(cubin, cubin_data)) {
				if(cuModuleLoadData(&cuModules[kernel], cubin_data.c_str()) != CUDA_SUCCESS)
					result = CUDA_ERROR_FILE_NOT_FOUND; /* TODO */
				//printf("Loaded %s!\n", kernel_file_names[kernel].c_str());
			}
			else
				result = CUDA_ERROR_FILE_NOT_FOUND;

			if(cuda_error_(result, "cuModuleLoad"))
				cuda_error_message(string_printf("Failed loading CUDA kernel %s.", cubin.c_str()));
		}

		cuda_pop_context();

		return (result == CUDA_SUCCESS);
	}

	void mem_alloc(device_memory& mem, MemoryType /*type*/)
	{
		cuda_push_context();
		CUdeviceptr device_pointer;
		size_t size = mem.memory_size();
		cuda_assert(cuMemAlloc(&device_pointer, size));
		mem.device_pointer = (device_ptr)device_pointer;
		mem.device_size = size;
		stats.mem_alloc(size);
		cuda_pop_context();
	}

	void mem_copy_to(device_memory& mem)
	{
		cuda_push_context();
		if(mem.device_pointer)
			cuda_assert(cuMemcpyHtoD(cuda_device_ptr(mem.device_pointer), (void*)mem.data_pointer, mem.memory_size()));
		cuda_pop_context();
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		size_t offset = elem*y*w;
		size_t size = elem*w*h;

		cuda_push_context();
		if(mem.device_pointer) {
			cuda_assert(cuMemcpyDtoH((uchar*)mem.data_pointer + offset,
			                         (CUdeviceptr)(mem.device_pointer + offset), size));
		}
		else {
			memset((char*)mem.data_pointer + offset, 0, size);
		}
		cuda_pop_context();
	}

	void mem_zero(device_memory& mem)
	{
		memset((void*)mem.data_pointer, 0, mem.memory_size());

		cuda_push_context();
		if(mem.device_pointer)
			cuda_assert(cuMemsetD8(cuda_device_ptr(mem.device_pointer), 0, mem.memory_size()));
		cuda_pop_context();
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			cuda_push_context();
			cuda_assert(cuMemFree(cuda_device_ptr(mem.device_pointer)));
			cuda_pop_context();

			mem.device_pointer = 0;

			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		for(int kernel = 0; kernel < CUDA_NUM_KERNELS; kernel++) {
			CUdeviceptr mem;
			size_t bytes;

			cuda_push_context();
			cuda_assert(cuModuleGetGlobal(&mem, &bytes, cuModules[kernel], name));
			//assert(bytes == size);
			cuda_assert(cuMemcpyHtoD(mem, host, size));
			cuda_pop_context();
		}
	}

	void tex_alloc(const char *name,
	               device_memory& mem,
	               InterpolationType interpolation,
	               ExtensionType extension)
	{
		/* todo: support 3D textures, only CPU for now */
		VLOG(1) << "Texture allocate: " << name << ", " << mem.memory_size() << " bytes.";

		/* determine format */
		CUarray_format_enum format;
		size_t dsize = datatype_size(mem.data_type);
		size_t size = mem.memory_size();
		bool use_texture = (interpolation != INTERPOLATION_NONE) || use_texture_storage;

		if(use_texture) {

			switch(mem.data_type) {
				case TYPE_UCHAR: format = CU_AD_FORMAT_UNSIGNED_INT8; break;
				case TYPE_UINT: format = CU_AD_FORMAT_UNSIGNED_INT32; break;
				case TYPE_INT: format = CU_AD_FORMAT_SIGNED_INT32; break;
				case TYPE_FLOAT: format = CU_AD_FORMAT_FLOAT; break;
				default: assert(0); return;
			}

			CUarray handle = NULL;
			if(interpolation != INTERPOLATION_NONE) {
				cuda_push_context();

				CUDA_ARRAY_DESCRIPTOR desc;

				desc.Width = mem.data_width;
				desc.Height = mem.data_height;
				desc.Format = format;
				desc.NumChannels = mem.data_elements;

				cuda_assert(cuArrayCreate(&handle, &desc));

				if(!handle) {
					cuda_pop_context();
					return;
				}

				if(mem.data_height > 1) {
					CUDA_MEMCPY2D param;
					memset(&param, 0, sizeof(param));
					param.dstMemoryType = CU_MEMORYTYPE_ARRAY;
					param.dstArray = handle;
					param.srcMemoryType = CU_MEMORYTYPE_HOST;
					param.srcHost = (void*)mem.data_pointer;
					param.srcPitch = mem.data_width*dsize*mem.data_elements;
					param.WidthInBytes = param.srcPitch;
					param.Height = mem.data_height;

					cuda_assert(cuMemcpy2D(&param));
				}
				else
					cuda_assert(cuMemcpyHtoA(handle, 0, (void*)mem.data_pointer, size));

				cuda_pop_context();
			}
			else {
					mem_alloc(mem, MEM_READ_ONLY);
					mem_copy_to(mem);
			}

			for(int kernel = 0; kernel < CUDA_NUM_KERNELS; kernel++) {
				CUtexref texref = NULL;

				cuda_push_context();
				cuda_assert(cuModuleGetTexRef(&texref, cuModules[kernel], name));

				if(!texref) {
					cuda_pop_context();
					continue;
				}

				if(interpolation != INTERPOLATION_NONE) {
					cuda_assert(cuTexRefSetArray(texref, handle, CU_TRSA_OVERRIDE_FORMAT));

					if(interpolation == INTERPOLATION_CLOSEST) {
						cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_POINT));
					}
					else if(interpolation == INTERPOLATION_LINEAR) {
						cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_LINEAR));
					}
					else {/* CUBIC and SMART are unsupported for CUDA */
						cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_LINEAR));
					}
					cuda_assert(cuTexRefSetFlags(texref, CU_TRSF_NORMALIZED_COORDINATES));

					mem.device_pointer = (device_ptr)handle;
					mem.device_size = size;

					stats.mem_alloc(size);
				}
				else {
					cuda_assert(cuTexRefSetAddress(NULL, texref, cuda_device_ptr(mem.device_pointer), size));
					cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_POINT));
					cuda_assert(cuTexRefSetFlags(texref, CU_TRSF_READ_AS_INTEGER));
				}

				switch(extension) {
					case EXTENSION_REPEAT:
						cuda_assert(cuTexRefSetAddressMode(texref, 0, CU_TR_ADDRESS_MODE_WRAP));
						cuda_assert(cuTexRefSetAddressMode(texref, 1, CU_TR_ADDRESS_MODE_WRAP));
						break;
					case EXTENSION_EXTEND:
						cuda_assert(cuTexRefSetAddressMode(texref, 0, CU_TR_ADDRESS_MODE_CLAMP));
						cuda_assert(cuTexRefSetAddressMode(texref, 1, CU_TR_ADDRESS_MODE_CLAMP));
						break;
					case EXTENSION_CLIP:
						cuda_assert(cuTexRefSetAddressMode(texref, 0, CU_TR_ADDRESS_MODE_BORDER));
						cuda_assert(cuTexRefSetAddressMode(texref, 1, CU_TR_ADDRESS_MODE_BORDER));
						break;
				}
				cuda_assert(cuTexRefSetFormat(texref, format, mem.data_elements));
				cuda_pop_context();
			}
		}
		else {
			mem_alloc(mem, MEM_READ_ONLY);
			mem_copy_to(mem);

			for(int kernel = 0; kernel < CUDA_NUM_KERNELS; kernel++) {
				cuda_push_context();

				CUdeviceptr cumem;
				size_t cubytes;

				cuda_assert(cuModuleGetGlobal(&cumem, &cubytes, cuModules[kernel], name));

				if(cubytes == 8) {
					/* 64 bit device pointer */
					uint64_t ptr = mem.device_pointer;
					cuda_assert(cuMemcpyHtoD(cumem, (void*)&ptr, cubytes));
				}
				else {
					/* 32 bit device pointer */
					uint32_t ptr = (uint32_t)mem.device_pointer;
					cuda_assert(cuMemcpyHtoD(cumem, (void*)&ptr, cubytes));
				}

				cuda_pop_context();
			}
		}

		tex_interp_map[mem.device_pointer] = (interpolation != INTERPOLATION_NONE);
	}

	void tex_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			if(tex_interp_map[mem.device_pointer]) {
				cuda_push_context();
				cuArrayDestroy((CUarray)mem.device_pointer);
				cuda_pop_context();

				tex_interp_map.erase(tex_interp_map.find(mem.device_pointer));
				mem.device_pointer = 0;

				stats.mem_free(mem.device_size);
				mem.device_size = 0;
			}
			else {
				tex_interp_map.erase(tex_interp_map.find(mem.device_pointer));
				mem_free(mem);
			}
		}
	}

	size_t get_shader_closure_size(int max_closure)
	{
		return (sizeof(ShaderClosure) * max_closure);
	}

	/* Returns size of Structure of arrays implementation of. */
	size_t get_shaderdata_soa_size()
	{
		size_t shader_soa_size = 0;

#define SD_VAR(type, what) shader_soa_size += sizeof(void *);
#define SD_CLOSURE_VAR(type, what, max_closure) shader_soa_size += sizeof(void *);
		#include "kernel_shaderdata_vars.h"
#undef SD_VAR
#undef SD_CLOSURE_VAR

		return shader_soa_size;
	}

	void path_trace(RenderTile& rtile, int2 max_render_feasible_tile_size)
	{
		if(have_error())
			return;

		assert(max_render_feasible_tile_size.x % SPLIT_BLOCK_X == 0);
		assert(max_render_feasible_tile_size.y % SPLIT_BLOCK_Y == 0);

		unsigned int size_y = (((rtile.h - 1) / SPLIT_BLOCK_Y) + 1) * SPLIT_BLOCK_Y;
		unsigned int num_threads = max_render_feasible_tile_size.x *
		                           max_render_feasible_tile_size.y;
		unsigned int num_tile_columns_possible = num_threads / size_y;
		/* Estimate number of parallel samples that can be
		 * processed in parallel.
		 */
		unsigned int num_parallel_samples = min(num_tile_columns_possible / rtile.w,
		                                        rtile.num_samples);
		/* Wavefront size in AMD is 64.
		 * TODO(sergey): What about other platforms?
		 */
		if(num_parallel_samples >= SPLIT_BLOCK_X) {
			/* TODO(sergey): Could use generic round-up here. */
			num_parallel_samples = (num_parallel_samples / SPLIT_BLOCK_X) * SPLIT_BLOCK_X;
		}
		assert(num_parallel_samples != 0);

		unsigned int size_x = rtile.w * num_parallel_samples;
		if(size_x*size_y > max_render_feasible_tile_size.x*max_render_feasible_tile_size.y) {
			printf("Size too large!\n");
			return;
		}
		printf("Size %d %d\n", size_x, size_y);

		if(first_tile) {
			cuda_push_context();
			size_t num_global_elements = max_render_feasible_tile_size.x *
			                             max_render_feasible_tile_size.y;
			size_t ShaderClosure_size = get_shader_closure_size(8);
			size_t per_thread_output_buffer_size =	rtile.buffer_size / (rtile.w * rtile.h);

#define SPLIT_BUF(name, type) cuda_assert(cuMemAlloc(&name, num_global_elements*sizeof(type)));
#define SPLIT_BUF2(name, type) cuda_assert(cuMemAlloc(&name, num_global_elements*2*sizeof(type)));
#define SPLIT_BUF_CL(name, type) cuda_assert(cuMemAlloc(&name, num_global_elements*ShaderClosure_size));
#define SPLIT_BUF2_CL(name, type) cuda_assert(cuMemAlloc(&name, num_global_elements*2*ShaderClosure_size));
#include "../kernel/split/kernel_split_bufs.h"
			cuda_assert(cuMemAlloc(&Queue_index, NUM_QUEUES * sizeof(int)));
			cuda_assert(cuMemAlloc(&use_queues_flag, sizeof(char)));
			cuda_assert(cuMemAlloc(&sd, get_shaderdata_soa_size()));
			cuda_assert(cuMemAlloc(&sd_DL_shadow, get_shaderdata_soa_size()));
			cuda_assert(cuMemAlloc(&Queue_data, num_global_elements * (NUM_QUEUES * sizeof(int)+sizeof(int))));
			cuda_assert(cuMemAlloc(&per_sample_output_buffers, num_global_elements * per_thread_output_buffer_size));
			cuda_pop_context();
			first_tile = false;
		}

		cuda_push_context();

		CUfunction cuKernels[CUDA_NUM_KERNELS];
		CUdeviceptr d_buffer = cuda_device_ptr(rtile.buffer);
		CUdeviceptr d_rng_state = cuda_device_ptr(rtile.rng_state);
		int dQueue_size = size_x*size_y;
		int total_num_rays = size_x*size_y;
		int zero = 0;
		int end_sample = rtile.start_sample + rtile.num_samples;

		/* get kernel function */
		for(int kernel = 1; kernel < CUDA_NUM_KERNELS; kernel++) {
			//printf("Loading %s (from file %s)!\n", kernel_names[kernel].c_str(), kernel_file_names[kernel].c_str());
			cuda_assert(cuModuleGetFunction(&cuKernels[kernel], cuModules[kernel], kernel_names[kernel].c_str()));
		}

		if(have_error())
			return;

#ifdef __KERNEL_DEBUG__
#define DEBUGDATA , &debugdata_coop
#else
#define DEBUGDATA
#endif

		void *data_init_args[] =         {&sd, &sd_DL_shadow, &P_sd, &P_sd_DL_shadow, &N_sd, &N_sd_DL_shadow, &Ng_sd, &Ng_sd_DL_shadow, &I_sd, &I_sd_DL_shadow, &shader_sd, &shader_sd_DL_shadow, &flag_sd, &flag_sd_DL_shadow, &prim_sd,
		                                  &prim_sd_DL_shadow, &type_sd, &type_sd_DL_shadow, &u_sd, &u_sd_DL_shadow, &v_sd, &v_sd_DL_shadow, &object_sd, &object_sd_DL_shadow, &time_sd, &time_sd_DL_shadow, &ray_length_sd,
		                                  &ray_length_sd_DL_shadow, &ray_depth_sd, &ray_depth_sd_DL_shadow, &transparent_depth_sd, &transparent_depth_sd_DL_shadow, &dP_sd, &dP_sd_DL_shadow, &dI_sd, &dI_sd_DL_shadow, &du_sd, &du_sd_DL_shadow,
		                                  &dv_sd, &dv_sd_DL_shadow, &dPdu_sd, &dPdu_sd_DL_shadow, &dPdv_sd, &dPdv_sd_DL_shadow, &ob_tfm_sd, &ob_tfm_sd_DL_shadow, &ob_itfm_sd, &ob_itfm_sd_DL_shadow, &closure_sd, &closure_sd_DL_shadow,
		                                  &num_closure_sd, &num_closure_sd_DL_shadow, &randb_closure_sd, &randb_closure_sd_DL_shadow, &ray_P_sd, &ray_P_sd_DL_shadow, &ray_dP_sd, &ray_dP_sd_DL_shadow,
		                                  &per_sample_output_buffers, &d_rng_state, &rng_coop, &throughput_coop, &L_transparent_coop, &PathRadiance_coop, &Ray_coop, &PathState_coop, &ray_state,
		                                  &rtile.start_sample, &rtile.x, &rtile.y, &rtile.w, &rtile.h, &rtile.offset, &rtile.stride, &zero, &zero, &rtile.stride,
		                                  &Queue_data, &Queue_index, &dQueue_size, &use_queues_flag, &work_array, &num_parallel_samples DEBUGDATA};
		void *scene_intersect_args[] =   {&rng_coop, &Ray_coop, &PathState_coop, &Intersection_coop, &ray_state, &rtile.w, &rtile.h, &Queue_data, &Queue_index, &dQueue_size, &use_queues_flag, &num_parallel_samples DEBUGDATA};
		void *lamp_emission_args[] =     {&sd, &throughput_coop, &PathRadiance_coop, &Ray_coop, &PathState_coop, &Intersection_coop, &ray_state, &rtile.w, &rtile.h, &Queue_data, &Queue_index, &dQueue_size, &use_queues_flag, &num_parallel_samples};
		void *queue_enqueue_args[] =     {&Queue_data, &Queue_index, &ray_state, &dQueue_size};
		void *background_buffer_args[] = {&sd, &per_sample_output_buffers, &d_rng_state, &rng_coop, &throughput_coop, &PathRadiance_coop, &Ray_coop, &PathState_coop, &L_transparent_coop, &ray_state, &rtile.w, &rtile.h, &rtile.x, &rtile.y,
		                                  &rtile.stride, &zero, &zero, &rtile.stride, &work_array, &Queue_data, &Queue_index, &dQueue_size, &end_sample, &rtile.start_sample, &num_parallel_samples DEBUGDATA};
		void *shader_eval_args[] =       {&sd, &rng_coop, &Ray_coop, &PathState_coop, &Intersection_coop, &ray_state, &Queue_data, &Queue_index, &dQueue_size};
		void *shader_stuff_args[] =      {&sd, &per_sample_output_buffers, &rng_coop, &throughput_coop, &L_transparent_coop, &PathRadiance_coop, &PathState_coop, &Intersection_coop, &AOAlpha_coop, &AOBSDF_coop, &AOLightRay_coop, &rtile.w, &rtile.h,
		                                  &rtile.x, &rtile.y, &rtile.stride, &ray_state, &work_array, &Queue_data, &Queue_index, &dQueue_size, &num_parallel_samples};
		void *direct_lighting_args[] =   {&sd, &sd_DL_shadow, &rng_coop, &PathState_coop, &ISLamp_coop, &LightRay_coop, &BSDFEval_coop, &ray_state, &Queue_data, &Queue_index, &dQueue_size};
		void *shadow_blocked_args[] =    {&sd_DL_shadow, &PathState_coop, &LightRay_coop, &AOLightRay_coop, &Intersection_coop_AO, &Intersection_coop_DL, &ray_state, &Queue_data, &Queue_index, &dQueue_size, &total_num_rays};
		void *next_iteration_args[] =    {&sd, &rng_coop, &throughput_coop, &PathRadiance_coop, &Ray_coop, &PathState_coop, &LightRay_coop, &ISLamp_coop, &BSDFEval_coop, &AOLightRay_coop, &AOBSDF_coop, &AOAlpha_coop, &ray_state,
		                                  &Queue_data, &Queue_index, &dQueue_size, &use_queues_flag};
		void *sum_all_radiance_args[] =  {&d_buffer, &per_sample_output_buffers, &num_parallel_samples, &rtile.w, &rtile.h, &rtile.stride, &zero, &zero, &rtile.stride, &rtile.start_sample};

		cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_DATA_INIT], size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, data_init_args, 0));
			cuda_assert(cuCtxSynchronize());
		bool activeRaysAvailable = true;
		unsigned int numHostIntervention = 0;
		unsigned int numNextPathIterTimes = num_path_iteration;
		char *host_ray_state = new char[size_x*size_y];
		while(activeRaysAvailable) {
			printf("Starting pass!\n");
			for(int iter = 0; iter < num_path_iteration; iter++) {
			printf("Starting iter!\n");
				cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_SCENE_INTERSECT], size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, scene_intersect_args, 0));
				cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_LAMP_EMISSION], size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, lamp_emission_args, 0));
				cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_QUEUE_ENQUEUE], size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, queue_enqueue_args, 0));
				cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_BACKGROUND_BUFFER], size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, background_buffer_args, 0));
				cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_SHADER_EVAL], size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, shader_eval_args, 0));
				cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_SHADER_STUFF], size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, shader_stuff_args, 0));
				cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_DIRECT_LIGHTING], size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, direct_lighting_args, 0));
				cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_SHADOW_BLOCKED], 2*size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, shadow_blocked_args, 0));
				cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_NEXT_ITERATION], size_x/SPLIT_BLOCK_X, size_y/SPLIT_BLOCK_Y, 1, SPLIT_BLOCK_X, SPLIT_BLOCK_Y, 1, 0, 0, next_iteration_args, 0));
				cuda_assert(cuCtxSynchronize());
			}
			cuda_assert(cuCtxSynchronize());
			cuda_assert(cuMemcpyDtoH(host_ray_state, ray_state, size_x*size_y*sizeof(char)));
			activeRaysAvailable = false;

			for(int ray = 0; ray < size_x*size_y; ray++) {
				if(int8_t(host_ray_state[ray]) != RAY_INACTIVE) {
					activeRaysAvailable = true;
					break;
				}
			}

			if(activeRaysAvailable) {
				numHostIntervention++;
				num_path_iteration = PATH_ITER_INC_FACTOR;
				numNextPathIterTimes += PATH_ITER_INC_FACTOR;
			}

			if(have_error())
				return;
		}

		cuda_assert(cuLaunchKernel(cuKernels[CUDA_KERNEL_SUM_ALL_RADIANCE], ((rtile.w - 1) / 16) + 1, ((rtile.h - 1) / 16) + 1, 1, 16, 16, 1, 0, 0, sum_all_radiance_args, 0));
		cuda_assert(cuCtxSynchronize());

		delete[] host_ray_state;
		if(numHostIntervention == 0) {
			num_path_iteration = ((numNextPathIterTimes - PATH_ITER_INC_FACTOR) <= 0) ?	PATH_ITER_INC_FACTOR : numNextPathIterTimes - PATH_ITER_INC_FACTOR;
		}
		else {
			num_path_iteration = numNextPathIterTimes;
		}

#if 0
		/* pass in parameters */
		void *args[] = {&d_buffer,
						 &d_rng_state,
						 &sample,
						 &rtile.x,
						 &rtile.y,
						 &rtile.w,
						 &rtile.h,
						 &rtile.offset,
						 &rtile.stride};

		/* launch kernel */
		int threads_per_block;
		cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuPathTrace));

		/*int num_registers;
		cuda_assert(cuFuncGetAttribute(&num_registers, CU_FUNC_ATTRIBUTE_NUM_REGS, cuPathTrace));

		printf("threads_per_block %d\n", threads_per_block);
		printf("num_registers %d\n", num_registers);*/

		int xthreads = (int)sqrt((float)threads_per_block);
		int ythreads = (int)sqrt((float)threads_per_block);
		int xblocks = (rtile.w + xthreads - 1)/xthreads;
		int yblocks = (rtile.h + ythreads - 1)/ythreads;

		cuda_assert(cuFuncSetCacheConfig(cuPathTrace, CU_FUNC_CACHE_PREFER_L1));

		cuda_assert(cuLaunchKernel(cuPathTrace,
								   xblocks , yblocks, 1, /* blocks */
								   xthreads, ythreads, 1, /* threads */
								   0, 0, args, 0));

		cuda_assert(cuCtxSynchronize());
#endif
		cuda_pop_context();
	}

	void film_convert(DeviceTask& task, device_ptr buffer, device_ptr rgba_byte, device_ptr rgba_half)
	{
		if(have_error())
			return;

		cuda_push_context();

		CUfunction cuFilmConvert;
		CUdeviceptr d_rgba = map_pixels((rgba_byte)? rgba_byte: rgba_half);
		CUdeviceptr d_buffer = cuda_device_ptr(buffer);

		/* get kernel function */
		if(rgba_half) {
			cuda_assert(cuModuleGetFunction(&cuFilmConvert, cuModules[CUDA_KERNEL], "kernel_cuda_convert_to_half_float"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuFilmConvert, cuModules[CUDA_KERNEL], "kernel_cuda_convert_to_byte"));
		}


		float sample_scale = 1.0f/(task.sample + 1);

		/* pass in parameters */
		void *args[] = {&d_rgba,
						 &d_buffer,
						 &sample_scale,
						 &task.x,
						 &task.y,
						 &task.w,
						 &task.h,
						 &task.offset,
						 &task.stride};

		/* launch kernel */
		int threads_per_block;
		cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuFilmConvert));

		int xthreads = (int)sqrt((float)threads_per_block);
		int ythreads = (int)sqrt((float)threads_per_block);
		int xblocks = (task.w + xthreads - 1)/xthreads;
		int yblocks = (task.h + ythreads - 1)/ythreads;

		cuda_assert(cuFuncSetCacheConfig(cuFilmConvert, CU_FUNC_CACHE_PREFER_L1));

		cuda_assert(cuLaunchKernel(cuFilmConvert,
								   xblocks , yblocks, 1, /* blocks */
								   xthreads, ythreads, 1, /* threads */
								   0, 0, args, 0));

		unmap_pixels((rgba_byte)? rgba_byte: rgba_half);

		cuda_pop_context();
	}

	void shader(DeviceTask& task)
	{
		if(have_error())
			return;

		cuda_push_context();

		CUfunction cuShader;
		CUdeviceptr d_input = cuda_device_ptr(task.shader_input);
		CUdeviceptr d_output = cuda_device_ptr(task.shader_output);
		CUdeviceptr d_output_luma = cuda_device_ptr(task.shader_output_luma);

		/* get kernel function */
		if(task.shader_eval_type >= SHADER_EVAL_BAKE) {
			cuda_assert(cuModuleGetFunction(&cuShader, cuModules[CUDA_KERNEL], "kernel_cuda_bake"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuShader, cuModules[CUDA_KERNEL], "kernel_cuda_shader"));
		}

		/* do tasks in smaller chunks, so we can cancel it */
		const int shader_chunk_size = 65536;
		const int start = task.shader_x;
		const int end = task.shader_x + task.shader_w;
		int offset = task.offset;

		bool canceled = false;
		for(int sample = 0; sample < task.num_samples && !canceled; sample++) {
			for(int shader_x = start; shader_x < end; shader_x += shader_chunk_size) {
				int shader_w = min(shader_chunk_size, end - shader_x);

				/* pass in parameters */
				void *args[8];
				int arg = 0;
				args[arg++] = &d_input;
				args[arg++] = &d_output;
				if(task.shader_eval_type < SHADER_EVAL_BAKE) {
					args[arg++] = &d_output_luma;
				}
				args[arg++] = &task.shader_eval_type;
				args[arg++] = &shader_x;
				args[arg++] = &shader_w;
				args[arg++] = &offset;
				args[arg++] = &sample;

				/* launch kernel */
				int threads_per_block;
				cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuShader));

				int xblocks = (shader_w + threads_per_block - 1)/threads_per_block;

				cuda_assert(cuFuncSetCacheConfig(cuShader, CU_FUNC_CACHE_PREFER_L1));
				cuda_assert(cuLaunchKernel(cuShader,
										   xblocks , 1, 1, /* blocks */
										   threads_per_block, 1, 1, /* threads */
										   0, 0, args, 0));

				cuda_assert(cuCtxSynchronize());

				if(task.get_cancel()) {
					canceled = false;
					break;
				}
			}

			task.update_progress(NULL);
		}

		cuda_pop_context();
	}

	CUdeviceptr map_pixels(device_ptr mem)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem];
			CUdeviceptr buffer;
			
			size_t bytes;
			cuda_assert(cuGraphicsMapResources(1, &pmem.cuPBOresource, 0));
			cuda_assert(cuGraphicsResourceGetMappedPointer(&buffer, &bytes, pmem.cuPBOresource));
			
			return buffer;
		}

		return cuda_device_ptr(mem);
	}

	void unmap_pixels(device_ptr mem)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem];

			cuda_assert(cuGraphicsUnmapResources(1, &pmem.cuPBOresource, 0));
		}
	}

	void pixels_alloc(device_memory& mem)
	{
		if(!background) {
			PixelMem pmem;

			pmem.w = mem.data_width;
			pmem.h = mem.data_height;

			cuda_push_context();

			glGenBuffers(1, &pmem.cuPBO);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
			if(mem.data_type == TYPE_HALF)
				glBufferData(GL_PIXEL_UNPACK_BUFFER, pmem.w*pmem.h*sizeof(GLhalf)*4, NULL, GL_DYNAMIC_DRAW);
			else
				glBufferData(GL_PIXEL_UNPACK_BUFFER, pmem.w*pmem.h*sizeof(uint8_t)*4, NULL, GL_DYNAMIC_DRAW);
			
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			
			glGenTextures(1, &pmem.cuTexId);
			glBindTexture(GL_TEXTURE_2D, pmem.cuTexId);
			if(mem.data_type == TYPE_HALF)
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, pmem.w, pmem.h, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pmem.w, pmem.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);
			
			CUresult result = cuGraphicsGLRegisterBuffer(&pmem.cuPBOresource, pmem.cuPBO, CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);

			if(result == CUDA_SUCCESS) {
				cuda_pop_context();

				mem.device_pointer = pmem.cuTexId;
				pixel_mem_map[mem.device_pointer] = pmem;

				mem.device_size = mem.memory_size();
				stats.mem_alloc(mem.device_size);

				return;
			}
			else {
				/* failed to register buffer, fallback to no interop */
				glDeleteBuffers(1, &pmem.cuPBO);
				glDeleteTextures(1, &pmem.cuTexId);

				cuda_pop_context();

				background = true;
			}
		}

		Device::pixels_alloc(mem);
	}

	void pixels_copy_from(device_memory& mem, int y, int w, int h)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem.device_pointer];

			cuda_push_context();

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
			uchar *pixels = (uchar*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_ONLY);
			size_t offset = sizeof(uchar)*4*y*w;
			memcpy((uchar*)mem.data_pointer + offset, pixels + offset, sizeof(uchar)*4*w*h);
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

			cuda_pop_context();

			return;
		}

		Device::pixels_copy_from(mem, y, w, h);
	}

	void pixels_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			if(!background) {
				PixelMem pmem = pixel_mem_map[mem.device_pointer];

				cuda_push_context();

				cuda_assert(cuGraphicsUnregisterResource(pmem.cuPBOresource));
				glDeleteBuffers(1, &pmem.cuPBO);
				glDeleteTextures(1, &pmem.cuTexId);

				cuda_pop_context();

				pixel_mem_map.erase(pixel_mem_map.find(mem.device_pointer));
				mem.device_pointer = 0;

				stats.mem_free(mem.device_size);
				mem.device_size = 0;

				return;
			}

			Device::pixels_free(mem);
		}
	}

	void draw_pixels(device_memory& mem, int y, int w, int h, int dx, int dy, int width, int height, bool transparent,
		const DeviceDrawParams &draw_params)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem.device_pointer];
			float *vpointer;

			cuda_push_context();

			/* for multi devices, this assumes the inefficient method that we allocate
			 * all pixels on the device even though we only render to a subset */
			size_t offset = 4*y*w;

			if(mem.data_type == TYPE_HALF)
				offset *= sizeof(GLhalf);
			else
				offset *= sizeof(uint8_t);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
			glBindTexture(GL_TEXTURE_2D, pmem.cuTexId);
			if(mem.data_type == TYPE_HALF)
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_HALF_FLOAT, (void*)offset);
			else
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)offset);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			
			glEnable(GL_TEXTURE_2D);
			
			if(transparent) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}

			glColor3f(1.0f, 1.0f, 1.0f);

			if(draw_params.bind_display_space_shader_cb) {
				draw_params.bind_display_space_shader_cb();
			}

			if(!vertex_buffer)
				glGenBuffers(1, &vertex_buffer);

			glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
			/* invalidate old contents - avoids stalling if buffer is still waiting in queue to be rendered */
			glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);

			vpointer = (float *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

			if(vpointer) {
				/* texture coordinate - vertex pair */
				vpointer[0] = 0.0f;
				vpointer[1] = 0.0f;
				vpointer[2] = dx;
				vpointer[3] = dy;

				vpointer[4] = (float)w/(float)pmem.w;
				vpointer[5] = 0.0f;
				vpointer[6] = (float)width + dx;
				vpointer[7] = dy;

				vpointer[8] = (float)w/(float)pmem.w;
				vpointer[9] = (float)h/(float)pmem.h;
				vpointer[10] = (float)width + dx;
				vpointer[11] = (float)height + dy;

				vpointer[12] = 0.0f;
				vpointer[13] = (float)h/(float)pmem.h;
				vpointer[14] = dx;
				vpointer[15] = (float)height + dy;

				glUnmapBuffer(GL_ARRAY_BUFFER);
			}

			glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), 0);
			glVertexPointer(2, GL_FLOAT, 4 * sizeof(float), (char *)NULL + 2 * sizeof(float));

			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);

			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_VERTEX_ARRAY);

			glBindBuffer(GL_ARRAY_BUFFER, 0);

			if(draw_params.unbind_display_space_shader_cb) {
				draw_params.unbind_display_space_shader_cb();
			}

			if(transparent)
				glDisable(GL_BLEND);
			
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);

			cuda_pop_context();

			return;
		}

		Device::draw_pixels(mem, y, w, h, dx, dy, width, height, transparent, draw_params);
	}

	void thread_run(DeviceTask *task)
	{
		if(task->type == DeviceTask::PATH_TRACE) {
			RenderTile tile;
			
			bool branched = task->integrator_branched;
			
			/* keep rendering tiles until done */
			while(task->acquire_tile(this, tile)) {
				/*int start_sample = tile.start_sample;
				int end_sample = tile.start_sample + tile.num_samples;

				for(int sample = start_sample; sample < end_sample; sample++) {
					if(task->get_cancel()) {
						if(task->need_finish_queue == false)
							break;
					}

					path_trace(tile, sample, branched);

					tile.sample = sample + 1;

					task->update_progress(&tile);
				}*/
				path_trace(tile, make_int2(1280, 256));

				tile.sample = tile.start_sample + tile.num_samples;

				task->release_tile(tile);
			}
		}
		else if(task->type == DeviceTask::SHADER) {
			shader(*task);

			cuda_push_context();
			cuda_assert(cuCtxSynchronize());
			cuda_pop_context();
		}
	}

	class CUDASplitDeviceTask : public DeviceTask {
	public:
		CUDASplitDeviceTask(CUDASplitDevice *device, DeviceTask& task)
		: DeviceTask(task)
		{
			run = function_bind(&CUDASplitDevice::thread_run, device, this);
		}
	};

	int get_split_task_count(DeviceTask& /*task*/)
	{
		return 1;
	}

	void task_add(DeviceTask& task)
	{
		if(task.type == DeviceTask::FILM_CONVERT) {
			/* must be done in main thread due to opengl access */
			film_convert(task, task.buffer, task.rgba_byte, task.rgba_half);

			cuda_push_context();
			cuda_assert(cuCtxSynchronize());
			cuda_pop_context();
		}
		else {
			task_pool.push(new CUDASplitDeviceTask(this, task));
		}
	}

	void task_wait()
	{
		task_pool.wait();
	}

	void task_cancel()
	{
		task_pool.cancel();
	}
};

bool device_cuda_split_init(void)
{
	return true;
	static bool initialized = false;
	static bool result = false;

	if(initialized)
		return result;

	initialized = true;
	int cuew_result = cuewInit();
	if(cuew_result == CUEW_SUCCESS) {
		VLOG(1) << "CUEW initialization succeeded";
		result = true;
	}
	else {
		VLOG(1) << "CUEW initialization failed: "
		        << ((cuew_result == CUEW_ERROR_ATEXIT_FAILED)
		            ? "Error setting up atexit() handler"
		            : "Error opening the library");
	}

	return result;
}

Device *device_cuda_split_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new CUDASplitDevice(info, stats, background);
}

void device_cuda_split_info(vector<DeviceInfo>& devices)
{
	CUresult result;
	int count = 0;

	result = cuInit(0);
	if(result != CUDA_SUCCESS) {
		if(result != CUDA_ERROR_NO_DEVICE)
			fprintf(stderr, "CUDA cuInit: %s\n", cuewErrorString(result));
		return;
	}

	result = cuDeviceGetCount(&count);
	if(result != CUDA_SUCCESS) {
		fprintf(stderr, "CUDA cuDeviceGetCount: %s\n", cuewErrorString(result));
		return;
	}
	
	vector<DeviceInfo> display_devices;

	for(int num = 0; num < count; num++) {
		char name[256];
		int attr;

		if(cuDeviceGetName(name, 256, num) != CUDA_SUCCESS)
			continue;

		int major, minor;
		cuDeviceComputeCapability(&major, &minor, num);
		if(major < 2) {
			continue;
		}

		DeviceInfo info;

		info.type = DEVICE_CUDA_SPLIT;
		info.description = string(name);
		info.id = string_printf("CUDA_%d", num);
		info.num = num;

		info.advanced_shading = (major >= 2);
		info.extended_images = (major >= 3);
		info.pack_images = false;

		/* if device has a kernel timeout, assume it is used for display */
		if(cuDeviceGetAttribute(&attr, CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT, num) == CUDA_SUCCESS && attr == 1) {
			info.display_device = true;
			display_devices.push_back(info);
		}
		else
			devices.push_back(info);
	}

	if(!display_devices.empty())
		devices.insert(devices.end(), display_devices.begin(), display_devices.end());
}

CCL_NAMESPACE_END
