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

#include <climits>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "device_intern.h"

#include "buffers.h"

#ifdef WITH_CUDA_DYNLOAD
#  include "cuew.h"
#else
#  include "util_opengl.h"
#  include <cuda.h>
#  include <cudaGL.h>
#endif
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

CCL_NAMESPACE_BEGIN

#ifndef WITH_CUDA_DYNLOAD

/* Transparently implement some functions, so majority of the file does not need
 * to worry about difference between dynamically loaded and linked CUDA at all.
 */

namespace {

const char *cuewErrorString(CUresult result)
{
	/* We can only give error code here without major code duplication, that
	 * should be enough since dynamic loading is only being disabled by folks
	 * who knows what they're doing anyway.
	 *
	 * NOTE: Avoid call from several threads.
	 */
	static string error;
	error = string_printf("%d", result);
	return error.c_str();
}

const char *cuewCompilerPath(void)
{
	return CYCLES_CUDA_NVCC_EXECUTABLE;
}

int cuewCompilerVersion(void)
{
	return (CUDA_VERSION / 100) + (CUDA_VERSION % 100 / 10);
}

}  /* namespace */
#endif  /* WITH_CUDA_DYNLOAD */

class CUDADevice : public Device
{
public:
	DedicatedTaskPool task_pool;
	CUdevice cuDevice;
	CUcontext cuContext;
	CUmodule cuModule;
	map<device_ptr, bool> tex_interp_map;
	map<device_ptr, uint> tex_bindless_map;
	int cuDevId;
	int cuDevArchitecture;
	bool first_error;
	KernelData kernel_globals;

	struct PixelMem {
		GLuint cuPBO;
		CUgraphicsResource cuPBOresource;
		GLuint cuTexId;
		int w, h;
	};

	map<device_ptr, PixelMem> pixel_mem_map;

	/* Bindless Textures */
	device_vector<uint> bindless_mapping;
	bool need_bindless_mapping;

	CUdeviceptr cuda_device_ptr(device_ptr mem)
	{
		return (CUdeviceptr)mem;
	}

	static bool have_precompiled_kernels()
	{
		string cubins_path = path_get("lib");
		return path_exists(cubins_path);
	}

	virtual bool show_samples() const
	{
		/* The CUDADevice only processes one tile at a time, so showing samples is fine. */
		return true;
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

	CUDADevice(DeviceInfo& info, Stats &stats, bool background_)
	: Device(info, stats, background_)
	{
		first_error = true;
		background = background_;

		cuDevId = info.num;
		cuDevice = 0;
		cuContext = 0;

		need_bindless_mapping = false;

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
		cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevId);
		cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevId);
		cuDevArchitecture = major*100 + minor*10;

		cuda_pop_context();
	}

	~CUDADevice()
	{
		task_pool.stop();

		if(info.has_bindless_textures) {
			tex_free(bindless_mapping);
		}

		cuda_assert(cuCtxDestroy(cuContext));
	}

	bool support_device(const DeviceRequestedFeatures& /*requested_features*/)
	{
		int major, minor;
		cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevId);
		cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevId);

		/* We only support sm_20 and above */
		if(major < 2) {
			cuda_error_message(string_printf("CUDA device supported only with compute capability 2.0 or up, found %d.%d.", major, minor));
			return false;
		}

		return true;
	}

	bool use_adaptive_compilation()
	{
		return DebugFlags().cuda.adaptive_compile;
	}

	/* Common NVCC flags which stays the same regardless of shading model,
	 * kernel sources md5 and only depends on compiler or compilation settings.
	 */
	string compile_kernel_get_common_cflags(
	        const DeviceRequestedFeatures& requested_features)
	{
		const int cuda_version = cuewCompilerVersion();
		const int machine = system_cpu_bits();
		const string kernel_path = path_get("kernel");
		const string include = kernel_path;
		string cflags = string_printf("-m%d "
		                              "--ptxas-options=\"-v\" "
		                              "--use_fast_math "
		                              "-DNVCC "
		                              "-D__KERNEL_CUDA_VERSION__=%d "
		                               "-I\"%s\"",
		                              machine,
		                              cuda_version,
		                              include.c_str());
		if(use_adaptive_compilation()) {
			cflags += " " + requested_features.get_build_options();
		}
		const char *extra_cflags = getenv("CYCLES_CUDA_EXTRA_CFLAGS");
		if(extra_cflags) {
			cflags += string(" ") + string(extra_cflags);
		}
#ifdef WITH_CYCLES_DEBUG
		cflags += " -D__KERNEL_DEBUG__";
#endif
		return cflags;
	}

	bool compile_check_compiler() {
		const char *nvcc = cuewCompilerPath();
		if(nvcc == NULL) {
			cuda_error_message("CUDA nvcc compiler not found. "
			                   "Install CUDA toolkit in default location.");
			return false;
		}
		const int cuda_version = cuewCompilerVersion();
		VLOG(1) << "Found nvcc " << nvcc
		        << ", CUDA version " << cuda_version
		        << ".";
		const int major = cuda_version / 10, minor = cuda_version & 10;
		if(cuda_version == 0) {
			cuda_error_message("CUDA nvcc compiler version could not be parsed.");
			return false;
		}
		if(cuda_version < 75) {
			printf("Unsupported CUDA version %d.%d detected, "
			       "you need CUDA 7.5 or newer.\n",
			       major, minor);
			return false;
		}
		else if(cuda_version != 75 && cuda_version != 80) {
			printf("CUDA version %d.%d detected, build may succeed but only "
			       "CUDA 7.5 and 8.0 are officially supported.\n",
			       major, minor);
		}
		return true;
	}

	string compile_kernel(const DeviceRequestedFeatures& requested_features)
	{
		/* Compute cubin name. */
		int major, minor;
		cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevId);
		cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevId);

		/* Attempt to use kernel provided with Blender. */
		if(!use_adaptive_compilation()) {
			const string cubin = path_get(string_printf("lib/kernel_sm_%d%d.cubin",
			                                            major, minor));
			VLOG(1) << "Testing for pre-compiled kernel " << cubin << ".";
			if(path_exists(cubin)) {
				VLOG(1) << "Using precompiled kernel.";
				return cubin;
			}
		}

		const string common_cflags =
		        compile_kernel_get_common_cflags(requested_features);

		/* Try to use locally compiled kernel. */
		const string kernel_path = path_get("kernel");
		const string kernel_md5 = path_files_md5_hash(kernel_path);

		/* We include cflags into md5 so changing cuda toolkit or changing other
		 * compiler command line arguments makes sure cubin gets re-built.
		 */
		const string cubin_md5 = util_md5_string(kernel_md5 + common_cflags);

		const string cubin_file = string_printf("cycles_kernel_sm%d%d_%s.cubin",
		                                        major, minor,
		                                        cubin_md5.c_str());
		const string cubin = path_cache_get(path_join("kernels", cubin_file));
		VLOG(1) << "Testing for locally compiled kernel " << cubin << ".";
		if(path_exists(cubin)) {
			VLOG(1) << "Using locally compiled kernel.";
			return cubin;
		}

#ifdef _WIN32
		if(have_precompiled_kernels()) {
			if(major < 2) {
				cuda_error_message(string_printf(
				        "CUDA device requires compute capability 2.0 or up, "
				        "found %d.%d. Your GPU is not supported.",
				        major, minor));
			}
			else {
				cuda_error_message(string_printf(
				        "CUDA binary kernel for this graphics card compute "
				        "capability (%d.%d) not found.",
				        major, minor));
			}
			return "";
		}
#endif

		/* Compile. */
		if(!compile_check_compiler()) {
			return "";
		}
		const char *nvcc = cuewCompilerPath();
		const string kernel = path_join(kernel_path,
		                          path_join("kernels",
		                                    path_join("cuda", "kernel.cu")));
		double starttime = time_dt();
		printf("Compiling CUDA kernel ...\n");

		path_create_directories(cubin);

		string command = string_printf("\"%s\" "
		                               "-arch=sm_%d%d "
		                               "--cubin \"%s\" "
		                               "-o \"%s\" "
		                               "%s ",
		                               nvcc,
		                               major, minor,
		                               kernel.c_str(),
		                               cubin.c_str(),
		                               common_cflags.c_str());

		printf("%s\n", command.c_str());

		if(system(command.c_str()) == -1) {
			cuda_error_message("Failed to execute compilation command, "
			                   "see console for details.");
			return "";
		}

		/* Verify if compilation succeeded */
		if(!path_exists(cubin)) {
			cuda_error_message("CUDA kernel compilation failed, "
			                   "see console for details.");
			return "";
		}

		printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

		return cubin;
	}

	bool load_kernels(const DeviceRequestedFeatures& requested_features)
	{
		/* check if cuda init succeeded */
		if(cuContext == 0)
			return false;

		/* check if GPU is supported */
		if(!support_device(requested_features))
			return false;

		/* get kernel */
		string cubin = compile_kernel(requested_features);

		if(cubin == "")
			return false;

		/* open module */
		cuda_push_context();

		string cubin_data;
		CUresult result;

		if(path_read_text(cubin, cubin_data))
			result = cuModuleLoadData(&cuModule, cubin_data.c_str());
		else
			result = CUDA_ERROR_FILE_NOT_FOUND;

		if(cuda_error_(result, "cuModuleLoad"))
			cuda_error_message(string_printf("Failed loading CUDA kernel %s.", cubin.c_str()));

		cuda_pop_context();

		return (result == CUDA_SUCCESS);
	}

	void load_bindless_mapping()
	{
		if(info.has_bindless_textures && need_bindless_mapping) {
			tex_free(bindless_mapping);
			tex_alloc("__bindless_mapping", bindless_mapping, INTERPOLATION_NONE, EXTENSION_REPEAT);
			need_bindless_mapping = false;
		}
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
		CUdeviceptr mem;
		size_t bytes;

		cuda_push_context();
		cuda_assert(cuModuleGetGlobal(&mem, &bytes, cuModule, name));
		if(strcmp(name, "__data") == 0) {
			kernel_globals = *(KernelData*) host;
		}
		//assert(bytes == size);
		cuda_assert(cuMemcpyHtoD(mem, host, size));
		cuda_pop_context();
	}

	void tex_alloc(const char *name,
	               device_memory& mem,
	               InterpolationType interpolation,
	               ExtensionType extension)
	{
		VLOG(1) << "Texture allocate: " << name << ", "
		        << string_human_readable_number(mem.memory_size()) << " bytes. ("
		        << string_human_readable_size(mem.memory_size()) << ")";

		/* Check if we are on sm_30 or above.
		 * We use arrays and bindles textures for storage there */
		bool has_bindless_textures = info.has_bindless_textures;

		/* General variables for both architectures */
		string bind_name = name;
		size_t dsize = datatype_size(mem.data_type);
		size_t size = mem.memory_size();

		CUaddress_mode address_mode = CU_TR_ADDRESS_MODE_WRAP;
		switch(extension) {
			case EXTENSION_REPEAT:
				address_mode = CU_TR_ADDRESS_MODE_WRAP;
				break;
			case EXTENSION_EXTEND:
				address_mode = CU_TR_ADDRESS_MODE_CLAMP;
				break;
			case EXTENSION_CLIP:
				address_mode = CU_TR_ADDRESS_MODE_BORDER;
				break;
			default:
				assert(0);
				break;
		}

		CUfilter_mode filter_mode;
		if(interpolation == INTERPOLATION_CLOSEST) {
			filter_mode = CU_TR_FILTER_MODE_POINT;
		}
		else {
			filter_mode = CU_TR_FILTER_MODE_LINEAR;
		}

		CUarray_format_enum format;
		switch(mem.data_type) {
			case TYPE_UCHAR: format = CU_AD_FORMAT_UNSIGNED_INT8; break;
			case TYPE_UINT: format = CU_AD_FORMAT_UNSIGNED_INT32; break;
			case TYPE_INT: format = CU_AD_FORMAT_SIGNED_INT32; break;
			case TYPE_FLOAT: format = CU_AD_FORMAT_FLOAT; break;
			case TYPE_HALF: format = CU_AD_FORMAT_HALF; break;
			default: assert(0); return;
		}

		/* General variables for Fermi */
		CUtexref texref = NULL;

		if(!has_bindless_textures) {
			if(mem.data_depth > 1) {
				/* Kernel uses different bind names for 2d and 3d float textures,
				 * so we have to adjust couple of things here.
				 */
				vector<string> tokens;
				string_split(tokens, name, "_");
				bind_name = string_printf("__tex_image_%s_3d_%s",
				                          tokens[2].c_str(),
				                          tokens[3].c_str());
			}

			cuda_push_context();
			cuda_assert(cuModuleGetTexRef(&texref, cuModule, bind_name.c_str()));
			cuda_pop_context();

			if(!texref) {
				return;
			}
		}

		/* Data Storage */
		if(interpolation == INTERPOLATION_NONE) {
			if(has_bindless_textures) {
				mem_alloc(mem, MEM_READ_ONLY);
				mem_copy_to(mem);

				cuda_push_context();

				CUdeviceptr cumem;
				size_t cubytes;

				cuda_assert(cuModuleGetGlobal(&cumem, &cubytes, cuModule, bind_name.c_str()));

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
			else {
				mem_alloc(mem, MEM_READ_ONLY);
				mem_copy_to(mem);

				cuda_push_context();

				cuda_assert(cuTexRefSetAddress(NULL, texref, cuda_device_ptr(mem.device_pointer), size));
				cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_POINT));
				cuda_assert(cuTexRefSetFlags(texref, CU_TRSF_READ_AS_INTEGER));

				cuda_pop_context();
			}
		}
		/* Texture Storage */
		else {
			CUarray handle = NULL;

			cuda_push_context();

			if(mem.data_depth > 1) {
				CUDA_ARRAY3D_DESCRIPTOR desc;

				desc.Width = mem.data_width;
				desc.Height = mem.data_height;
				desc.Depth = mem.data_depth;
				desc.Format = format;
				desc.NumChannels = mem.data_elements;
				desc.Flags = 0;

				cuda_assert(cuArray3DCreate(&handle, &desc));
			}
			else {
				CUDA_ARRAY_DESCRIPTOR desc;

				desc.Width = mem.data_width;
				desc.Height = mem.data_height;
				desc.Format = format;
				desc.NumChannels = mem.data_elements;

				cuda_assert(cuArrayCreate(&handle, &desc));
			}

			if(!handle) {
				cuda_pop_context();
				return;
			}

			/* Allocate 3D, 2D or 1D memory */
			if(mem.data_depth > 1) {
				CUDA_MEMCPY3D param;
				memset(&param, 0, sizeof(param));
				param.dstMemoryType = CU_MEMORYTYPE_ARRAY;
				param.dstArray = handle;
				param.srcMemoryType = CU_MEMORYTYPE_HOST;
				param.srcHost = (void*)mem.data_pointer;
				param.srcPitch = mem.data_width*dsize*mem.data_elements;
				param.WidthInBytes = param.srcPitch;
				param.Height = mem.data_height;
				param.Depth = mem.data_depth;

				cuda_assert(cuMemcpy3D(&param));
			}
			else if(mem.data_height > 1) {
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

			/* Fermi and Kepler */
			mem.device_pointer = (device_ptr)handle;
			mem.device_size = size;

			stats.mem_alloc(size);

			/* Bindless Textures - Kepler */
			if(has_bindless_textures) {
				int flat_slot = 0;
				if(string_startswith(name, "__tex_image")) {
					int pos =  string(name).rfind("_");
					flat_slot = atoi(name + pos + 1);
				}
				else {
					assert(0);
				}

				CUDA_RESOURCE_DESC resDesc;
				memset(&resDesc, 0, sizeof(resDesc));
				resDesc.resType = CU_RESOURCE_TYPE_ARRAY;
				resDesc.res.array.hArray = handle;
				resDesc.flags = 0;

				CUDA_TEXTURE_DESC texDesc;
				memset(&texDesc, 0, sizeof(texDesc));
				texDesc.addressMode[0] = address_mode;
				texDesc.addressMode[1] = address_mode;
				texDesc.addressMode[2] = address_mode;
				texDesc.filterMode = filter_mode;
				texDesc.flags = CU_TRSF_NORMALIZED_COORDINATES;

				CUtexObject tex = 0;
				cuda_assert(cuTexObjectCreate(&tex, &resDesc, &texDesc, NULL));

				/* Safety check */
				if((uint)tex > UINT_MAX) {
					assert(0);
				}

				/* Resize once */
				if(flat_slot >= bindless_mapping.size()) {
					/* Allocate some slots in advance, to reduce amount
					 * of re-allocations.
					 */
					bindless_mapping.resize(flat_slot + 128);
				}

				/* Set Mapping and tag that we need to (re-)upload to device */
				bindless_mapping.get_data()[flat_slot] = (uint)tex;
				tex_bindless_map[mem.device_pointer] = (uint)tex;
				need_bindless_mapping = true;
			}
			/* Regular Textures - Fermi */
			else {
				cuda_assert(cuTexRefSetArray(texref, handle, CU_TRSA_OVERRIDE_FORMAT));
				cuda_assert(cuTexRefSetFilterMode(texref, filter_mode));
				cuda_assert(cuTexRefSetFlags(texref, CU_TRSF_NORMALIZED_COORDINATES));
			}

			cuda_pop_context();
		}

		/* Fermi, Data and Image Textures */
		if(!has_bindless_textures) {
			cuda_push_context();

			cuda_assert(cuTexRefSetAddressMode(texref, 0, address_mode));
			cuda_assert(cuTexRefSetAddressMode(texref, 1, address_mode));
			if(mem.data_depth > 1) {
				cuda_assert(cuTexRefSetAddressMode(texref, 2, address_mode));
			}

			cuda_assert(cuTexRefSetFormat(texref, format, mem.data_elements));

			cuda_pop_context();
		}

		/* Fermi and Kepler */
		tex_interp_map[mem.device_pointer] = (interpolation != INTERPOLATION_NONE);
	}

	void tex_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			if(tex_interp_map[mem.device_pointer]) {
				cuda_push_context();
				cuArrayDestroy((CUarray)mem.device_pointer);
				cuda_pop_context();

				/* Free CUtexObject (Bindless Textures) */
				if(info.has_bindless_textures && tex_bindless_map[mem.device_pointer]) {
					uint flat_slot = tex_bindless_map[mem.device_pointer];
					cuTexObjectDestroy(flat_slot);
				}

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

	void denoise(RenderTile &rtile, int sample)
	{
		if(have_error())
			return;

		cuda_push_context();

		CUfunction cuFilterDivideShadow, cuFilterGetFeature, cuFilterNonLocalMeans, cuFilterCombineHalves;
		CUfunction cuFilterConstructTransform, cuFilterEstimateBandwidths, cuFilterEstimateBiasVariance, cuFilterCalculateBandwidth;
		CUfunction cuFilterFinalPassWLR, cuFilterFinalPassNLM, cuFilterDivideCombined;
		CUdeviceptr d_buffers = cuda_device_ptr(rtile.buffer);

		cuda_assert(cuModuleGetFunction(&cuFilterDivideShadow, cuModule, "kernel_cuda_filter_divide_shadow"));
		cuda_assert(cuModuleGetFunction(&cuFilterGetFeature, cuModule, "kernel_cuda_filter_get_feature"));
		cuda_assert(cuModuleGetFunction(&cuFilterNonLocalMeans, cuModule, "kernel_cuda_filter_non_local_means"));
		cuda_assert(cuModuleGetFunction(&cuFilterCombineHalves, cuModule, "kernel_cuda_filter_combine_halves"));

		cuda_assert(cuModuleGetFunction(&cuFilterConstructTransform, cuModule, "kernel_cuda_filter_construct_transform"));
		cuda_assert(cuModuleGetFunction(&cuFilterEstimateBandwidths, cuModule, "kernel_cuda_filter_estimate_bandwidths"));
		cuda_assert(cuModuleGetFunction(&cuFilterEstimateBiasVariance, cuModule, "kernel_cuda_filter_estimate_bias_variance"));
		cuda_assert(cuModuleGetFunction(&cuFilterCalculateBandwidth, cuModule, "kernel_cuda_filter_calculate_bandwidth"));
		cuda_assert(cuModuleGetFunction(&cuFilterFinalPassWLR, cuModule, "kernel_cuda_filter_final_pass_wlr"));
		cuda_assert(cuModuleGetFunction(&cuFilterFinalPassNLM, cuModule, "kernel_cuda_filter_final_pass_nlm"));
		cuda_assert(cuModuleGetFunction(&cuFilterDivideCombined, cuModule, "kernel_cuda_filter_divide_combined"));

		cuda_assert(cuFuncSetCacheConfig(cuFilterDivideShadow, CU_FUNC_CACHE_PREFER_L1));
		cuda_assert(cuFuncSetCacheConfig(cuFilterGetFeature, CU_FUNC_CACHE_PREFER_L1));
		cuda_assert(cuFuncSetCacheConfig(cuFilterNonLocalMeans, CU_FUNC_CACHE_PREFER_L1));
		cuda_assert(cuFuncSetCacheConfig(cuFilterCombineHalves, CU_FUNC_CACHE_PREFER_L1));

		bool l1 = false;
		if(getenv("CYCLES_DENOISE_PREFER_L1")) l1 = true;
		cuda_assert(cuFuncSetCacheConfig(cuFilterConstructTransform, l1? CU_FUNC_CACHE_PREFER_L1: CU_FUNC_CACHE_PREFER_SHARED));
		cuda_assert(cuFuncSetCacheConfig(cuFilterEstimateBandwidths, l1? CU_FUNC_CACHE_PREFER_L1: CU_FUNC_CACHE_PREFER_SHARED));
		cuda_assert(cuFuncSetCacheConfig(cuFilterEstimateBiasVariance, l1? CU_FUNC_CACHE_PREFER_L1: CU_FUNC_CACHE_PREFER_SHARED));
		cuda_assert(cuFuncSetCacheConfig(cuFilterCalculateBandwidth, l1? CU_FUNC_CACHE_PREFER_L1: CU_FUNC_CACHE_PREFER_SHARED));
		cuda_assert(cuFuncSetCacheConfig(cuFilterFinalPassWLR, l1? CU_FUNC_CACHE_PREFER_L1: CU_FUNC_CACHE_PREFER_SHARED));
		cuda_assert(cuFuncSetCacheConfig(cuFilterFinalPassNLM, l1? CU_FUNC_CACHE_PREFER_L1: CU_FUNC_CACHE_PREFER_SHARED));
		cuda_assert(cuFuncSetCacheConfig(cuFilterDivideCombined, l1? CU_FUNC_CACHE_PREFER_L1: CU_FUNC_CACHE_PREFER_SHARED));

		if(have_error())
			return;

		int overscan = rtile.buffers->params.overscan;

		int hw = kernel_globals.integrator.half_window;
		int4 filter_area = make_int4(rtile.x + overscan, rtile.y + overscan, rtile.w - 2*overscan, rtile.h - 2*overscan);
		int4 buffer_area = make_int4(rtile.buffers->params.full_x, rtile.buffers->params.full_y, rtile.buffers->params.width, rtile.buffers->params.height);
		int4 rect = make_int4(max(filter_area.x - hw, buffer_area.x),
		                      max(filter_area.y - hw, buffer_area.y),
		                      min(filter_area.x + filter_area.z + hw, buffer_area.x + buffer_area.z),
		                      min(filter_area.y + filter_area.w + hw, buffer_area.y + buffer_area.w));

		int threads_per_block;
		cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuFilterFinalPassWLR));

		int xthreads = (int)sqrt((float)threads_per_block);
		int ythreads = (int)sqrt((float)threads_per_block);
		int xblocks = (buffer_area.z + xthreads - 1)/xthreads;
		int yblocks = (buffer_area.w + ythreads - 1)/ythreads;

		CUdeviceptr d_denoise_buffers;
		int w = align_up(rect.z - rect.x, 4);
		int frame_stride = w*(rect.w - rect.y);
		int pass_stride = frame_stride*rtile.buffers->params.frames;
		cuda_assert(cuMemAlloc(&d_denoise_buffers, 22*pass_stride*sizeof(float)));
#define CUDA_PTR_ADD(ptr, x) ((CUdeviceptr) (((float*) (ptr)) + (x)))

		for(int frame = 0; frame < rtile.buffers->params.frames; frame++) {
			CUdeviceptr d_denoise_buffer = CUDA_PTR_ADD(d_denoise_buffers, frame_stride*frame);
			CUdeviceptr d_buffer = CUDA_PTR_ADD(d_buffers, frame*rtile.buffers->params.width*rtile.buffers->params.height*rtile.buffers->params.get_passes_size());
			/* ==== Step 1: Prefilter general features. ==== */
			{
				int mean_from[]      = { 0, 1, 2,  6,  7,  8, 12 };
				int variance_from[]  = { 3, 4, 5,  9, 10, 11, 13 };
				int offset_to[]      = { 0, 2, 4, 10, 12, 14,  6 };
				for(int i = 0; i < 7; i++) {
					CUdeviceptr d_mean = CUDA_PTR_ADD(d_denoise_buffer, offset_to[i]*pass_stride);
					CUdeviceptr d_variance = CUDA_PTR_ADD(d_denoise_buffer, (offset_to[i]+1)*pass_stride);
					CUdeviceptr d_unfiltered = CUDA_PTR_ADD(d_denoise_buffer, 16*pass_stride);

					void *get_feature_args[] = {&sample, &d_buffer, &mean_from[i], &variance_from[i],
					                            &buffer_area,
					                            &rtile.offset, &rtile.stride,
					                            &d_unfiltered, &d_variance,
					                            &rect};
					cuda_assert(cuLaunchKernel(cuFilterGetFeature,
					                           xblocks , yblocks, 1, /* blocks */
					                           xthreads, ythreads, 1, /* threads */
					                           0, 0, get_feature_args, 0));

					/* Smooth the (generally pretty noisy) buffer variance using the spatial information from the sample variance. */
					float a = 1.0f, k_2 = 0.25f;
					int r = 4, f = 2;
					void *filter_feature_args[] = {&d_unfiltered, &d_unfiltered, &d_variance, &d_mean,
					                               &rect,
					                               &r, &f, &a, &k_2};
					cuda_assert(cuLaunchKernel(cuFilterNonLocalMeans,
					                           xblocks , yblocks, 1, /* blocks */
					                           xthreads, ythreads, 1, /* threads */
					                           0, 0, filter_feature_args, 0));
				}
			}

			/* ==== Step 2: Prefilter shadow feature. ==== */
			{
				CUdeviceptr d_mean = CUDA_PTR_ADD(d_denoise_buffer, 8*pass_stride);
				CUdeviceptr d_variance = CUDA_PTR_ADD(d_denoise_buffer, 9*pass_stride);
				/* Reuse some passes of the filter_buffer for temporary storage. */
				CUdeviceptr d_sampleV = CUDA_PTR_ADD(d_denoise_buffer, 16*pass_stride);
				CUdeviceptr d_sampleVV = CUDA_PTR_ADD(d_denoise_buffer, 17*pass_stride);
				CUdeviceptr d_bufferV = CUDA_PTR_ADD(d_denoise_buffer, 18*pass_stride);
				CUdeviceptr d_cleanV = CUDA_PTR_ADD(d_denoise_buffer, 19*pass_stride);
				CUdeviceptr d_unfiltered = CUDA_PTR_ADD(d_denoise_buffer, 20*pass_stride);
				CUdeviceptr d_unfilteredA = CUDA_PTR_ADD(d_denoise_buffer, 20*pass_stride);
				CUdeviceptr d_unfilteredB = CUDA_PTR_ADD(d_denoise_buffer, 21*pass_stride);
				CUdeviceptr d_null = (CUdeviceptr) 0;
				/* Get the A/B unfiltered passes, the combined sample variance, the estimated variance of the sample variance and the buffer variance. */
				void *divide_args[] = {&sample, &d_buffer,
					                   &buffer_area,
				                       &rtile.offset, &rtile.stride,
				                       &d_unfiltered, &d_sampleV, &d_sampleVV, &d_bufferV,
				                       &rect};
				cuda_assert(cuLaunchKernel(cuFilterDivideShadow,
				                           xblocks , yblocks, 1, /* blocks */
				                           xthreads, ythreads, 1, /* threads */
				                           0, 0, divide_args, 0));
#ifdef WITH_CYCLES_DEBUG_FILTER
#define WRITE_DEBUG(name, ptr) debug_write_pfm(string_printf("debug_%dx%d_cuda_shadow_%s.pfm", rtile.x+rtile.buffers->params.overscan, rtile.y+rtile.buffers->params.overscan, name).c_str(), ptr, rtile.w, rtile.h, 1, w)
				float *temp = new float[pass_stride*6];
				cuda_assert(cuMemcpyDtoH(temp, d_sampleV, 6*pass_stride*sizeof(float)));

				WRITE_DEBUG("unfilteredA", temp + 4*pass_stride);
				WRITE_DEBUG("unfilteredB", temp + 5*pass_stride);
				WRITE_DEBUG("bufferV", temp + 2*pass_stride);
				WRITE_DEBUG("sampleV", temp + 0*pass_stride);
				WRITE_DEBUG("sampleVV", temp + 1*pass_stride);
#endif

				/* Smooth the (generally pretty noisy) buffer variance using the spatial information from the sample variance. */
				float a = 2.0f, k_2 = 2.0f;
				int r = 6, f = 3;
				void *filter_variance_args[] = {&d_bufferV, &d_sampleV, &d_sampleVV, &d_cleanV,
				                                &rect,
				                                &r, &f, &a, &k_2};
				cuda_assert(cuLaunchKernel(cuFilterNonLocalMeans,
				                           xblocks , yblocks, 1, /* blocks */
				                           xthreads, ythreads, 1, /* threads */
				                           0, 0, filter_variance_args, 0));
#ifdef WITH_CYCLES_DEBUG_FILTER
				cuda_assert(cuMemcpyDtoH(temp, d_cleanV, pass_stride*sizeof(float)));
				WRITE_DEBUG("cleanV", temp);
#endif

				/* Use the smoothed variance to filter the two shadow half images using each other for weight calculation. */
				a = 1.0f; k_2 = 0.25f;
				r = 5; f = 3;
				void *filter_unfilteredA_args[] = {&d_unfilteredA, &d_unfilteredB, &d_cleanV, &d_sampleV,
				                                   &rect,
				                                   &r, &f, &a, &k_2};
				cuda_assert(cuLaunchKernel(cuFilterNonLocalMeans,
				                           xblocks , yblocks, 1, /* blocks */
				                           xthreads, ythreads, 1, /* threads */
				                           0, 0, filter_unfilteredA_args, 0));

				void *filter_unfilteredB_args[] = {&d_unfilteredB, &d_unfilteredA, &d_cleanV, &d_bufferV,
				                                   &rect,
				                                   &r, &f, &a, &k_2};
				cuda_assert(cuLaunchKernel(cuFilterNonLocalMeans,
				                           xblocks , yblocks, 1, /* blocks */
				                           xthreads, ythreads, 1, /* threads */
				                           0, 0, filter_unfilteredB_args, 0));
				cuda_assert(cuCtxSynchronize());
#ifdef WITH_CYCLES_DEBUG_FILTER
				cuda_assert(cuMemcpyDtoH(temp, d_sampleV, 3*pass_stride*sizeof(float)));
				WRITE_DEBUG("filteredA", temp);
				WRITE_DEBUG("filteredB", temp + 2*pass_stride);
#endif

				/* Estimate the residual variance between the two filtered halves. */
				int var_r = 2;
				void *residual_variance_args[] = {&d_null, &d_sampleVV, &d_sampleV, &d_bufferV,
				                                  &rect, &var_r};
				cuda_assert(cuLaunchKernel(cuFilterCombineHalves,
				                           xblocks , yblocks, 1, /* blocks */
				                           xthreads, ythreads, 1, /* threads */
				                           0, 0, residual_variance_args, 0));
#ifdef WITH_CYCLES_DEBUG_FILTER
				cuda_assert(cuMemcpyDtoH(temp, d_sampleVV, pass_stride*sizeof(float)));
				WRITE_DEBUG("residualV", temp);
#endif

				/* Use the residual variance for a second filter pass. */
				r = 4; f = 2;
				k_2 = 1.0f;
				void *filter_filteredA_args[] = {&d_sampleV, &d_bufferV, &d_sampleVV, &d_unfilteredA,
				                                 &rect,
				                                 &r, &f, &a, &k_2};
				cuda_assert(cuLaunchKernel(cuFilterNonLocalMeans,
				                           xblocks , yblocks, 1, /* blocks */
				                           xthreads, ythreads, 1, /* threads */
				                           0, 0, filter_filteredA_args, 0));

				void *filter_filteredB_args[] = {&d_bufferV, &d_sampleV, &d_sampleVV, &d_unfilteredB,
				                                 &rect,
				                                 &r, &f, &a, &k_2};
				cuda_assert(cuLaunchKernel(cuFilterNonLocalMeans,
				                           xblocks , yblocks, 1, /* blocks */
				                           xthreads, ythreads, 1, /* threads */
				                           0, 0, filter_filteredB_args, 0));
				cuda_assert(cuCtxSynchronize());
#ifdef WITH_CYCLES_DEBUG_FILTER
				cuda_assert(cuMemcpyDtoH(temp, d_unfilteredA, 2*pass_stride*sizeof(float)));
				WRITE_DEBUG("finalA", temp);
				WRITE_DEBUG("finalB", temp + 1*pass_stride);
#endif

				/* Combine the two double-filtered halves to a final shadow feature image and associated variance. */
				var_r = 0;
				void *final_prefiltered_args[] = {&d_mean, &d_variance,
				                                  &d_unfilteredA, &d_unfilteredB,
				                                  &rect, &var_r};
				cuda_assert(cuLaunchKernel(cuFilterCombineHalves,
				                           xblocks , yblocks, 1, /* blocks */
				                           xthreads, ythreads, 1, /* threads */
				                           0, 0, final_prefiltered_args, 0));
				cuda_assert(cuCtxSynchronize());
#ifdef WITH_CYCLES_DEBUG_FILTER
				cuda_assert(cuMemcpyDtoH(temp, d_mean, 2*pass_stride*sizeof(float)));
				WRITE_DEBUG("final", temp);
				WRITE_DEBUG("finalV", temp + 1*pass_stride);
				delete[] temp;
#undef WRITE_DEBUG
#endif
			}

			/* ==== Step 3: Copy combined color pass. ==== */
			{
				int mean_from[]      = {20, 21, 22};
				int variance_from[]  = {23, 24, 25};
				int offset_to[]      = {16, 18, 20};
				for(int i = 0; i < 3; i++) {
					CUdeviceptr d_mean = CUDA_PTR_ADD(d_denoise_buffer, offset_to[i]*pass_stride);
					CUdeviceptr d_variance = CUDA_PTR_ADD(d_denoise_buffer, (offset_to[i]+1)*pass_stride);

					void *get_feature_args[] = {&sample, &d_buffer, &mean_from[i], &variance_from[i],
					                            &buffer_area,
					                            &rtile.offset, &rtile.stride,
					                            &d_mean, &d_variance,
					                            &rect};
					cuda_assert(cuLaunchKernel(cuFilterGetFeature,
					                           xblocks , yblocks, 1, /* blocks */
					                           xthreads, ythreads, 1, /* threads */
					                           0, 0, get_feature_args, 0));
				}
			}
		}
#undef CUDA_PTR_ADD

#ifdef WITH_CYCLES_DEBUG_FILTER
#define WRITE_DEBUG(name, pass) debug_write_pfm(string_printf("debug_%dx%d_cuda_feature%d_%s.pfm", rtile.x+rtile.buffers->params.overscan, rtile.y+rtile.buffers->params.overscan, i, name).c_str(), host_denoise_buffer+pass*pass_stride, rtile.w, rtile.h, 1, w)
		float *host_denoise_buffer = new float[22*pass_stride];
		cuda_assert(cuMemcpyDtoH(host_denoise_buffer, d_denoise_buffers, 22*pass_stride*sizeof(float)));
		for(int i = 0; i < 8; i++) {
			WRITE_DEBUG("filtered", 2*i);
			WRITE_DEBUG("variance", 2*i+1);
		}
		delete[] host_denoise_buffer;
#undef WRITE_DEBUG
#endif

		/* Use the prefiltered feature to denoise the image. */
		CUdeviceptr d_storage, d_transforms;
		cuda_assert(cuMemAlloc(&d_storage, filter_area.z*filter_area.w*sizeof(CUDAFilterStorage)));
		cuda_assert(cuMemAlloc(&d_transforms, filter_area.z*filter_area.w*sizeof(float)*DENOISE_FEATURES*DENOISE_FEATURES));

		xthreads = (int)sqrt((float)threads_per_block);
		ythreads = (int)sqrt((float)threads_per_block);
		xblocks = (filter_area.z + xthreads - 1)/xthreads;
		yblocks = (filter_area.w + ythreads - 1)/ythreads;

		void *transform_args[] = {&sample,
		                          &d_denoise_buffers,
		                          &d_transforms,
		                          &d_storage,
		                          &filter_area,
		                          &rect};
		cuda_assert(cuLaunchKernel(cuFilterConstructTransform,
		                           xblocks , yblocks, 1, /* blocks */
		                           xthreads, ythreads, 1, /* threads */
		                           0, 0, transform_args, 0));

		if(kernel_globals.integrator.filter_weights == FILTER_WEIGHTS_NLM) {
			void *final_args[] = {&sample,
			                      &d_denoise_buffers,
			                      &rtile.offset,
			                      &rtile.stride,
			                      &d_transforms,
			                      &d_storage,
			                      &d_buffers,
			                      &filter_area,
			                      &rect};
			cuda_assert(cuLaunchKernel(cuFilterFinalPassNLM,
			                           xblocks , yblocks, 1, /* blocks */
			                           xthreads, ythreads, 1, /* threads */
			                           0, 0, final_args, 0));

			cuda_assert(cuCtxSynchronize());
		}
		else {
			cuda_assert(cuLaunchKernel(cuFilterEstimateBandwidths,
			                           xblocks , yblocks, 1, /* blocks */
			                           xthreads, ythreads, 1, /* threads */
			                           0, 0, transform_args, 0));

			for(int g = 0; g < 6; g++) {
				void *bias_variance_args[] = {&sample,
				                              &d_denoise_buffers,
				                              &d_transforms,
				                              &d_storage,
				                              &filter_area,
				                              &rect,
				                              &g};
				cuda_assert(cuLaunchKernel(cuFilterEstimateBiasVariance,
				                           xblocks , yblocks, 1, /* blocks */
				                           xthreads, ythreads, 1, /* threads */
				                           0, 0, bias_variance_args, 0));
			}

			void *bandwidth_args[] = {&sample,
			                          &d_storage,
			                          &filter_area};
			cuda_assert(cuLaunchKernel(cuFilterCalculateBandwidth,
			                           xblocks , yblocks, 1, /* blocks */
			                           xthreads, ythreads, 1, /* threads */
			                           0, 0, bandwidth_args, 0));

			void *final_args[] = {&sample,
			                      &d_denoise_buffers,
			                      &rtile.offset,
			                      &rtile.stride,
			                      &d_transforms,
			                      &d_storage,
			                      &d_buffers,
			                      &filter_area,
			                      &rect};
			cuda_assert(cuLaunchKernel(cuFilterFinalPassWLR,
			                           xblocks , yblocks, 1, /* blocks */
			                           xthreads, ythreads, 1, /* threads */
			                           0, 0, final_args, 0));

			cuda_assert(cuCtxSynchronize());
		}

		if(kernel_globals.integrator.use_gradients) {
			void *divide_args[] = {&d_buffers,
			                       &sample,
			                       &rtile.offset,
			                       &rtile.stride,
			                       &filter_area};
			cuda_assert(cuLaunchKernel(cuFilterDivideCombined,
			                           xblocks , yblocks, 1, /* blocks */
			                           xthreads, ythreads, 1, /* threads */
			                           0, 0, divide_args, 0));
		}

#ifdef WITH_CYCLES_DEBUG_FILTER
		CUDAFilterStorage *host_storage = new CUDAFilterStorage[filter_area.z*filter_area.w];
		cuda_assert(cuMemcpyDtoH(host_storage, d_storage, sizeof(CUDAFilterStorage)*filter_area.z*filter_area.w));
#define WRITE_DEBUG(name, var) debug_write_pfm(string_printf("debug_%dx%d_cuda_%s.pfm", rtile.x+rtile.buffers->params.overscan, rtile.y+rtile.buffers->params.overscan, name).c_str(), &host_storage[0].var, filter_area.z, filter_area.w, sizeof(CUDAFilterStorage)/sizeof(float), filter_area.z);
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
		delete[] host_storage;
#undef WRITE_DEBUG
#endif
		cuda_assert(cuMemFree(d_storage));
		cuda_assert(cuMemFree(d_transforms));
		cuda_assert(cuMemFree(d_denoise_buffers));

		cuda_pop_context();
	}

	void path_trace(RenderTile& rtile, int sample, bool branched)
	{
		if(have_error())
			return;

		cuda_push_context();

		CUfunction cuPathTrace;
		CUdeviceptr d_buffer = cuda_device_ptr(rtile.buffer);
		CUdeviceptr d_rng_state = cuda_device_ptr(rtile.rng_state);

		/* get kernel function */
		if(branched) {
			cuda_assert(cuModuleGetFunction(&cuPathTrace, cuModule, "kernel_cuda_branched_path_trace"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuPathTrace, cuModule, "kernel_cuda_path_trace"));
		}

		if(have_error())
			return;

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

		int xthreads = (int)sqrt(threads_per_block);
		int ythreads = (int)sqrt(threads_per_block);
		int xblocks = (rtile.w + xthreads - 1)/xthreads;
		int yblocks = (rtile.h + ythreads - 1)/ythreads;

		cuda_assert(cuFuncSetCacheConfig(cuPathTrace, CU_FUNC_CACHE_PREFER_L1));

		cuda_assert(cuLaunchKernel(cuPathTrace,
		                           xblocks , yblocks, 1, /* blocks */
		                           xthreads, ythreads, 1, /* threads */
		                           0, 0, args, 0));

		cuda_assert(cuCtxSynchronize());

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
			cuda_assert(cuModuleGetFunction(&cuFilmConvert, cuModule, "kernel_cuda_convert_to_half_float"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuFilmConvert, cuModule, "kernel_cuda_convert_to_byte"));
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

		int xthreads = (int)sqrt(threads_per_block);
		int ythreads = (int)sqrt(threads_per_block);
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
			cuda_assert(cuModuleGetFunction(&cuShader, cuModule, "kernel_cuda_bake"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuShader, cuModule, "kernel_cuda_shader"));
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
				if(task.shader_eval_type >= SHADER_EVAL_BAKE) {
					args[arg++] = &task.shader_filter;
				}
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
					canceled = true;
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
		if(task->type == DeviceTask::RENDER) {
			RenderTile tile;

			bool branched = task->integrator_branched;

			/* Upload Bindless Mapping */
			load_bindless_mapping();

			/* keep rendering tiles until done */
			while(task->acquire_tile(this, tile)) {
				if(tile.task == RenderTile::PATH_TRACE) {
					int start_sample = tile.start_sample;
					int end_sample = tile.start_sample + tile.num_samples;

					for(int sample = start_sample; sample < end_sample; sample++) {
						if(task->get_cancel()) {
							if(task->need_finish_queue == false)
								break;
						}

						path_trace(tile, sample, branched);

						tile.sample = sample + 1;

						task->update_progress(&tile, tile.w*tile.h);
					}

					if(tile.buffers->params.overscan && !task->get_cancel()) { /* TODO(lukas) Works, but seems hacky? */
						denoise(tile, end_sample);
					}
				}
				else if(tile.task == RenderTile::DENOISE) {
					int sample = tile.start_sample + tile.num_samples;
					denoise(tile, sample);
					tile.sample = sample;
				}

				task->release_tile(tile);
			}
		}
		else if(task->type == DeviceTask::SHADER) {
			/* Upload Bindless Mapping */
			load_bindless_mapping();

			shader(*task);

			cuda_push_context();
			cuda_assert(cuCtxSynchronize());
			cuda_pop_context();
		}
	}

	class CUDADeviceTask : public DeviceTask {
	public:
		CUDADeviceTask(CUDADevice *device, DeviceTask& task)
		: DeviceTask(task)
		{
			run = function_bind(&CUDADevice::thread_run, device, this);
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
			task_pool.push(new CUDADeviceTask(this, task));
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

bool device_cuda_init(void)
{
#ifdef WITH_CUDA_DYNLOAD
	static bool initialized = false;
	static bool result = false;

	if(initialized)
		return result;

	initialized = true;
	int cuew_result = cuewInit();
	if(cuew_result == CUEW_SUCCESS) {
		VLOG(1) << "CUEW initialization succeeded";
		if(CUDADevice::have_precompiled_kernels()) {
			VLOG(1) << "Found precompiled kernels";
			result = true;
		}
#ifndef _WIN32
		else if(cuewCompilerPath() != NULL) {
			VLOG(1) << "Found CUDA compiler " << cuewCompilerPath();
			result = true;
		}
		else {
			VLOG(1) << "Neither precompiled kernels nor CUDA compiler wad found,"
			        << " unable to use CUDA";
		}
#endif
	}
	else {
		VLOG(1) << "CUEW initialization failed: "
		        << ((cuew_result == CUEW_ERROR_ATEXIT_FAILED)
		            ? "Error setting up atexit() handler"
		            : "Error opening the library");
	}

	return result;
#else  /* WITH_CUDA_DYNLOAD */
	return true;
#endif /* WITH_CUDA_DYNLOAD */
}

Device *device_cuda_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new CUDADevice(info, stats, background);
}

void device_cuda_info(vector<DeviceInfo>& devices)
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

		int major;
		cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, num);
		if(major < 2) {
			continue;
		}

		DeviceInfo info;

		info.type = DEVICE_CUDA;
		info.description = string(name);
		info.num = num;

		info.advanced_shading = (major >= 2);
		info.has_bindless_textures = (major >= 3);
		info.pack_images = false;

		int pci_location[3] = {0, 0, 0};
		cuDeviceGetAttribute(&pci_location[0], CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID, num);
		cuDeviceGetAttribute(&pci_location[1], CU_DEVICE_ATTRIBUTE_PCI_BUS_ID, num);
		cuDeviceGetAttribute(&pci_location[2], CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, num);
		info.id = string_printf("CUDA_%s_%04x:%02x:%02x",
		                        name,
		                        (unsigned int)pci_location[0],
		                        (unsigned int)pci_location[1],
		                        (unsigned int)pci_location[2]);

		/* if device has a kernel timeout, assume it is used for display */
		if(cuDeviceGetAttribute(&attr, CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT, num) == CUDA_SUCCESS && attr == 1) {
			info.description += " (Display)";
			info.display_device = true;
			display_devices.push_back(info);
		}
		else
			devices.push_back(info);
	}

	if(!display_devices.empty())
		devices.insert(devices.end(), display_devices.begin(), display_devices.end());
}

string device_cuda_capabilities(void)
{
	CUresult result = cuInit(0);
	if(result != CUDA_SUCCESS) {
		if(result != CUDA_ERROR_NO_DEVICE) {
			return string("Error initializing CUDA: ") + cuewErrorString(result);
		}
		return "No CUDA device found\n";
	}

	int count;
	result = cuDeviceGetCount(&count);
	if(result != CUDA_SUCCESS) {
		return string("Error getting devices: ") + cuewErrorString(result);
	}

	string capabilities = "";
	for(int num = 0; num < count; num++) {
		char name[256];
		if(cuDeviceGetName(name, 256, num) != CUDA_SUCCESS) {
			continue;
		}
		capabilities += string("\t") + name + "\n";
		int value;
#define GET_ATTR(attr) \
		{ \
			if(cuDeviceGetAttribute(&value, \
			                        CU_DEVICE_ATTRIBUTE_##attr, \
			                        num) == CUDA_SUCCESS) \
			{ \
				capabilities += string_printf("\t\tCU_DEVICE_ATTRIBUTE_" #attr "\t\t\t%d\n", \
				                              value); \
			} \
		} (void)0
		/* TODO(sergey): Strip all attributes which are not useful for us
		 * or does not depend on the driver.
		 */
		GET_ATTR(MAX_THREADS_PER_BLOCK);
		GET_ATTR(MAX_BLOCK_DIM_X);
		GET_ATTR(MAX_BLOCK_DIM_Y);
		GET_ATTR(MAX_BLOCK_DIM_Z);
		GET_ATTR(MAX_GRID_DIM_X);
		GET_ATTR(MAX_GRID_DIM_Y);
		GET_ATTR(MAX_GRID_DIM_Z);
		GET_ATTR(MAX_SHARED_MEMORY_PER_BLOCK);
		GET_ATTR(SHARED_MEMORY_PER_BLOCK);
		GET_ATTR(TOTAL_CONSTANT_MEMORY);
		GET_ATTR(WARP_SIZE);
		GET_ATTR(MAX_PITCH);
		GET_ATTR(MAX_REGISTERS_PER_BLOCK);
		GET_ATTR(REGISTERS_PER_BLOCK);
		GET_ATTR(CLOCK_RATE);
		GET_ATTR(TEXTURE_ALIGNMENT);
		GET_ATTR(GPU_OVERLAP);
		GET_ATTR(MULTIPROCESSOR_COUNT);
		GET_ATTR(KERNEL_EXEC_TIMEOUT);
		GET_ATTR(INTEGRATED);
		GET_ATTR(CAN_MAP_HOST_MEMORY);
		GET_ATTR(COMPUTE_MODE);
		GET_ATTR(MAXIMUM_TEXTURE1D_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE3D_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE3D_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE3D_DEPTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_LAYERED_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE2D_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_TEXTURE2D_ARRAY_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_ARRAY_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE2D_ARRAY_NUMSLICES);
		GET_ATTR(SURFACE_ALIGNMENT);
		GET_ATTR(CONCURRENT_KERNELS);
		GET_ATTR(ECC_ENABLED);
		GET_ATTR(TCC_DRIVER);
		GET_ATTR(MEMORY_CLOCK_RATE);
		GET_ATTR(GLOBAL_MEMORY_BUS_WIDTH);
		GET_ATTR(L2_CACHE_SIZE);
		GET_ATTR(MAX_THREADS_PER_MULTIPROCESSOR);
		GET_ATTR(ASYNC_ENGINE_COUNT);
		GET_ATTR(UNIFIED_ADDRESSING);
		GET_ATTR(MAXIMUM_TEXTURE1D_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE1D_LAYERED_LAYERS);
		GET_ATTR(CAN_TEX2D_GATHER);
		GET_ATTR(MAXIMUM_TEXTURE2D_GATHER_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_GATHER_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE);
		GET_ATTR(MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE);
		GET_ATTR(MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE);
		GET_ATTR(TEXTURE_PITCH_ALIGNMENT);
		GET_ATTR(MAXIMUM_TEXTURECUBEMAP_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_SURFACE1D_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE2D_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE2D_HEIGHT);
		GET_ATTR(MAXIMUM_SURFACE3D_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE3D_HEIGHT);
		GET_ATTR(MAXIMUM_SURFACE3D_DEPTH);
		GET_ATTR(MAXIMUM_SURFACE1D_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE1D_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_SURFACE2D_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE2D_LAYERED_HEIGHT);
		GET_ATTR(MAXIMUM_SURFACE2D_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_SURFACECUBEMAP_WIDTH);
		GET_ATTR(MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_TEXTURE1D_LINEAR_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_LINEAR_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_LINEAR_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE2D_LINEAR_PITCH);
		GET_ATTR(MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT);
		GET_ATTR(COMPUTE_CAPABILITY_MAJOR);
		GET_ATTR(COMPUTE_CAPABILITY_MINOR);
		GET_ATTR(MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH);
		GET_ATTR(STREAM_PRIORITIES_SUPPORTED);
		GET_ATTR(GLOBAL_L1_CACHE_SUPPORTED);
		GET_ATTR(LOCAL_L1_CACHE_SUPPORTED);
		GET_ATTR(MAX_SHARED_MEMORY_PER_MULTIPROCESSOR);
		GET_ATTR(MAX_REGISTERS_PER_MULTIPROCESSOR);
		GET_ATTR(MANAGED_MEMORY);
		GET_ATTR(MULTI_GPU_BOARD);
		GET_ATTR(MULTI_GPU_BOARD_GROUP_ID);
#undef GET_ATTR
		capabilities += "\n";
	}

	return capabilities;
}

CCL_NAMESPACE_END
