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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clew.h"

#include "device.h"
#include "device_intern.h"

/* hack: we need to get OpenCL features available for different vendors instead of default CPU
 * now we have only one opencl feature set shared by nvidia and amd
 */

#define __KERNEL_OPENCL__
#define __SPLIT_KERNEL__

#include "buffers.h"

#include "util_foreach.h"
#include "util_map.h"
#include "util_math.h"
#include "util_md5.h"
#include "util_opengl.h"
#include "util_path.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

#define CL_MEM_PTR(p) ((cl_mem)(uintptr_t)(p))

#ifdef __SPLIT_KERNEL__
/* This value may be tuned according to the scene we are rendering */
#define PATH_ITER_INC_FACTOR 8

/*
 * When allocate global memory in chunks. We may not be able to
 * allocate exactly "CL_DEVICE_MAX_MEM_ALLOC_SIZE" bytes in chunks;
 * Since some bytes may be needed for aligning chunks of memory;
 * This is the amount of memory that we dedicate for that purpose.
 */
#define DATA_ALLOCATION_MEM_FACTOR 5000000; //5MB

/* Additional kernel build options regarding optimization */
string opt;
/* Additional kernel build option denoting compute device type */
string compute_device_type_build_option;

/* Shader data variable count - To calculate ShaderData size */
#define SD_NUM_FLOAT3 5
#ifdef __DPDU__
#define SD_NUM_DPDU_FLOAT3 2
#endif
#define SD_NUM_INT 8
#define SD_NUM_FLOAT 5
#ifdef __RAY_DIFFERENTIALS__
#define SD_NUM_RAY_DIFFERENTIALS_DIFFERENTIAL3 2
#define SD_NUM_DIFFERENTIAL 2
#endif
#define SD_NUM_RAY_DP_DIFFERENTIAL3 1
#endif

static cl_device_type opencl_device_type()
{
	char *device = getenv("CYCLES_OPENCL_TEST");

	if(device) {
		if(strcmp(device, "ALL") == 0)
			return CL_DEVICE_TYPE_ALL;
		else if(strcmp(device, "DEFAULT") == 0)
			return CL_DEVICE_TYPE_DEFAULT;
		else if(strcmp(device, "CPU") == 0)
			return CL_DEVICE_TYPE_CPU;
		else if(strcmp(device, "GPU") == 0)
			return CL_DEVICE_TYPE_GPU;
		else if(strcmp(device, "ACCELERATOR") == 0)
			return CL_DEVICE_TYPE_ACCELERATOR;
	}

	return CL_DEVICE_TYPE_ALL;
}

static bool opencl_kernel_use_debug()
{
	return (getenv("CYCLES_OPENCL_DEBUG") != NULL);
}

static bool opencl_kernel_use_advanced_shading(const string& platform)
{
	/* keep this in sync with kernel_types.h! */
	if(platform == "NVIDIA CUDA")
		return true;
	else if(platform == "Apple")
		return false;
	else if(platform == "AMD Accelerated Parallel Processing")
		return false;
	else if(platform == "Intel(R) OpenCL")
		return true;

	return false;
}

static string opencl_kernel_build_options(const string& platform, const string *debug_src = NULL)
{
#ifdef __SPLIT_KERNEL__
	string build_options = " -cl-fast-relaxed-math -D__SPLIT_KERNEL__=1 ";
	build_options.append(opt);
	build_options.append(compute_device_type_build_option);
#else
	string build_options = " -cl-fast-relaxed-math ";
#endif

	if(platform == "NVIDIA CUDA")
		build_options += "-D__KERNEL_OPENCL_NVIDIA__ -cl-nv-maxrregcount=32 -cl-nv-verbose ";

	else if(platform == "Apple")
		build_options += "-D__KERNEL_OPENCL_APPLE__ ";

	else if(platform == "AMD Accelerated Parallel Processing")
		build_options += "-D__KERNEL_OPENCL_AMD__ ";

	else if(platform == "Intel(R) OpenCL") {
		build_options += "-D__KERNEL_OPENCL_INTEL_CPU__";

		/* options for gdb source level kernel debugging. this segfaults on linux currently */
		if(opencl_kernel_use_debug() && debug_src)
			build_options += "-g -s \"" + *debug_src + "\"";
	}

#ifndef __SPLIT_KERNEL__
	/* kernel debug currently not supported in __SPLIT_KERNEL__ */
	if(opencl_kernel_use_debug())
		build_options += "-D__KERNEL_OPENCL_DEBUG__ ";

#ifdef WITH_CYCLES_DEBUG
	build_options += "-D__KERNEL_DEBUG__ ";
#endif
#endif

	return build_options;
}

/* thread safe cache for contexts and programs */
class OpenCLCache
{
	struct Slot
	{
		thread_mutex *mutex;
		cl_context context;
		cl_program program;

		Slot() : mutex(NULL), context(NULL), program(NULL) {}

		Slot(const Slot &rhs)
			: mutex(rhs.mutex)
			, context(rhs.context)
			, program(rhs.program)
		{
			/* copy can only happen in map insert, assert that */
			assert(mutex == NULL);
		}

		~Slot()
		{
			delete mutex;
			mutex = NULL;
		}
	};

	/* key is combination of platform ID and device ID */
	typedef pair<cl_platform_id, cl_device_id> PlatformDevicePair;

	/* map of Slot objects */
	typedef map<PlatformDevicePair, Slot> CacheMap;
	CacheMap cache;

	thread_mutex cache_lock;

	/* lazy instantiate */
	static OpenCLCache &global_instance()
	{
		static OpenCLCache instance;
		return instance;
	}

	OpenCLCache()
	{
	}

	~OpenCLCache()
	{
		/* Intel OpenCL bug raises SIGABRT due to pure virtual call
		 * so this is disabled. It's not necessary to free objects
		 * at process exit anyway.
		 * http://software.intel.com/en-us/forums/topic/370083#comments */

		//flush();
	}

	/* lookup something in the cache. If this returns NULL, slot_locker
	 * will be holding a lock for the cache. slot_locker should refer to a
	 * default constructed thread_scoped_lock */
	template<typename T>
	static T get_something(cl_platform_id platform, cl_device_id device,
		T Slot::*member, thread_scoped_lock &slot_locker)
	{
		assert(platform != NULL);

		OpenCLCache &self = global_instance();

		thread_scoped_lock cache_lock(self.cache_lock);

		pair<CacheMap::iterator,bool> ins = self.cache.insert(
			CacheMap::value_type(PlatformDevicePair(platform, device), Slot()));

		Slot &slot = ins.first->second;

		/* create slot lock only while holding cache lock */
		if(!slot.mutex)
			slot.mutex = new thread_mutex;

		/* need to unlock cache before locking slot, to allow store to complete */
		cache_lock.unlock();

		/* lock the slot */
		slot_locker = thread_scoped_lock(*slot.mutex);

		/* If the thing isn't cached */
		if(slot.*member == NULL) {
			/* return with the caller's lock holder holding the slot lock */
			return NULL;
		}

		/* the item was already cached, release the slot lock */
		slot_locker.unlock();

		return slot.*member;
	}

	/* store something in the cache. you MUST have tried to get the item before storing to it */
	template<typename T>
	static void store_something(cl_platform_id platform, cl_device_id device, T thing,
		T Slot::*member, thread_scoped_lock &slot_locker)
	{
		assert(platform != NULL);
		assert(device != NULL);
		assert(thing != NULL);

		OpenCLCache &self = global_instance();

		thread_scoped_lock cache_lock(self.cache_lock);
		CacheMap::iterator i = self.cache.find(PlatformDevicePair(platform, device));
		cache_lock.unlock();

		Slot &slot = i->second;

		/* sanity check */
		assert(i != self.cache.end());
		assert(slot.*member == NULL);

		slot.*member = thing;

		/* unlock the slot */
		slot_locker.unlock();
	}

public:
	/* see get_something comment */
	static cl_context get_context(cl_platform_id platform, cl_device_id device,
		thread_scoped_lock &slot_locker)
	{
		cl_context context = get_something<cl_context>(platform, device, &Slot::context, slot_locker);

		if(!context)
			return NULL;

		/* caller is going to release it when done with it, so retain it */
		cl_int ciErr = clRetainContext(context);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;

		return context;
	}

	/* see get_something comment */
	static cl_program get_program(cl_platform_id platform, cl_device_id device,
		thread_scoped_lock &slot_locker)
	{
		cl_program program = get_something<cl_program>(platform, device, &Slot::program, slot_locker);

		if(!program)
			return NULL;

		/* caller is going to release it when done with it, so retain it */
		cl_int ciErr = clRetainProgram(program);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;

		return program;
	}

	/* see store_something comment */
	static void store_context(cl_platform_id platform, cl_device_id device, cl_context context,
		thread_scoped_lock &slot_locker)
	{
		store_something<cl_context>(platform, device, context, &Slot::context, slot_locker);

		/* increment reference count in OpenCL.
		 * The caller is going to release the object when done with it. */
		cl_int ciErr = clRetainContext(context);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;
	}

	/* see store_something comment */
	static void store_program(cl_platform_id platform, cl_device_id device, cl_program program,
		thread_scoped_lock &slot_locker)
	{
		store_something<cl_program>(platform, device, program, &Slot::program, slot_locker);

		/* increment reference count in OpenCL.
		 * The caller is going to release the object when done with it. */
		cl_int ciErr = clRetainProgram(program);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;
	}

	/* discard all cached contexts and programs
	 * the parameter is a temporary workaround. See OpenCLCache::~OpenCLCache */
	static void flush()
	{
		OpenCLCache &self = global_instance();
		thread_scoped_lock cache_lock(self.cache_lock);

		foreach(CacheMap::value_type &item, self.cache) {
			if(item.second.program != NULL)
				clReleaseProgram(item.second.program);
			if(item.second.context != NULL)
				clReleaseContext(item.second.context);
		}

		self.cache.clear();
	}
};

class OpenCLDevice : public Device
{
public:
	DedicatedTaskPool task_pool;
	cl_context cxContext;
	cl_command_queue cqCommandQueue;
	cl_platform_id cpPlatform;
	cl_device_id cdDevice;
	cl_int ciErr;

#ifdef __SPLIT_KERNEL__
	/* Kernel declaration */
	cl_kernel ckPathTraceKernel_DataInit_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_LampEmission_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_Subsurface_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_DirectLighting_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL;
	cl_kernel ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL;

	/* Global memory variables [porting]; These memory is used for
	 * co-operation between different kernels; Data written by one
	 * kernel will be avaible to another kernel via this global
	 * memory
	 */
	cl_mem rng_coop;
	cl_mem throughput_coop;
	cl_mem L_transparent_coop;
	cl_mem PathRadiance_coop;
	cl_mem Ray_coop;
	cl_mem PathState_coop;
	cl_mem Intersection_coop;
	cl_mem ShaderData_coop;
	cl_mem ShaderData_coop_DL;
	cl_mem ShaderData_coop_shadow;

	/* KernelGlobals buffer */
	cl_mem kgbuffer;

	/* global buffers for ShaderData */
	cl_mem sd;                      /* ShaderData used in the main path-iteration loop */
	cl_mem sd_dl;                   /* ShaderData used in DirectLighting kernel */
	cl_mem sd_shadow;               /* ShaderData used in ShadowBlocked kernel */

	/* global buffers of each member of ShaderData */
	cl_mem P_sd;
	cl_mem P_sd_dl;
	cl_mem P_sd_shadow;
	cl_mem N_sd;
	cl_mem N_sd_dl;
	cl_mem N_sd_shadow;
	cl_mem Ng_sd;
	cl_mem Ng_sd_dl;
	cl_mem Ng_sd_shadow;
	cl_mem I_sd;
	cl_mem I_sd_dl;
	cl_mem I_sd_shadow;
	cl_mem shader_sd;
	cl_mem shader_sd_dl;
	cl_mem shader_sd_shadow;
	cl_mem flag_sd;
	cl_mem flag_sd_dl;
	cl_mem flag_sd_shadow;
	cl_mem prim_sd;
	cl_mem prim_sd_dl;
	cl_mem prim_sd_shadow;
	cl_mem type_sd;
	cl_mem type_sd_dl;
	cl_mem type_sd_shadow;
	cl_mem u_sd;
	cl_mem u_sd_dl;
	cl_mem u_sd_shadow;
	cl_mem v_sd;
	cl_mem v_sd_dl;
	cl_mem v_sd_shadow;
	cl_mem object_sd;
	cl_mem object_sd_dl;
	cl_mem object_sd_shadow;
	cl_mem time_sd;
	cl_mem time_sd_dl;
	cl_mem time_sd_shadow;
	cl_mem ray_length_sd;
	cl_mem ray_length_sd_dl;
	cl_mem ray_length_sd_shadow;
	cl_mem ray_depth_sd;
	cl_mem ray_depth_sd_dl;
	cl_mem ray_depth_sd_shadow;
	cl_mem transparent_depth_sd;
	cl_mem transparent_depth_sd_dl;
	cl_mem transparent_depth_sd_shadow;
#ifdef __RAY_DIFFERENTIALS__
	cl_mem dP_sd,dI_sd;
	cl_mem dP_sd_dl, dI_sd_dl;
	cl_mem dP_sd_shadow, dI_sd_shadow;
	cl_mem du_sd, dv_sd;
	cl_mem du_sd_dl, dv_sd_dl;
	cl_mem du_sd_shadow, dv_sd_shadow;
#endif
#ifdef __DPDU__
	cl_mem dPdu_sd, dPdv_sd;
	cl_mem dPdu_sd_dl, dPdv_sd_dl;
	cl_mem dPdu_sd_shadow, dPdv_sd_shadow;
#endif
	cl_mem closure_sd;
	cl_mem closure_sd_dl;
	cl_mem closure_sd_shadow;
	cl_mem num_closure_sd;
	cl_mem num_closure_sd_dl;
	cl_mem num_closure_sd_shadow;
	cl_mem randb_closure_sd;
	cl_mem randb_closure_sd_dl;
	cl_mem randb_closure_sd_shadow;
	cl_mem ray_P_sd;
	cl_mem ray_P_sd_dl;
	cl_mem ray_P_sd_shadow;
	cl_mem ray_dP_sd;
	cl_mem ray_dP_sd_dl;
	cl_mem ray_dP_sd_shadow;

	/* Global memory required for shadow blocked and accum_radiance */
	cl_mem BSDFEval_coop;
	cl_mem ISLamp_coop;
	cl_mem LightRay_coop;
	cl_mem AOAlpha_coop;
	cl_mem AOBSDF_coop;
	cl_mem AOLightRay_coop;
	cl_mem Intersection_coop_AO;
	cl_mem Intersection_coop_DL;

	/* Global state array that tracks ray state */
	cl_mem ray_state;

	/* per sample buffers */
	cl_mem per_sample_output_buffers;

	/* Denotes which sample each ray is being processed for */
	cl_mem work_array;

	/* Queue*/
	cl_mem Queue_data;  /* Array of size queuesize * num_queues * sizeof(int) */
	cl_mem Queue_index; /* Array of size num_queues * sizeof(int); Tracks the size of each queue */

	/* Flag to make sceneintersect and lampemission kernel use queues */
	cl_mem use_queues_flag;

	/* cl_program declaration */
	cl_program dataInit_program;
	cl_program sceneIntersect_program;
	cl_program lampEmission_program;
	cl_program QueueEnqueue_program;
	cl_program background_BufferUpdate_program;
	cl_program shaderEval_program;
	cl_program holdout_emission_blurring_termination_ao_program;
	cl_program subsurface_program;
	cl_program directLighting_program;
	cl_program shadowBlocked_program;
	cl_program nextIterationSetUp_program;
	cl_program sumAllRadiance_program;

	/* Required memory size */
	size_t rng_size;
	size_t throughput_size;
	size_t L_transparent_size;
	size_t rayState_size;
	size_t hostRayState_size;
	size_t work_element_size;
	size_t ISLamp_size;

	/* size of structures declared in kernel_types.h */
	size_t PathRadiance_size;
	size_t Ray_size;
	size_t PathState_size;
	size_t Intersection_size;

	/* Volume of ShaderData; ShaderData (in split_kernel) is a
	 * Structure-Of-Arrays implementation; We need to calculate memory
	 * required for a single thread
	 */
	size_t ShaderData_volume;

	/* This is total ShaderClosure size required for one thread */
	size_t ShaderClosure_size;

	/* Sizes of memory required for shadow blocked function */
	size_t AOAlpha_size;
	size_t AOBSDF_size;
	size_t AOLightRay_size;
	size_t LightRay_size;
	size_t BSDFEval_size;
	size_t Intersection_coop_AO_size;
	size_t Intersection_coop_DL_size;

	/* This is sizeof_output_buffer / tile_size */
	size_t per_thread_output_buffer_size;

	/* Total allocatable available device memory */
	size_t total_allocatable_memory;

	/*
	* Total allocatable memory that is actually available to us for processing
	* samples in parallel; This value determines how many threads can be launched
	* in parallel
	*/
	size_t total_allocatable_parallel_sample_processing_memory;

	/* Amount of memory required to process a single thread */
	size_t per_thread_memory;

	/* Sizes of cl_mem buffers fected using kernel_textures.h */
	size_t render_scene_input_data_size;

	/* host version of ray_state; Used in checking host path-iteration termination */
	char *hostRayStateArray;

	/* Number of path-iterations to be done in one shot */
	unsigned int PathIteration_times;

#ifdef __WORK_STEALING__
	/* Work pool with respect to each work group */
	cl_mem work_pool_wgs;

	/* Denotes the maximum work groups possible w.r.t. current tile size */
	unsigned int max_work_groups;
#endif

	/* Flag denoting if rendering the scene with current tile size is possible */
	bool cannot_render_scene;

	/* Marked True in constructor and marked false at the end of path_trace() */
	bool first_tile;

#else
	cl_kernel ckPathTraceKernel;
	cl_kernel ckFilmConvertByteKernel;
	cl_kernel ckFilmConvertHalfFloatKernel;
	cl_kernel ckShaderKernel;
	cl_kernel ckBakeKernel;

	cl_program cpProgram;
#endif

	typedef map<string, device_vector<uchar>*> ConstMemMap;
	typedef map<string, device_ptr> MemMap;

	ConstMemMap const_mem_map;
	MemMap mem_map;
	device_ptr null_mem;

	bool device_initialized;
	string platform_name;

	bool opencl_error(cl_int err)
	{
		if(err != CL_SUCCESS) {
			string message = string_printf("OpenCL error (%d): %s", err, clewErrorString(err));
			if(error_msg == "")
				error_msg = message;
			fprintf(stderr, "%s\n", message.c_str());
			return true;
		}

		return false;
	}

	void opencl_error(const string& message)
	{
		if(error_msg == "")
			error_msg = message;
		fprintf(stderr, "%s\n", message.c_str());
	}

#define opencl_assert(stmt) \
	{ \
		cl_int err = stmt; \
		\
		if(err != CL_SUCCESS) { \
			string message = string_printf("OpenCL error: %s in %s", clewErrorString(err), #stmt); \
			if(error_msg == "") \
				error_msg = message; \
			fprintf(stderr, "%s\n", message.c_str()); \
		} \
	} (void)0

	void opencl_assert_err(cl_int err, const char* where)
	{
		if(err != CL_SUCCESS) {
			string message = string_printf("OpenCL error (%d): %s in %s", err, clewErrorString(err), where);
			if(error_msg == "")
				error_msg = message;
			fprintf(stderr, "%s\n", message.c_str());
#ifndef NDEBUG
			abort();
#endif
		}
	}

	OpenCLDevice(DeviceInfo& info, Stats &stats, bool background_)
	: Device(info, stats, background_)
	{
		cpPlatform = NULL;
		cdDevice = NULL;
		cxContext = NULL;
		cqCommandQueue = NULL;
		null_mem = 0;
		device_initialized = false;

#ifdef __SPLIT_KERNEL__
		/* Initialize kernels */
		ckPathTraceKernel_DataInit_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_LampEmission_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_Subsurface_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_DirectLighting_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL = NULL;
		ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL = NULL;

		/* Initialize cl_mem variables */
		kgbuffer = NULL;
		sd = NULL;
		sd_dl = NULL;
		sd_shadow = NULL;

		P_sd = NULL;
		P_sd_dl = NULL;;
		P_sd_shadow = NULL;
		N_sd = NULL;
		N_sd_dl = NULL;
		N_sd_shadow = NULL;
		Ng_sd = NULL;
		Ng_sd_dl = NULL;
		Ng_sd_shadow = NULL;
		I_sd = NULL;
		I_sd_dl = NULL;
		I_sd_shadow = NULL;
		shader_sd = NULL;
		shader_sd_dl = NULL;
		shader_sd_shadow = NULL;
		flag_sd = NULL;
		flag_sd_dl = NULL;
		flag_sd_shadow = NULL;
		prim_sd = NULL;
		prim_sd_dl = NULL;
		prim_sd_shadow = NULL;
		type_sd = NULL;
		type_sd_dl = NULL;
		type_sd_shadow = NULL;
		u_sd = NULL;
		u_sd_dl = NULL;
		u_sd_shadow = NULL;
		v_sd = NULL;
		v_sd_dl = NULL;
		v_sd_shadow = NULL;
		object_sd = NULL;
		object_sd_dl = NULL;
		object_sd_shadow = NULL;
		time_sd = NULL;
		time_sd_dl = NULL;
		time_sd_shadow = NULL;
		ray_length_sd = NULL;
		ray_length_sd_dl = NULL;
		ray_length_sd_shadow = NULL;
		ray_depth_sd = NULL;
		ray_depth_sd_dl = NULL;
		ray_depth_sd_shadow = NULL;
		transparent_depth_sd = NULL;
		transparent_depth_sd_dl = NULL;
		transparent_depth_sd_shadow = NULL;
#ifdef __RAY_DIFFERENTIALS__
		dP_sd = NULL;
		dI_sd = NULL;
		dP_sd_dl = NULL;
		dI_sd_dl = NULL;
		dP_sd_shadow = NULL;
		dI_sd_shadow = NULL;
		du_sd = NULL;
		dv_sd = NULL;
		du_sd_dl = NULL;
		dv_sd_dl = NULL;
		du_sd_shadow = NULL;
		dv_sd_shadow = NULL;
#endif
#ifdef __DPDU__
		dPdu_sd = NULL;
		dPdv_sd = NULL;
		dPdu_sd_dl = NULL;
		dPdv_sd_dl = NULL;
		dPdu_sd_shadow = NULL;
		dPdv_sd_shadow = NULL;
#endif
		closure_sd = NULL;
		closure_sd_dl = NULL;
		closure_sd_shadow = NULL;
		num_closure_sd = NULL;
		num_closure_sd_dl = NULL;
		num_closure_sd_shadow = NULL;
		randb_closure_sd = NULL;
		randb_closure_sd_dl = NULL;
		randb_closure_sd_shadow = NULL;
		ray_P_sd = NULL;
		ray_P_sd_dl = NULL;
		ray_P_sd_shadow = NULL;
		ray_dP_sd = NULL;
		ray_dP_sd_dl = NULL;
		ray_dP_sd_shadow = NULL;

		rng_coop = NULL;
		throughput_coop = NULL;
		L_transparent_coop = NULL;
		PathRadiance_coop = NULL;
		Ray_coop = NULL;
		PathState_coop = NULL;
		Intersection_coop = NULL;
		ShaderData_coop = NULL;
		ShaderData_coop_DL = NULL;
		ShaderData_coop_shadow = NULL;
		ray_state = NULL;

		AOAlpha_coop = NULL;
		AOBSDF_coop = NULL;
		AOLightRay_coop = NULL;
		BSDFEval_coop = NULL;
		ISLamp_coop = NULL;
		LightRay_coop = NULL;
		Intersection_coop_AO = NULL;
		Intersection_coop_DL = NULL;

		work_array = NULL;

		/* Queue */
		Queue_data = NULL;
		Queue_index = NULL;
		use_queues_flag = NULL;

		per_sample_output_buffers = NULL;

		/* Initialize program */
		dataInit_program = NULL;
		sceneIntersect_program = NULL;
		lampEmission_program = NULL;
		QueueEnqueue_program = NULL;
		background_BufferUpdate_program = NULL;
		shaderEval_program = NULL;
		holdout_emission_blurring_termination_ao_program = NULL;
		subsurface_program = NULL;
		directLighting_program = NULL;
		shadowBlocked_program = NULL;
		nextIterationSetUp_program = NULL;
		sumAllRadiance_program = NULL;

		/* Initialize required memory size */
		rng_size = sizeof(RNG);
		throughput_size = sizeof(float3);
		L_transparent_size = sizeof(float);
		rayState_size = sizeof(char);
		hostRayState_size = sizeof(char);
		work_element_size = sizeof(unsigned int);
		ISLamp_size = sizeof(int);

		/* Initialize size of structures declared in kernel_types.h */
		PathRadiance_size = sizeof(PathRadiance);
		Ray_size = sizeof(Ray);
		PathState_size = sizeof(PathState);
		Intersection_size = sizeof(Intersection);

		/* Initialize sizes of memory required for shadow blocked function */
		LightRay_size = sizeof(Ray);
		BSDFEval_size = sizeof(BsdfEval);
		Intersection_coop_AO_size = sizeof(Intersection);
		Intersection_coop_DL_size = sizeof(Intersection);

		/* initialize sizes of memory required for shadow blocked function */
		AOAlpha_size = sizeof(float3);
		AOBSDF_size = sizeof(float3);
		AOLightRay_size = sizeof(Ray);
		LightRay_size = sizeof(Ray);
		BSDFEval_size = sizeof(BsdfEval);
		Intersection_coop_AO_size = sizeof(Intersection);
		Intersection_coop_DL_size = sizeof(Intersection);

		per_thread_output_buffer_size = 0;
		per_thread_memory = 0;
		render_scene_input_data_size = 0;
		hostRayStateArray = NULL;
		PathIteration_times = PATH_ITER_INC_FACTOR;
#ifdef __WORK_STEALING__
		work_pool_wgs = NULL;
		max_work_groups = 0;
#endif
		cannot_render_scene = false;
		first_tile = true;

#else
		ckPathTraceKernel = NULL;
		ckFilmConvertByteKernel = NULL;
		ckFilmConvertHalfFloatKernel = NULL;
		ckShaderKernel = NULL;
		ckBakeKernel = NULL;

		cpProgram = NULL;
#endif

		/* setup platform */
		cl_uint num_platforms;

		ciErr = clGetPlatformIDs(0, NULL, &num_platforms);
		if(opencl_error(ciErr))
			return;

		if(num_platforms == 0) {
			opencl_error("OpenCL: no platforms found.");
			return;
		}

		vector<cl_platform_id> platforms(num_platforms, NULL);

		ciErr = clGetPlatformIDs(num_platforms, &platforms[0], NULL);
		if(opencl_error(ciErr)) {
			fprintf(stderr, "clGetPlatformIDs failed \n");
			return;
		}

		int num_base = 0;
		int total_devices = 0;

		for(int platform = 0; platform < num_platforms; platform++) {
			cl_uint num_devices;

			if(opencl_error(clGetDeviceIDs(platforms[platform], opencl_device_type(), 0, NULL, &num_devices)))
				return;

			total_devices += num_devices;

			if(info.num - num_base >= num_devices) {
				/* num doesn't refer to a device in this platform */
				num_base += num_devices;
				continue;
			}

			/* device is in this platform */
			cpPlatform = platforms[platform];

			/* get devices */
			vector<cl_device_id> device_ids(num_devices, NULL);

			if(opencl_error(clGetDeviceIDs(cpPlatform, opencl_device_type(), num_devices, &device_ids[0], NULL))) {
				fprintf(stderr, "clGetDeviceIDs failed \n");
				return;
			}

			cdDevice = device_ids[info.num - num_base];

			char name[256];
			clGetPlatformInfo(cpPlatform, CL_PLATFORM_NAME, sizeof(name), &name, NULL);
			platform_name = name;

			break;
		}

		if(total_devices == 0) {
			opencl_error("OpenCL: no devices found.");
			return;
		}
		else if(!cdDevice) {
			opencl_error("OpenCL: specified device not found.");
			return;
		}

		{
			/* try to use cached context */
			thread_scoped_lock cache_locker;
			cxContext = OpenCLCache::get_context(cpPlatform, cdDevice, cache_locker);

			if(cxContext == NULL) {
				/* create context properties array to specify platform */
				const cl_context_properties context_props[] = {
					CL_CONTEXT_PLATFORM, (cl_context_properties)cpPlatform,
					0, 0
				};

				/* create context */
				cxContext = clCreateContext(context_props, 1, &cdDevice,
					context_notify_callback, cdDevice, &ciErr);

				if(opencl_error(ciErr)) {
					opencl_error("OpenCL: clCreateContext failed");
					return;
				}

				/* cache it */
				OpenCLCache::store_context(cpPlatform, cdDevice, cxContext, cache_locker);
			}
		}

#ifdef __SPLIT_KERNEL__
		ciErr = clGetDeviceInfo(cdDevice, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(size_t), &total_allocatable_memory, NULL);
		assert(ciErr == CL_SUCCESS);
		if(platform_name == "AMD Accelerated Parallel Processing") {
			/* This value is tweak-able; AMD platform does not seem to
			 * give maximum performance when all of CL_DEVICE_MAX_MEM_ALLOC_SIZE
			 * is considered for further computation.
			 */
			total_allocatable_memory /= 2;
		}
#endif

		cqCommandQueue = clCreateCommandQueue(cxContext, cdDevice, 0, &ciErr);
		if(opencl_error(ciErr))
			return;

		null_mem = (device_ptr)clCreateBuffer(cxContext, CL_MEM_READ_ONLY, 1, NULL, &ciErr);
		if(opencl_error(ciErr))
			return;

		fprintf(stderr,"Device init succes\n");
		device_initialized = true;
	}

	static void CL_CALLBACK context_notify_callback(const char *err_info,
		const void * /*private_info*/, size_t /*cb*/, void *user_data)
	{
		char name[256];
		clGetDeviceInfo((cl_device_id)user_data, CL_DEVICE_NAME, sizeof(name), &name, NULL);

		fprintf(stderr, "OpenCL error (%s): %s\n", name, err_info);
	}

	bool opencl_version_check()
	{
		char version[256];

		int major, minor, req_major = 1, req_minor = 1;

		clGetPlatformInfo(cpPlatform, CL_PLATFORM_VERSION, sizeof(version), &version, NULL);

		if(sscanf(version, "OpenCL %d.%d", &major, &minor) < 2) {
			opencl_error(string_printf("OpenCL: failed to parse platform version string (%s).", version));
			return false;
		}

		if(!((major == req_major && minor >= req_minor) || (major > req_major))) {
			opencl_error(string_printf("OpenCL: platform version 1.1 or later required, found %d.%d", major, minor));
			return false;
		}

		clGetDeviceInfo(cdDevice, CL_DEVICE_OPENCL_C_VERSION, sizeof(version), &version, NULL);

		if(sscanf(version, "OpenCL C %d.%d", &major, &minor) < 2) {
			opencl_error(string_printf("OpenCL: failed to parse OpenCL C version string (%s).", version));
			return false;
		}

		if(!((major == req_major && minor >= req_minor) || (major > req_major))) {
			opencl_error(string_printf("OpenCL: C version 1.1 or later required, found %d.%d", major, minor));
			return false;
		}

		return true;
	}

#ifdef __SPLIT_KERNEL__
	bool load_binary_SPLIT_KERNEL(cl_program *program, const string& kernel_path, const string& clbin, string custom_kernel_build_options, const string *debug_src = NULL)
	{
		/* read binary into memory */
		vector<uint8_t> binary;

		if(!path_read_binary(clbin, binary)) {
			opencl_error(string_printf("OpenCL failed to read cached binary %s.", clbin.c_str()));
			return false;
		}

		/* create program */
		cl_int status;
		size_t size = binary.size();
		const uint8_t *bytes = &binary[0];

		*program = clCreateProgramWithBinary(cxContext, 1, &cdDevice,
			&size, &bytes, &status, &ciErr);

		if(opencl_error(status) || opencl_error(ciErr)) {
			opencl_error(string_printf("OpenCL failed create program from cached binary %s.", clbin.c_str()));
			return false;
		}

		if(!build_kernel_SPLIT_KERNEL(kernel_path, program, custom_kernel_build_options, debug_src))
			return false;

		return true;
	}
#else
	bool load_binary(const string& kernel_path, const string& clbin, const string *debug_src = NULL)
	{
		/* read binary into memory */
		vector<uint8_t> binary;

		if(!path_read_binary(clbin, binary)) {
			opencl_error(string_printf("OpenCL failed to read cached binary %s.", clbin.c_str()));
			return false;
		}

		/* create program */
		cl_int status;
		size_t size = binary.size();
		const uint8_t *bytes = &binary[0];

		cpProgram = clCreateProgramWithBinary(cxContext, 1, &cdDevice,
			&size, &bytes, &status, &ciErr);

		if(opencl_error(status) || opencl_error(ciErr)) {
			opencl_error(string_printf("OpenCL failed create program from cached binary %s.", clbin.c_str()));
			return false;
		}

		if(!build_kernel(kernel_path, debug_src))
			return false;

		return true;
	}
#endif

#ifdef __SPLIT_KERNEL__
	bool save_binary_SPLIT_KERNEL(cl_program *program, const string& clbin) {
		size_t size = 0;
		clGetProgramInfo(*program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &size, NULL);

		if(!size)
			return false;

		vector<uint8_t> binary(size);
		uint8_t *bytes = &binary[0];

		clGetProgramInfo(*program, CL_PROGRAM_BINARIES, sizeof(uint8_t*), &bytes, NULL);

		if(!path_write_binary(clbin, binary)) {
			opencl_error(string_printf("OpenCL failed to write cached binary %s.", clbin.c_str()));
			return false;
		}

		return true;
	}

#else
	bool save_binary(const string& clbin)
	{
		size_t size = 0;
		clGetProgramInfo(cpProgram, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &size, NULL);

		if(!size)
			return false;

		vector<uint8_t> binary(size);
		uint8_t *bytes = &binary[0];

		clGetProgramInfo(cpProgram, CL_PROGRAM_BINARIES, sizeof(uint8_t*), &bytes, NULL);

		if(!path_write_binary(clbin, binary)) {
			opencl_error(string_printf("OpenCL failed to write cached binary %s.", clbin.c_str()));
			return false;
		}

		return true;
	}
#endif

#ifdef __SPLIT_KERNEL__
	bool build_kernel_SPLIT_KERNEL(const string& /*kernel_path*/,
	                               cl_program *kernel_program,
	                               string custom_kernel_build_options,
	                               const string *debug_src = NULL)
	{
		string build_options;
		build_options = opencl_kernel_build_options(platform_name, debug_src) + custom_kernel_build_options;

		ciErr = clBuildProgram(*kernel_program, 0, NULL, build_options.c_str(), NULL, NULL);

		/* show warnings even if build is successful */
		size_t ret_val_size = 0;

		clGetProgramBuildInfo(*kernel_program, cdDevice, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);

		if(ret_val_size > 1) {
			vector<char> build_log(ret_val_size+1);
			clGetProgramBuildInfo(*kernel_program, cdDevice, CL_PROGRAM_BUILD_LOG, ret_val_size, &build_log[0], NULL);

			build_log[ret_val_size] = '\0';
			fprintf(stderr, "OpenCL kernel build output:\n");
			fprintf(stderr, "%s\n", &build_log[0]);
		}

		if(ciErr != CL_SUCCESS) {
			opencl_error("OpenCL build failed: errors in console");
			return false;
		}

		return true;
	}
#else
	bool build_kernel(const string& /*kernel_path*/, const string *debug_src = NULL)
	{
		string build_options = opencl_kernel_build_options(platform_name, debug_src);

		ciErr = clBuildProgram(cpProgram, 0, NULL, build_options.c_str(), NULL, NULL);

		/* show warnings even if build is successful */
		size_t ret_val_size = 0;

		clGetProgramBuildInfo(cpProgram, cdDevice, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);

		if(ret_val_size > 1) {
			vector<char> build_log(ret_val_size+1);
			clGetProgramBuildInfo(cpProgram, cdDevice, CL_PROGRAM_BUILD_LOG, ret_val_size, &build_log[0], NULL);

			build_log[ret_val_size] = '\0';
			fprintf(stderr, "OpenCL kernel build output:\n");
			fprintf(stderr, "%s\n", &build_log[0]);
		}

		if(ciErr != CL_SUCCESS) {
			opencl_error("OpenCL build failed: errors in console");
			return false;
		}

		return true;
	}
#endif

#ifdef __SPLIT_KERNEL__
	bool compile_kernel_SPLIT_KERNEL(const string& kernel_path,
	                                 const string& /*kernel_name*/,
	                                 string source,
	                                 cl_program *kernel_program,
	                                 string custom_kernel_build_options,
	                                 const string *debug_src = NULL)
	{
		/* we compile kernels consisting of many files. unfortunately opencl
		 * kernel caches do not seem to recognize changes in included files.
		 * so we force recompile on changes by adding the md5 hash of all files */
		source = path_source_replace_includes(source, kernel_path);

		if(debug_src)
			path_write_text(*debug_src, source);
		size_t source_len = source.size();
		const char *source_str = source.c_str();

		*kernel_program = clCreateProgramWithSource(cxContext, 1, &source_str, &source_len, &ciErr);

		if(opencl_error(ciErr))
			return false;

		double starttime = time_dt();
		printf("Compiling OpenCL kernel ...\n");

		if(!build_kernel_SPLIT_KERNEL(kernel_path, kernel_program, custom_kernel_build_options, debug_src))
			return false;

		printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

		return true;
	}
#else
	bool compile_kernel(const string& kernel_path, const string& kernel_md5, const string *debug_src = NULL)
	{
		/* we compile kernels consisting of many files. unfortunately opencl
		 * kernel caches do not seem to recognize changes in included files.
		 * so we force recompile on changes by adding the md5 hash of all files */
		string source = "#include \"kernel.cl\" // " + kernel_md5 + "\n";
		source = path_source_replace_includes(source, kernel_path);

		if(debug_src)
			path_write_text(*debug_src, source);

		size_t source_len = source.size();
		const char *source_str = source.c_str();

		cpProgram = clCreateProgramWithSource(cxContext, 1, &source_str, &source_len, &ciErr);

		if(opencl_error(ciErr))
			return false;

		double starttime = time_dt();
		printf("Compiling OpenCL kernel ...\n");

		if(!build_kernel(kernel_path, debug_src))
			return false;

		printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

		return true;
	}
#endif

	string device_md5_hash(string kernel_custom_build_option)
	{
		MD5Hash md5;
		char version[256], driver[256], name[256], vendor[256];

		clGetPlatformInfo(cpPlatform, CL_PLATFORM_VENDOR, sizeof(vendor), &vendor, NULL);
		clGetDeviceInfo(cdDevice, CL_DEVICE_VERSION, sizeof(version), &version, NULL);
		clGetDeviceInfo(cdDevice, CL_DEVICE_NAME, sizeof(name), &name, NULL);
		clGetDeviceInfo(cdDevice, CL_DRIVER_VERSION, sizeof(driver), &driver, NULL);

		md5.append((uint8_t*)vendor, strlen(vendor));
		md5.append((uint8_t*)version, strlen(version));
		md5.append((uint8_t*)name, strlen(name));
		md5.append((uint8_t*)driver, strlen(driver));

		string options;

		options = opencl_kernel_build_options(platform_name) + kernel_custom_build_option;
		md5.append((uint8_t*)options.c_str(), options.size());

		return md5.get_hex();
	}

#ifdef __SPLIT_KERNEL__
	bool load_split_kernel_SPLIT_KERNEL(cl_program *program,
	                                    string kernel_path,
	                                    string kernel_name,
	                                    string /*device_md5*/,
	                                    string kernel_init_source,
	                                    string clbin,
	                                    string custom_kernel_build_options) {

		if(!opencl_version_check())
			return false;

		clbin = path_user_get(path_join("cache", clbin));

		/* path to preprocessed source for debugging */
		string *debug_src = NULL;

		/* if exists already, try use it */
		if(path_exists(clbin) && load_binary_SPLIT_KERNEL(program, kernel_path, clbin, custom_kernel_build_options, debug_src)) {
			/* kernel loaded from binary */
		}
		else {
			/* if does not exist or loading binary failed, compile kernel */
			if(!compile_kernel_SPLIT_KERNEL(kernel_path, kernel_name, kernel_init_source, program, custom_kernel_build_options))
				return false;

			/* save binary for reuse */
			if(!save_binary_SPLIT_KERNEL(program, clbin))
				return false;
		}

		return true;
	}
#endif

#ifdef __SPLIT_KERNEL__
	/* Get enum type names */
	string get_node_type_as_string(NodeType node) {
		switch (node) {
			case NODE_SHADER_JUMP :
				return "__NODE_SHADER_JUMP__";
				break;
			case NODE_CLOSURE_BSDF:
				return "__NODE_CLOSURE_BSDF__";
				break;
			case NODE_CLOSURE_EMISSION:
				return "__NODE_CLOSURE_EMISSION__";
				break;
			case NODE_CLOSURE_BACKGROUND:
				return "__NODE_CLOSURE_BACKGROUND__";
				break;
			case NODE_CLOSURE_HOLDOUT:
				return "__NODE_CLOSURE_HOLDOUT__";
				break;
			case NODE_CLOSURE_AMBIENT_OCCLUSION:
				return "__NODE_CLOSURE_AMBIENT_OCCLUSION__";
				break;
			case NODE_CLOSURE_VOLUME:
				return "__NODE_CLOSURE_VOLUME__";
				break;
			case NODE_CLOSURE_SET_WEIGHT:
				return "__NODE_CLOSURE_SET_WEIGHT__";
				break;
			case NODE_CLOSURE_WEIGHT:
				return "__NODE_CLOSURE_WEIGHT__";
				break;
			case NODE_EMISSION_WEIGHT:
				return "__NODE_EMISSION_WEIGHT__";
				break;
			case NODE_MIX_CLOSURE:
				return "__NODE_MIX_CLOSURE__";
				break;
			case NODE_JUMP_IF_ZERO:
				return "__NODE_JUMP_IF_ZERO__";
				break;
			case NODE_JUMP_IF_ONE:
				return "__NODE_JUMP_IF_ONE__";
				break;
			case NODE_TEX_IMAGE:
				return "__NODE_TEX_IMAGE__";
				break;
			case NODE_TEX_IMAGE_BOX:
				return "__NODE_TEX_IMAGE_BOX__";
				break;
			case NODE_TEX_ENVIRONMENT:
				return "__NODE_TEX_ENVIRONMENT__";
				break;
			case NODE_TEX_SKY:
				return "__NODE_TEX_SKY__";
				break;
			case NODE_TEX_GRADIENT:
				return "__NODE_TEX_GRADIENT__";
				break;
			case NODE_TEX_NOISE:
				return "__NODE_TEX_NOISE__";
				break;
			case NODE_TEX_VORONOI:
				return "__NODE_TEX_VORONOI__";
				break;
			case NODE_TEX_MUSGRAVE:
				return "__NODE_TEX_MUSGRAVE__";
				break;
			case NODE_TEX_WAVE:
				return "__NODE_TEX_WAVE__";
				break;
			case NODE_TEX_MAGIC:
				return "__NODE_TEX_MAGIC__";
				break;
			case NODE_TEX_CHECKER:
				return "__NODE_TEX_CHECKER__";
				break;
			case NODE_TEX_BRICK:
				return "__NODE_TEX_BRICK__";
				break;
			case NODE_CAMERA:
				return "__NODE_CAMERA__";
				break;
			case NODE_GEOMETRY:
				return "__NODE_GEOMETRY__";
				break;
			case NODE_GEOMETRY_BUMP_DX:
				return "__NODE_GEOMETRY_BUMP_DX__";
				break;
			case NODE_GEOMETRY_BUMP_DY:
				return "__NODE_GEOMETRY_BUMP_DY__";
				break;
			case NODE_LIGHT_PATH:
				return "__NODE_LIGHT_PATH__";
				break;
			case NODE_OBJECT_INFO:
				return "__NODE_OBJECT_INFO__";
				break;
			case NODE_PARTICLE_INFO:
				return "__NODE_PARTICLE_INFO__";
				break;
			case NODE_HAIR_INFO:
				return "__NODE_HAIR_INFO__";
				break;
			case NODE_CONVERT:
				return "__NODE_CONVERT__";
				break;
			case NODE_VALUE_F:
				return "__NODE_VALUE_F__";
				break;
			case NODE_VALUE_V:
				return "__NODE_VALUE_V__";
				break;
			case NODE_INVERT:
				return "__NODE_INVERT__";
				break;
			case NODE_GAMMA:
				return "__NODE_GAMMA__";
				break;
			case NODE_BRIGHTCONTRAST:
				return "__NODE_BRIGHTCONTRAST__";
				break;
			case NODE_MIX:
				return "__NODE_MIX__";
				break;
			case NODE_SEPARATE_VECTOR:
				return "__NODE_SEPARATE_VECTOR__";
				break;
			case NODE_COMBINE_VECTOR:
				return "__NODE_COMBINE_VECTOR__";
				break;
			case NODE_SEPARATE_HSV:
				return "__NODE_SEPARATE_HSV__";
				break;
			case NODE_COMBINE_HSV:
				return "__NODE_COMBINE_HSV__";
				break;
			case NODE_HSV:
				return "__NODE_HSV__";
				break;
			case NODE_ATTR:
				return "__NODE_ATTR__";
				break;
			case NODE_ATTR_BUMP_DX:
				return "__NODE_ATTR_BUMP_DX__";
				break;
			case NODE_ATTR_BUMP_DY:
				return "__NODE_ATTR_BUMP_DY__";
				break;
			case NODE_FRESNEL:
				return "__NODE_FRESNEL__";
				break;
			case NODE_LAYER_WEIGHT:
				return "__NODE_LAYER_WEIGHT__";
				break;
			case NODE_WIREFRAME:
				return "__NODE_WIREFRAME__";
				break;
			case NODE_WAVELENGTH:
				return "__NODE_WAVELENGTH__";
				break;
			case NODE_BLACKBODY:
				return "__NODE_BLACKBODY__";
				break;
			case NODE_SET_DISPLACEMENT:
				return "__NODE_SET_DISPLACEMENT__";
				break;
			case NODE_SET_BUMP:
				return "__NODE_SET_BUMP__";
				break;
			case NODE_MATH:
				return "__NODE_MATH__";
				break;
			case NODE_VECTOR_MATH:
				return "__NODE_VECTOR_MATH__";
				break;
			case NODE_VECTOR_TRANSFORM:
				return "__NODE_VECTOR_TRANSFORM__";
				break;
			case NODE_NORMAL:
				return "__NODE_NORMAL__";
				break;
			case NODE_MAPPING:
				return "__NODE_MAPPING__";
				break;
			case NODE_MIN_MAX:
				return "__NODE_MIN_MAX__";
				break;
			case NODE_TEX_COORD:
				return "__NODE_TEX_COORD__";
				break;
			case NODE_TEX_COORD_BUMP_DX:
				return "__NODE_TEX_COORD_BUMP_DX__";
				break;
			case NODE_TEX_COORD_BUMP_DY:
				return "__NODE_TEX_COORD_BUMP_DY__";
				break;
			case NODE_CLOSURE_SET_NORMAL:
				return "__NODE_CLOSURE_SET_NORMAL__";
				break;
			case NODE_RGB_RAMP:
				return "__NODE_RGB_RAMP__";
				break;
			case NODE_RGB_CURVES:
				return "__NODE_RGB_CURVES__";
				break;
			case NODE_VECTOR_CURVES:
				return "__NODE_VECTOR_CURVES__";
				break;
			case NODE_LIGHT_FALLOFF:
				return "__NODE_LIGHT_FALLOFF__";
				break;
			case NODE_TANGENT:
				return "__NODE_TANGENT__";
				break;
			case NODE_NORMAL_MAP:
				return "__NODE_NORMAL_MAP__";
				break;
			case NODE_END:
				return "__NODE_END__";
				break;
			default:
				assert(0);
		}
		return "";
	}
#endif


	bool load_kernels(bool /*experimental*/)
	{
		/* verify if device was initialized */
		if(!device_initialized) {
			fprintf(stderr, "OpenCL: failed to initialize device.\n");
			return false;
		}

#ifdef __SPLIT_KERNEL__
		string svm_build_options = "";
		opt = "";
		/* Enable only the macros related to the scene */
		for(int node_iter = NODE_END; node_iter <= NODE_UVMAP; node_iter++) {
			if(node_iter == NODE_GEOMETRY_DUPLI || node_iter == NODE_UVMAP) { continue;  }
			if(closure_nodes.find(node_iter) == closure_nodes.end()) {
				svm_build_options += " -D" + get_node_type_as_string((NodeType)node_iter) + "=0 ";
			}
			else {
				svm_build_options += " -D" + get_node_type_as_string((NodeType)node_iter) + "=1 ";
			}
		}
#ifdef __MULTI_CLOSURE__
		opt += string_printf(" -DMAX_CLOSURE=%d ", clos_max);
#endif

		compute_device_type_build_option = "";
		cl_device_type device_type;
		ciErr = clGetDeviceInfo(cdDevice, CL_DEVICE_TYPE, sizeof(cl_device_type), &device_type, NULL);
		assert(ciErr == CL_SUCCESS);
		if(device_type == CL_DEVICE_TYPE_GPU) {
			compute_device_type_build_option = " -D__COMPUTE_DEVICE_GPU__ ";
		}

		string kernel_path = path_get("kernel");
		string kernel_md5 = path_files_md5_hash(kernel_path);
		string device_md5;
		string custom_kernel_build_options;
		string kernel_init_source;
		string clbin;

		kernel_init_source = "#include \"kernel_DataInit.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash("");
		clbin = string_printf("cycles_kernel_%s_%s_DataInit.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&dataInit_program, kernel_path, "dataInit", device_md5, kernel_init_source, clbin, ""))
			return false;

		kernel_init_source = "#include \"kernel_SceneIntersect.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash("");
		clbin = string_printf("cycles_kernel_%s_%s_SceneIntersect.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&sceneIntersect_program, kernel_path, "SceneIntersect", device_md5, kernel_init_source, clbin, ""))
			return false;

		kernel_init_source = "#include \"kernel_LampEmission.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash(svm_build_options);
		clbin = string_printf("cycles_kernel_%s_%s_LampEmission.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&lampEmission_program, kernel_path, "LampEmission", device_md5, kernel_init_source, clbin, svm_build_options))
			return false;

		kernel_init_source = "#include \"kernel_QueueEnqueue.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash("");
		clbin = string_printf("cycles_kernel_%s_%s_QueueEnqueue.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&QueueEnqueue_program, kernel_path, "Queue", device_md5, kernel_init_source, clbin, ""))
			return false;

		kernel_init_source = "#include \"kernel_Background_BufferUpdate.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash(svm_build_options);
		clbin = string_printf("cycles_kernel_%s_%s_Background_BufferUpdate.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&background_BufferUpdate_program, kernel_path, "Background", device_md5, kernel_init_source, clbin, svm_build_options))
			return false;

		kernel_init_source = "#include \"kernel_ShaderEval.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash(svm_build_options);
		clbin = string_printf("cycles_kernel_%s_%s_ShaderEval.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&shaderEval_program, kernel_path, "shaderEval", device_md5, kernel_init_source, clbin, svm_build_options))
			return false;

		kernel_init_source = "#include \"kernel_Holdout_Emission_Blurring_Pathtermination_AO.cl\" // "+ kernel_md5 + "\n";
		device_md5 = device_md5_hash("");
		clbin = string_printf("cycles_kernel_%s_%s_Holdout_Emission_Blurring_Pathtermination_AO.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&holdout_emission_blurring_termination_ao_program, kernel_path, "ao", device_md5, kernel_init_source, clbin, ""))
			return false;
#ifdef __SUBSURFACE__
		kernel_init_source = "#include \"kernel_Subsurface.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash("");
		clbin = string_printf("cycles_kernel_%s_%s_Subsurface.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&subsurface_program, kernel_path, kernel_md5, device_md5, kernel_init_source, clbin, ""))
			return false;
#endif
		kernel_init_source = "#include \"kernel_DirectLighting.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash(svm_build_options);
		clbin = string_printf("cycles_kernel_%s_%s_DirectLighting.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&directLighting_program, kernel_path, "directLighting", device_md5, kernel_init_source, clbin, svm_build_options))
			return false;

		kernel_init_source = "#include \"kernel_ShadowBlocked.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash(svm_build_options);
		clbin = string_printf("cycles_kernel_%s_%s_ShadowBlocked.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&shadowBlocked_program, kernel_path, "shadow", device_md5, kernel_init_source, clbin, svm_build_options))
			return false;

		kernel_init_source = "#include \"kernel_NextIterationSetUp.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash("");
		clbin = string_printf("cycles_kernel_%s_%s_NextIterationSetUp.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&nextIterationSetUp_program, kernel_path, "nextIter", device_md5, kernel_init_source, clbin, ""))
			return false;

		kernel_init_source = "#include \"kernel_SumAllRadiance.cl\" // " + kernel_md5 + "\n";
		device_md5 = device_md5_hash("");
		clbin = string_printf("cycles_kernel_%s_%s_SumAllRadiance.clbin", device_md5.c_str(), kernel_md5.c_str());
		if(!load_split_kernel_SPLIT_KERNEL(&sumAllRadiance_program, kernel_path, "sumAll", device_md5, kernel_init_source, clbin, ""))
			return false;

#else
		/* try to use cached kernel */
		thread_scoped_lock cache_locker;
		cpProgram = OpenCLCache::get_program(cpPlatform, cdDevice, cache_locker);

		if(!cpProgram) {
			/* verify we have right opencl version */
			if(!opencl_version_check())
				return false;

			/* md5 hash to detect changes */
			string kernel_path = path_get("kernel");
			string kernel_md5 = path_files_md5_hash(kernel_path);
			string device_md5 = device_md5_hash("");

			/* path to cached binary */
			string clbin = string_printf("cycles_kernel_%s_%s.clbin", device_md5.c_str(), kernel_md5.c_str());
			clbin = path_user_get(path_join("cache", clbin));

			/* path to preprocessed source for debugging */
			string clsrc, *debug_src = NULL;

			if(opencl_kernel_use_debug()) {
				clsrc = string_printf("cycles_kernel_%s_%s.cl", device_md5.c_str(), kernel_md5.c_str());
				clsrc = path_user_get(path_join("cache", clsrc));
				debug_src = &clsrc;
			}

			/* if exists already, try use it */
			if(path_exists(clbin) && load_binary(kernel_path, clbin, debug_src)) {
				/* kernel loaded from binary */
			}
			else {
				/* if does not exist or loading binary failed, compile kernel */
				if(!compile_kernel(kernel_path, kernel_md5, debug_src))
					return false;

				/* save binary for reuse */
				if(!save_binary(clbin))
					return false;
			}

			/* cache the program */
			OpenCLCache::store_program(cpPlatform, cdDevice, cpProgram, cache_locker);
		}
#endif

		/* find kernels */
#ifdef __SPLIT_KERNEL__

		ckPathTraceKernel_DataInit_SPLIT_KERNEL = clCreateKernel(dataInit_program, "kernel_ocl_path_trace_data_initialization_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL = clCreateKernel(sceneIntersect_program, "kernel_ocl_path_trace_SceneIntersect_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckPathTraceKernel_LampEmission_SPLIT_KERNEL = clCreateKernel(lampEmission_program, "kernel_ocl_path_trace_LampEmission_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL = clCreateKernel(QueueEnqueue_program, "kernel_ocl_path_trace_QueueEnqueue_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL = clCreateKernel(background_BufferUpdate_program, "kernel_ocl_path_trace_Background_BufferUpdate_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL = clCreateKernel(shaderEval_program, "kernel_ocl_path_trace_ShaderEvaluation_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL = clCreateKernel(holdout_emission_blurring_termination_ao_program, "kernel_ocl_path_trace_holdout_emission_blurring_pathtermination_AO_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;
#ifdef __SUBSURFACE__
		ckPathTraceKernel_Subsurface_SPLIT_KERNEL = clCreateKernel(subsurface_program, "kernel_ocl_path_trace_Subsurface_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;
#endif
		ckPathTraceKernel_DirectLighting_SPLIT_KERNEL = clCreateKernel(directLighting_program, "kernel_ocl_path_trace_DirectLighting_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL = clCreateKernel(shadowBlocked_program, "kernel_ocl_path_trace_ShadowBlocked_DirectLighting_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL = clCreateKernel(nextIterationSetUp_program, "kernel_ocl_path_trace_SetupNextIteration_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL = clCreateKernel(sumAllRadiance_program, "kernel_ocl_path_trace_SumAllRadiance_SPLIT_KERNEL", &ciErr);
		if(opencl_error(ciErr))
			return false;
#else
		ckPathTraceKernel = clCreateKernel(cpProgram, "kernel_ocl_path_trace", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckFilmConvertByteKernel = clCreateKernel(cpProgram, "kernel_ocl_convert_to_byte", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckFilmConvertHalfFloatKernel = clCreateKernel(cpProgram, "kernel_ocl_convert_to_half_float", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckShaderKernel = clCreateKernel(cpProgram, "kernel_ocl_shader", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckBakeKernel = clCreateKernel(cpProgram, "kernel_ocl_bake", &ciErr);
		if(opencl_error(ciErr))
			return false;
#endif

		return true;
	}

	~OpenCLDevice()
	{
		task_pool.stop();

		if(null_mem)
			clReleaseMemObject(CL_MEM_PTR(null_mem));

		ConstMemMap::iterator mt;
		for(mt = const_mem_map.begin(); mt != const_mem_map.end(); mt++) {
			mem_free(*(mt->second));
			delete mt->second;
		}

#ifdef __SPLIT_KERNEL__
		/* Release kernels */
		if(ckPathTraceKernel_DataInit_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_DataInit_SPLIT_KERNEL);

		if(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL);

		if(ckPathTraceKernel_LampEmission_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_LampEmission_SPLIT_KERNEL);

		if(ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL);

		if(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL);

		if(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL);

		if(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL);

		if(ckPathTraceKernel_Subsurface_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_Subsurface_SPLIT_KERNEL);

		if(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL);

		if(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL);

		if(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL);

		if(ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL)
			clReleaseKernel(ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL);

		/* Release global memory */
		if(P_sd != NULL)
			clReleaseMemObject(P_sd);

		if(P_sd_dl != NULL)
			clReleaseMemObject(P_sd_dl);

		if(P_sd_shadow != NULL)
			clReleaseMemObject(P_sd_shadow);

		if(N_sd != NULL)
			clReleaseMemObject(N_sd);

		if(N_sd_dl != NULL)
			clReleaseMemObject(N_sd_dl);

		if(N_sd_shadow != NULL)
			clReleaseMemObject(N_sd_shadow);

		if(Ng_sd != NULL)
			clReleaseMemObject(Ng_sd);

		if(Ng_sd_dl != NULL)
			clReleaseMemObject(Ng_sd_dl);

		if(Ng_sd_shadow != NULL)
			clReleaseMemObject(Ng_sd_shadow);

		if(I_sd != NULL)
			clReleaseMemObject(I_sd);

		if(I_sd_dl != NULL)
			clReleaseMemObject(I_sd_dl);

		if(I_sd_shadow != NULL)
			clReleaseMemObject(I_sd_shadow);

		if(shader_sd != NULL)
			clReleaseMemObject(shader_sd);

		if(shader_sd_dl != NULL)
			clReleaseMemObject(shader_sd_dl);

		if(shader_sd_shadow != NULL)
			clReleaseMemObject(shader_sd_shadow);

		if(flag_sd != NULL)
			clReleaseMemObject(flag_sd);

		if(flag_sd_dl != NULL)
			clReleaseMemObject(flag_sd_dl);

		if(flag_sd_shadow != NULL)
			clReleaseMemObject(flag_sd_shadow);

		if(prim_sd != NULL)
			clReleaseMemObject(prim_sd);

		if(prim_sd_dl != NULL)
			clReleaseMemObject(prim_sd_dl);

		if(prim_sd_shadow != NULL)
			clReleaseMemObject(prim_sd_shadow);

		if(type_sd != NULL)
			clReleaseMemObject(type_sd);

		if(type_sd_dl != NULL)
			clReleaseMemObject(type_sd_dl);

		if(type_sd_shadow != NULL)
			clReleaseMemObject(type_sd_shadow);

		if(u_sd != NULL)
			clReleaseMemObject(u_sd);

		if(u_sd_dl != NULL)
			clReleaseMemObject(u_sd_dl);

		if(u_sd_shadow != NULL)
			clReleaseMemObject(u_sd_shadow);

		if(v_sd != NULL)
			clReleaseMemObject(v_sd);

		if(v_sd_dl != NULL)
			clReleaseMemObject(v_sd_dl);

		if(v_sd_shadow != NULL)
			clReleaseMemObject(v_sd_shadow);

		if(object_sd != NULL)
			clReleaseMemObject(object_sd);

		if(object_sd_dl != NULL)
			clReleaseMemObject(object_sd_dl);

		if(object_sd_shadow != NULL)
			clReleaseMemObject(object_sd_shadow);

		if(time_sd != NULL)
			clReleaseMemObject(time_sd);

		if(time_sd_dl != NULL)
			clReleaseMemObject(time_sd_dl);

		if(time_sd_shadow != NULL)
			clReleaseMemObject(time_sd_shadow);

		if(ray_length_sd != NULL)
			clReleaseMemObject(ray_length_sd);

		if(ray_length_sd_dl != NULL)
			clReleaseMemObject(ray_length_sd_dl);

		if(ray_length_sd_shadow != NULL)
			clReleaseMemObject(ray_length_sd_shadow);

		if(ray_depth_sd != NULL)
			clReleaseMemObject(ray_depth_sd);

		if(ray_depth_sd_dl != NULL)
			clReleaseMemObject(ray_depth_sd_dl);

		if(ray_depth_sd_shadow != NULL)
			clReleaseMemObject(ray_depth_sd_shadow);

		if(transparent_depth_sd != NULL)
			clReleaseMemObject(transparent_depth_sd);

		if(transparent_depth_sd_dl != NULL)
			clReleaseMemObject(transparent_depth_sd_dl);

		if(transparent_depth_sd_shadow != NULL)
			clReleaseMemObject(transparent_depth_sd_shadow);

#ifdef __RAY_DIFFERENTIALS__
		if(dP_sd != NULL)
			clReleaseMemObject(dP_sd);

		if(dP_sd_dl != NULL)
			clReleaseMemObject(dP_sd_dl);

		if(dP_sd_shadow != NULL)
			clReleaseMemObject(dP_sd_shadow);

		if(dI_sd != NULL)
			clReleaseMemObject(dI_sd);

		if(dI_sd_dl != NULL)
			clReleaseMemObject(dI_sd_dl);

		if(dI_sd_shadow != NULL)
			clReleaseMemObject(dI_sd_shadow);

		if(du_sd != NULL)
			clReleaseMemObject(du_sd);

		if(du_sd_dl != NULL)
			clReleaseMemObject(du_sd_dl);

		if(du_sd_shadow != NULL)
			clReleaseMemObject(du_sd_shadow);

		if(dv_sd != NULL)
			clReleaseMemObject(dv_sd);

		if(dv_sd_dl != NULL)
			clReleaseMemObject(dv_sd_dl);

		if(dv_sd_shadow != NULL)
			clReleaseMemObject(dv_sd_shadow);
#endif
#ifdef __DPDU__
		if(dPdu_sd != NULL)
			clReleaseMemObject(dPdu_sd);

		if(dPdu_sd_dl != NULL)
			clReleaseMemObject(dPdu_sd_dl);

		if(dPdu_sd_shadow != NULL)
			clReleaseMemObject(dPdu_sd_shadow);

		if(dPdv_sd != NULL)
			clReleaseMemObject(dPdv_sd);

		if(dPdv_sd_dl != NULL)
			clReleaseMemObject(dPdv_sd_dl);

		if(dPdv_sd_shadow != NULL)
			clReleaseMemObject(dPdv_sd_shadow);
#endif

		if(closure_sd != NULL)
			clReleaseMemObject(closure_sd);

		if(closure_sd_dl != NULL)
			clReleaseMemObject(closure_sd_dl);

		if(closure_sd_shadow != NULL)
			clReleaseMemObject(closure_sd_shadow);

		if(num_closure_sd != NULL)
			clReleaseMemObject(num_closure_sd);

		if(num_closure_sd_dl != NULL)
			clReleaseMemObject(num_closure_sd_dl);

		if(num_closure_sd_shadow != NULL)
			clReleaseMemObject(num_closure_sd_shadow);

		if(randb_closure_sd != NULL)
			clReleaseMemObject(randb_closure_sd);

		if(randb_closure_sd_dl != NULL)
			clReleaseMemObject(randb_closure_sd_dl);

		if(randb_closure_sd_shadow != NULL)
			clReleaseMemObject(randb_closure_sd_shadow);

		if(ray_P_sd != NULL)
			clReleaseMemObject(ray_P_sd);

		if(ray_P_sd_dl != NULL)
			clReleaseMemObject(ray_P_sd_dl);

		if(ray_P_sd_shadow != NULL)
			clReleaseMemObject(ray_P_sd_shadow);

		if(ray_dP_sd != NULL)
			clReleaseMemObject(ray_dP_sd);

		if(ray_dP_sd_dl != NULL)
			clReleaseMemObject(ray_dP_sd_dl);

		if(ray_dP_sd_shadow != NULL)
			clReleaseMemObject(ray_dP_sd_shadow);

		if(rng_coop != NULL)
			clReleaseMemObject(rng_coop);

		if(throughput_coop != NULL)
			clReleaseMemObject(throughput_coop);

		if(L_transparent_coop != NULL)
			clReleaseMemObject(L_transparent_coop);

		if(PathRadiance_coop != NULL)
			clReleaseMemObject(PathRadiance_coop);

		if(Ray_coop != NULL)
			clReleaseMemObject(Ray_coop);

		if(PathState_coop != NULL)
			clReleaseMemObject(PathState_coop);

		if(Intersection_coop != NULL)
			clReleaseMemObject(Intersection_coop);

		if(ShaderData_coop != NULL)
			clReleaseMemObject(ShaderData_coop);

		if(ShaderData_coop_DL != NULL)
			clReleaseMemObject(ShaderData_coop_DL);

		if(ShaderData_coop_shadow != NULL)
			clReleaseMemObject(ShaderData_coop_shadow);

		if(kgbuffer != NULL)
			clReleaseMemObject(kgbuffer);

		if(sd != NULL)
			clReleaseMemObject(sd);

		if(sd_dl != NULL)
			clReleaseMemObject(sd_dl);

		if(sd_shadow != NULL)
			clReleaseMemObject(sd_shadow);

		if(ray_state != NULL)
			clReleaseMemObject(ray_state);

		if(AOAlpha_coop != NULL)
			clReleaseMemObject(AOAlpha_coop);

		if(AOBSDF_coop != NULL)
			clReleaseMemObject(AOBSDF_coop);

		if(AOLightRay_coop != NULL)
			clReleaseMemObject(AOLightRay_coop);

		if(BSDFEval_coop != NULL)
			clReleaseMemObject(BSDFEval_coop);

		if(ISLamp_coop != NULL)
			clReleaseMemObject(ISLamp_coop);

		if(LightRay_coop != NULL)
			clReleaseMemObject(LightRay_coop);

		if(Intersection_coop_AO != NULL)
			clReleaseMemObject(Intersection_coop_AO);

		if(Intersection_coop_DL != NULL)
			clReleaseMemObject(Intersection_coop_DL);

		if(use_queues_flag != NULL)
			clReleaseMemObject(use_queues_flag);

		if(Queue_data != NULL)
			clReleaseMemObject(Queue_data);

		if(Queue_index != NULL)
			clReleaseMemObject(Queue_index);

		if(work_array != NULL)
			clReleaseMemObject(work_array);

#ifdef __WORK_STEALING__
		if(work_pool_wgs != NULL)
			clReleaseMemObject(work_pool_wgs);

#endif

		if(hostRayStateArray != NULL)
			free(hostRayStateArray);

		/* Release programs */
		if(dataInit_program)
			clReleaseProgram(dataInit_program);

		if(sceneIntersect_program)
			clReleaseProgram(sceneIntersect_program);

		if(lampEmission_program)
			clReleaseProgram(lampEmission_program);

		if(QueueEnqueue_program)
			clReleaseProgram(QueueEnqueue_program);

		if(background_BufferUpdate_program)
			clReleaseProgram(background_BufferUpdate_program);

		if(shaderEval_program)
			clReleaseProgram(shaderEval_program);

		if(holdout_emission_blurring_termination_ao_program)
			clReleaseProgram(holdout_emission_blurring_termination_ao_program);

		if(subsurface_program)
			clReleaseProgram(subsurface_program);

		if(directLighting_program)
			clReleaseProgram(directLighting_program);

		if(shadowBlocked_program)
			clReleaseProgram(shadowBlocked_program);

		if(nextIterationSetUp_program)
			clReleaseProgram(nextIterationSetUp_program);

		if(sumAllRadiance_program)
			clReleaseProgram(sumAllRadiance_program);
#else
		if(ckPathTraceKernel)
			clReleaseKernel(ckPathTraceKernel);
		if(ckFilmConvertByteKernel)
			clReleaseKernel(ckFilmConvertByteKernel);
		if(ckFilmConvertHalfFloatKernel)
			clReleaseKernel(ckFilmConvertHalfFloatKernel);
		if(cpProgram)
			clReleaseProgram(cpProgram);
#endif

		if(cqCommandQueue)
			clReleaseCommandQueue(cqCommandQueue);
		if(cxContext)
			clReleaseContext(cxContext);
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		size_t size = mem.memory_size();

		cl_mem_flags mem_flag;
		void *mem_ptr = NULL;

		if(type == MEM_READ_ONLY)
			mem_flag = CL_MEM_READ_ONLY;
		else if(type == MEM_WRITE_ONLY)
			mem_flag = CL_MEM_WRITE_ONLY;
		else
			mem_flag = CL_MEM_READ_WRITE;

		mem.device_pointer = (device_ptr)clCreateBuffer(cxContext, mem_flag, size, mem_ptr, &ciErr);

		opencl_assert_err(ciErr, "clCreateBuffer");

		stats.mem_alloc(size);
		mem.device_size = size;
	}

	void mem_copy_to(device_memory& mem)
	{
		/* this is blocking */
		size_t size = mem.memory_size();
		opencl_assert(clEnqueueWriteBuffer(cqCommandQueue, CL_MEM_PTR(mem.device_pointer), CL_TRUE, 0, size, (void*)mem.data_pointer, 0, NULL, NULL));
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		size_t offset = elem*y*w;
		size_t size = elem*w*h;

		opencl_assert(clEnqueueReadBuffer(cqCommandQueue, CL_MEM_PTR(mem.device_pointer), CL_TRUE, offset, size, (uchar*)mem.data_pointer + offset, 0, NULL, NULL));
	}

	void mem_zero(device_memory& mem)
	{
		if(mem.device_pointer) {
			memset((void*)mem.data_pointer, 0, mem.memory_size());
			mem_copy_to(mem);
		}
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			opencl_assert(clReleaseMemObject(CL_MEM_PTR(mem.device_pointer)));
			mem.device_pointer = 0;

			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		ConstMemMap::iterator i = const_mem_map.find(name);

		if(i == const_mem_map.end()) {
			device_vector<uchar> *data = new device_vector<uchar>();
			data->copy((uchar*)host, size);

			mem_alloc(*data, MEM_READ_ONLY);
			i = const_mem_map.insert(ConstMemMap::value_type(name, data)).first;
		}
		else {
			device_vector<uchar> *data = i->second;
			data->copy((uchar*)host, size);
		}

		mem_copy_to(*i->second);
	}

	void tex_alloc(const char *name,
	               device_memory& mem,
	               InterpolationType /*interpolation*/,
	               bool /*periodic*/)
	{
		mem_alloc(mem, MEM_READ_ONLY);
		mem_copy_to(mem);
		assert(mem_map.find(name) == mem_map.end());
		mem_map.insert(MemMap::value_type(name, mem.device_pointer));
	}

	void tex_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			foreach(const MemMap::value_type& value, mem_map) {
				if(value.second == mem.device_pointer) {
					mem_map.erase(value.first);
					break;
				}
			}

			mem_free(mem);
		}
	}

	size_t global_size_round_up(int group_size, int global_size)
	{
		int r = global_size % group_size;
		return global_size + ((r == 0)? 0: group_size - r);
	}

	void enqueue_kernel(cl_kernel kernel, size_t w, size_t h)
	{
		size_t workgroup_size, max_work_items[3];

		clGetKernelWorkGroupInfo(kernel, cdDevice,
			CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &workgroup_size, NULL);
		clGetDeviceInfo(cdDevice,
			CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t)*3, max_work_items, NULL);

		/* try to divide evenly over 2 dimensions */
		size_t sqrt_workgroup_size = max((size_t)sqrt((double)workgroup_size), 1);
		size_t local_size[2] = {sqrt_workgroup_size, sqrt_workgroup_size};

		/* some implementations have max size 1 on 2nd dimension */
		if(local_size[1] > max_work_items[1]) {
			local_size[0] = workgroup_size/max_work_items[1];
			local_size[1] = max_work_items[1];
		}

		size_t global_size[2] = {global_size_round_up(local_size[0], w), global_size_round_up(local_size[1], h)};

		/* run kernel */
		opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, kernel, 2, NULL, global_size, NULL, 0, NULL, NULL));
		opencl_assert(clFlush(cqCommandQueue));
	}

#ifdef __SPLIT_KERNEL__
	size_t get_tex_size(const char *tex_name) {
		cl_mem ptr;
		size_t ret_size;

		MemMap::iterator i = mem_map.find(tex_name);
		if(i != mem_map.end()) {
			ptr = CL_MEM_PTR(i->second);
			ciErr = clGetMemObjectInfo(ptr, CL_MEM_SIZE, sizeof(ret_size), &ret_size, NULL);
			assert(ciErr == CL_SUCCESS);
		}
		else {
			ret_size = 0;
		}

		return ret_size;
	}
#endif

#ifdef __SPLIT_KERNEL__
	size_t get_shader_closure_size(int max_closure) {
		return (sizeof(ShaderClosure)* max_closure);
	}

	size_t get_shader_data_size(size_t shader_closure_size) {
		size_t shader_data_size = 0;
		shader_data_size = SD_NUM_FLOAT3 * sizeof(float3)
#ifdef __DPDU__
			+ SD_NUM_DPDU_FLOAT3 * sizeof(float3)
#endif
#ifdef __RAY_DIFFERENTIALS__
			+ SD_NUM_RAY_DIFFERENTIALS_DIFFERENTIAL3 * sizeof(differential3)
			+SD_NUM_DIFFERENTIAL * sizeof(differential)
#endif
			+ SD_NUM_RAY_DP_DIFFERENTIAL3 * sizeof(differential3)
			+SD_NUM_INT * sizeof(int)
			+SD_NUM_FLOAT * sizeof(float);

		return (shader_data_size + shader_closure_size);
	}
#endif

	void path_trace(RenderTile& rtile, int sample)
	{
		/* cast arguments to cl types */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_buffer = CL_MEM_PTR(rtile.buffer);
		cl_mem d_rng_state = CL_MEM_PTR(rtile.rng_state);
		cl_int d_x = rtile.x;
		cl_int d_y = rtile.y;
		cl_int d_w = rtile.w;
		cl_int d_h = rtile.h;
		cl_int d_offset = rtile.offset;
		cl_int d_stride = rtile.stride;
#ifdef __SPLIT_KERNEL__
		(void)sample;

		if(cannot_render_scene) {
			return;
		}

		/* ray_state and hostRayStateArray should be of same size */
		assert(hostRayState_size == rayState_size);
		assert(rayState_size == 1);

		size_t global_size[2];
		size_t local_size[2] = { 64, 1 };

		if(first_tile) {

#ifdef __MULTI_CLOSURE__
			ShaderClosure_size = get_shader_closure_size(clos_max);
#else
			ShaderClosure_size = get_shader_closure_size(MAX_CLOSURE);
#endif
			ShaderData_volume = get_shader_data_size(ShaderClosure_size);

			/* Determine texture memories once */
#define KERNEL_TEX(type, ttype, name) \
			render_scene_input_data_size += get_tex_size(#name);
#include "kernel_textures.h"

#ifdef __WORK_STEALING__
			/* Calculate max groups */
			size_t max_global_size[2];
			size_t tile_x = rtile.tile_size.x;
			size_t tile_y = rtile.tile_size.y;
			max_global_size[0] = (((tile_x - 1) / local_size[0]) + 1) * local_size[0];
			max_global_size[1] = (((tile_y - 1) / local_size[1]) + 1) * local_size[1];
			max_work_groups = (max_global_size[0] * max_global_size[1]) / (local_size[0] * local_size[1]);

			/* Allocate work_pool_wgs memory */
			work_pool_wgs = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, max_work_groups * sizeof(unsigned int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create work_pool_wgs memory");
#endif

			/* Allocate queue_index memory only once */
			Queue_index = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, NUM_QUEUES * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Queue_index memory");

			use_queues_flag = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, sizeof(char), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create use_queues_flag memory");

			/* Calculate per thread memory */
			size_t output_buffer_size = 0;
			ciErr = clGetMemObjectInfo(d_buffer, CL_MEM_SIZE, sizeof(output_buffer_size), &output_buffer_size, NULL);
			assert(ciErr == CL_SUCCESS && "Can't get d_buffer mem object info");

			/* This value is different when running on AMD and NV */
			per_thread_output_buffer_size = output_buffer_size / (d_w * d_h);

			per_thread_memory = rng_size + throughput_size + L_transparent_size + rayState_size + work_element_size
				 + ISLamp_size + PathRadiance_size + Ray_size + PathState_size
				 + Intersection_size                  /* Overall isect */
				 + Intersection_coop_AO_size          /* Instersection_coop_AO */
				 + Intersection_coop_DL_size          /* Intersection coop DL */
				 + ShaderData_volume       /* Overall ShaderData */
				 + ShaderData_volume       /* ShaderData_coop_DL */
				 + (ShaderData_volume * 2) /* ShaderData coop shadow */
				 + LightRay_size + BSDFEval_size + AOAlpha_size + AOBSDF_size + AOLightRay_size
				 + (sizeof(int) * NUM_QUEUES)
				 + per_thread_output_buffer_size;

			int user_set_tile_w = rtile.tile_size.x;
			int user_set_tile_h = rtile.tile_size.y;

			total_allocatable_parallel_sample_processing_memory = total_allocatable_memory
			- sizeof(int)* NUM_QUEUES                                                /* Queue index size */
			- sizeof(char)                                                           /* use_queues */
			-render_scene_input_data_size                                            /* size for textures, bvh etc */
			- (user_set_tile_w * user_set_tile_h) * per_thread_output_buffer_size    /* max d_buffer size possible */
			- (user_set_tile_w * user_set_tile_h) * sizeof(RNG)                      /* max d_rng_state size possible */
#ifdef __WORK_STEALING__
			- max_work_groups * sizeof(unsigned int)
#endif
			- DATA_ALLOCATION_MEM_FACTOR;
		}

		/* Set the range of samples to be processed for every ray in path-regeneration logic */
		cl_int start_sample = rtile.start_sample;
		cl_int end_sample = rtile.start_sample + rtile.num_samples;
		cl_int num_samples = rtile.num_samples;

#ifdef __WORK_STEALING__
		/* TODO : support dynamic num_parallel_samples in work_stealing
		 * Do not change the values of num_parallel_samples/num_parallel_threads
		 */
		unsigned int num_parallel_samples = 0;
		global_size[0] = (((rtile.tile_size.x - 1) / local_size[0]) + 1) * local_size[0];
		global_size[1] = (((rtile.tile_size.y - 1) / local_size[1]) + 1) * local_size[1];
		unsigned int num_parallel_threads = global_size[0] * global_size[1];

		/* Check if we can process atleast one sample */
		num_parallel_samples = (total_allocatable_parallel_sample_processing_memory / (per_thread_memory * num_parallel_threads));
		num_parallel_samples = (num_parallel_samples > 0) ? 1 : 0;
#else
		unsigned int num_parallel_threads = total_allocatable_parallel_sample_processing_memory / per_thread_memory;

		/* Estimate maximum global work size that can be launched */
		global_size[1] = (((d_h - 1) / local_size[1]) + 1) * local_size[1];
		global_size[0] = num_parallel_threads / global_size[1];
		global_size[0] = (global_size[0] / local_size[0]) * local_size[0];

		/* Estimate number of parallel samples that can be processed in parallel */
		unsigned int num_parallel_samples = (global_size[0] / d_w) <= rtile.num_samples ? (global_size[0] / d_w) : rtile.num_samples;
		/* Wavefront size in AMD is 64 */
		num_parallel_samples = ((num_parallel_samples / 64) == 0) ?
			num_parallel_samples :
			(num_parallel_samples / 64) * 64;
#endif

		if(num_parallel_samples == 0) {
			/* Rough estimate maximum rectangular tile size for this scene, to report to the user */
			size_t scene_alloc_memory = total_allocatable_memory
				- sizeof(int)* NUM_QUEUES
				- sizeof(char)
				-render_scene_input_data_size
				- DATA_ALLOCATION_MEM_FACTOR;
			unsigned int tile_max_x = 8, tile_max_y = 8;
			bool max_rect_tile_reached = false;
			while(!max_rect_tile_reached) {
				unsigned int num_parallel_samples_possible = 0;
#ifdef __WORK_STEALING__
				unsigned int current_max_global_size[2];
				current_max_global_size[0] = (((tile_max_x - 1) / local_size[0]) + 1) * local_size[0];
				current_max_global_size[1] = (((tile_max_y - 1) / local_size[1]) + 1) * local_size[1];
				unsigned int current_max_work_groups = (current_max_global_size[0] * current_max_global_size[1]) / (local_size[0] * local_size[1]);
#endif
				size_t memory_for_parallel_sample_processing = scene_alloc_memory
#ifdef __WORK_STEALING__
					- current_max_work_groups * sizeof(unsigned int)
#endif
					- (tile_max_x * tile_max_y) * per_thread_output_buffer_size
					- (tile_max_x * tile_max_y) * sizeof(RNG);
				num_parallel_samples_possible = memory_for_parallel_sample_processing / (per_thread_memory * tile_max_x * tile_max_y);
				if(num_parallel_samples_possible == 0) {
					max_rect_tile_reached = true;
				}
				else {
					tile_max_x += local_size[0];
					tile_max_y += local_size[1];
				}
			}

			fprintf(stdout, "Not enough device memory, reduce tile size. \n \
One possible tile size is %zux%zu \n", tile_max_x - local_size[0] , tile_max_y - local_size[1]);
			cannot_render_scene = true;
			return;
		}

#ifdef __WORK_STEALING__
		global_size[0] = (((d_w - 1) / local_size[0]) + 1) * local_size[0];
		global_size[1] = (((d_h - 1) / local_size[1]) + 1) * local_size[1];
#else
		/* We may not need all global_size[0] threads; We only need as much as num_parallel_samples * d_w */
		global_size[0] = num_parallel_samples * d_w;
		global_size[0] = (((global_size[0] - 1) / local_size[0]) + 1) * local_size[0];

		assert(global_size[0] * global_size[1] <= num_parallel_threads);
		assert(global_size[0] * global_size[1] >= d_w * d_h);
#endif // __WORK_STEALING__

		/* Allocate all required global memory once */
		if(first_tile) {

			/* Copy dummy KernelGlobals related to OpenCL from kernel_globals.h to fetch its size */
#ifdef __KERNEL_OPENCL__
			typedef struct KernelGlobals {
				ccl_constant KernelData *data;

#define KERNEL_TEX(type, ttype, name) \
	ccl_global type *name;
#include "kernel_textures.h"
			} KernelGlobals;

			kgbuffer = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, sizeof(KernelGlobals), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create kgbuffer memory");
#endif
			size_t num_shader_soa_ptr = SD_NUM_FLOAT3 + SD_NUM_INT + SD_NUM_FLOAT
#ifdef __DPDU__
				+ SD_NUM_DPDU_FLOAT3
#endif
#ifdef __RAY_DIFFERENTIAL__
				+ SD_NUM_RAY_DIFFERENTIALS_DIFFERENTIAL3
				+ SD_NUM_DIFFERENTIAL
#endif
				+ SD_NUM_RAY_DP_DIFFERENTIAL3;
			size_t ShaderData_SOA_size = num_shader_soa_ptr * sizeof(void *);

				/* Create global buffers for ShaderData */
			sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, ShaderData_SOA_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Shaderdata memory");

			sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, ShaderData_SOA_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create sd_dl memory");

			sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, ShaderData_SOA_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create sd_shadow memory");

			P_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads*sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create P_sd memory");

			P_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads*sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create P_sd_dl memory");

			P_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create P_sd_shadow memory");

			N_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create N_sd memory");

			N_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create N_sd_dl memory");

			N_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create N_sd_shadow memory");

			Ng_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Ng_sd memory");

			Ng_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Ng_sd_dl memory");

			Ng_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Ng_sd_shadow memory");

			I_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create I_sd memory");

			I_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create I_sd_dl memory");

			I_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create I_sd_shadow memory");

			shader_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create shader_sd memory");

			shader_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create shader_sd_dl memory");

			shader_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create shader_sd_shadow memory");

			flag_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create flag_sd memory");

			flag_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create flag_sd_dl memory");

			flag_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create flag_sd_shadow memory");

			prim_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create prim_sd memory");

			prim_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create prim_sd_dl memory");

			prim_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create prim_sd_shadow memory");

			type_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create type_sd memory");

			type_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create type_sd_dl memory");

			type_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create type_sd_shadow memory");

			u_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create u_sd memory");

			u_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create u_sd_dl memory");

			u_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create u_sd_shadow memory");

			v_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create v_sd memory");

			v_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create v_sd_dl memory");

			v_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create v_sd_shadow memory");

			object_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create object_sd memory");

			object_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create object_sd_dl memory");

			object_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create object_sd_shadow memory");

			time_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create time_sd memory");

			time_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create time_sd_dl memory");

			time_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create time_sd_shadow memory");

			ray_length_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_length_sd memory");

			ray_length_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_length_sd_dl memory");

			ray_length_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_length_sd_shadow memory");

			ray_depth_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_depth_sd memory");

			ray_depth_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_depth_sd_dl memory");

			ray_depth_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_depth_sd_shadow memory");

			transparent_depth_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create transparent_depth_sd memory");

			transparent_depth_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create transparent_depth_sd_dl memory");

			transparent_depth_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create transparent_depth_sd_shadow memory");

#ifdef __RAY_DIFFERENTIALS__
			dP_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dP_sd memory");

			dP_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dP_sd_dl memory");

			dP_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(differential3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dP_sd_shadow memory");

			dI_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dI_sd memory");

			dI_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dI_sd_dl memory");

			dI_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(differential3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dI_sd_shadow memory");

			du_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create du_sd memory");

			du_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create du_sd_dl memory");

			du_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(differential), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create du_sd_shadow memory");

			dv_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create du_sd memory");

			dv_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create du_sd_dl memory");

			dv_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(differential), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create du_sd_shadow memory");
#endif
#ifdef __DPDU__
			dPdu_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dPdu_sd memory");

			dPdu_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dPdu_sd_dl memory");

			dPdu_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dPdu_sd_shadow memory");

			dPdv_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dPdv_sd memory");

			dPdv_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dPdv_sd_dl memory");

			dPdv_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create dPdv_sd_shadow memory");
#endif
			closure_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * ShaderClosure_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create closure_sd memory");

			closure_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * ShaderClosure_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create closure_sd_dl memory");

			closure_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * ShaderClosure_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create closure_sd_shadow memory");

			num_closure_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create num_closure_sd memory");

			num_closure_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create num_closure_sd_dl memory");

			num_closure_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(int), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create num_closure_sd_shadow memory");

			randb_closure_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create randb_closure_sd memory");

			randb_closure_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create randb_closure_sd_dl memory");

			randb_closure_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create randb_closure_sd_shadow memory");

			ray_P_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_P_sd memory");

			ray_P_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_P_sd_dl memory");

			ray_P_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(float3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_P_sd_shadow memory");

			ray_dP_sd = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_dP_sd memory");

			ray_dP_sd_dl = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * sizeof(differential3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_dP_sd_dl memory");

			ray_dP_sd_shadow = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * 2 * sizeof(differential3), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_dP_sd_shadow memory");

			/* creation of global memory buffers which are shared among the kernels */
			rng_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * rng_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create rng_coop memory");

			throughput_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * throughput_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create throughput_coop memory");

			L_transparent_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * L_transparent_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create L_transparent_coop memory");

			PathRadiance_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * PathRadiance_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create PathRadiance_coop memory");

			Ray_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * Ray_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Ray_coop memory");

			PathState_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * PathState_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create PathState_coop memory");

			Intersection_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * Intersection_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Intersection_coop memory");

			AOAlpha_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * AOAlpha_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create AOAlpha_coop memory");

			AOBSDF_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * AOBSDF_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create AOBSDF_coop memory");

			AOLightRay_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * AOLightRay_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create AOLightRay_coop memory");

			BSDFEval_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * BSDFEval_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create BSDFEval_coop memory");

			ISLamp_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * ISLamp_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ISLamp_coop memory");

			LightRay_coop = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * LightRay_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create LightRay_coop memory");

			Intersection_coop_AO = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * Intersection_coop_AO_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Intersection_coop_AO_memory");

			Intersection_coop_DL = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * Intersection_coop_DL_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Intersection_coop_DL_memory");

			ray_state = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * rayState_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create ray_state memory");

			hostRayStateArray = (char *)calloc(num_parallel_threads, hostRayState_size);
			assert(hostRayStateArray != NULL && "Can't create hostRayStateArray memory");

			Queue_data = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * (NUM_QUEUES * sizeof(int) + sizeof(int)), NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create Queue data memory");

			work_array = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * work_element_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create work_array memory");

			per_sample_output_buffers = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, num_parallel_threads * per_thread_output_buffer_size, NULL, &ciErr);
			assert(ciErr == CL_SUCCESS && "Can't create per_sample_output_buffers memory");
		}

		cl_int dQueue_size = global_size[0] * global_size[1];
		cl_int total_num_rays = global_size[0] * global_size[1];

		/* Set arguments for ckPathTraceKernel_DataInit_SPLIT_KERNEL kernel */
		cl_uint narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(sd), (void*)&sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(sd_dl), (void*)&sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(sd_shadow), (void*)&sd_shadow));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(P_sd), (void*)&P_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(P_sd_dl), (void*)&P_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(P_sd_shadow), (void*)&P_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(N_sd), (void*)&N_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(N_sd_dl), (void*)&N_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(N_sd_shadow), (void*)&N_sd_shadow));


		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(Ng_sd), (void*)&Ng_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(Ng_sd_dl), (void*)&Ng_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(Ng_sd_shadow), (void*)&Ng_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(I_sd), (void*)&I_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(I_sd_dl), (void*)&I_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(I_sd_shadow), (void*)&I_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(shader_sd), (void*)&shader_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(shader_sd_dl), (void*)&shader_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(shader_sd_shadow), (void*)&shader_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(flag_sd), (void*)&flag_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(flag_sd_dl), (void*)&flag_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(flag_sd_shadow), (void*)&flag_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(prim_sd), (void*)&prim_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(prim_sd_dl), (void*)&prim_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(prim_sd_shadow), (void*)&prim_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(type_sd), (void*)&type_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(type_sd_dl), (void*)&type_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(type_sd_shadow), (void*)&type_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(u_sd), (void*)&u_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(u_sd_dl), (void*)&u_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(u_sd_shadow), (void*)&u_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(v_sd), (void*)&v_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(v_sd_dl), (void*)&v_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(v_sd_shadow), (void*)&v_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(object_sd), (void*)&object_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(object_sd_dl), (void*)&object_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(object_sd_shadow), (void*)&object_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(time_sd), (void*)&time_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(time_sd_dl), (void*)&time_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(time_sd_shadow), (void*)&time_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_length_sd), (void*)&ray_length_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_length_sd_dl), (void*)&ray_length_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_length_sd_shadow), (void*)&ray_length_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_depth_sd), (void*)&ray_depth_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_depth_sd_dl), (void*)&ray_depth_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_depth_sd_shadow), (void*)&ray_depth_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(transparent_depth_sd), (void*)&transparent_depth_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(transparent_depth_sd_dl), (void*)&transparent_depth_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(transparent_depth_sd_shadow), (void*)&transparent_depth_sd_shadow));
#ifdef __RAY_DIFFERENTIALS__
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dP_sd), (void*)&dP_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dP_sd_dl), (void*)&dP_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dP_sd_shadow), (void*)&dP_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dI_sd), (void*)&dI_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dI_sd_dl), (void*)&dI_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dI_sd_shadow), (void*)&dI_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(du_sd), (void*)&du_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(du_sd_dl), (void*)&du_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(du_sd_shadow), (void*)&du_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dv_sd), (void*)&dv_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dv_sd_dl), (void*)&dv_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dv_sd_shadow), (void*)&dv_sd_shadow));
#endif
#ifdef __DPDU__
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dPdu_sd), (void*)&dPdu_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dPdu_sd_dl), (void*)&dPdu_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dPdu_sd_shadow), (void*)&dPdu_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dPdv_sd), (void*)&dPdv_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dPdv_sd_dl), (void*)&dPdv_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dPdv_sd_shadow), (void*)&dPdv_sd_shadow));
#endif
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(closure_sd), (void*)&closure_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(closure_sd_dl), (void*)&closure_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(closure_sd_shadow), (void*)&closure_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(num_closure_sd), (void*)&num_closure_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(num_closure_sd_dl), (void*)&num_closure_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(num_closure_sd_shadow), (void*)&num_closure_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(randb_closure_sd), (void*)&randb_closure_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(randb_closure_sd_dl), (void*)&randb_closure_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(randb_closure_sd_shadow), (void*)&randb_closure_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_P_sd), (void*)&ray_P_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_P_sd_dl), (void*)&ray_P_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_P_sd_shadow), (void*)&ray_P_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_dP_sd), (void*)&ray_dP_sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_dP_sd_dl), (void*)&ray_dP_sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_dP_sd_shadow), (void*)&ray_dP_sd_shadow));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(per_sample_output_buffers), (void*)&per_sample_output_buffers));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(d_rng_state), (void*)&d_rng_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(rng_coop), (void*)&rng_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(throughput_coop), (void*)&throughput_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(L_transparent_coop), (void*)&L_transparent_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(PathRadiance_coop), (void*)&PathRadiance_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(Ray_coop), (void*)&Ray_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));

#define KERNEL_TEX(type, ttype, name) \
		set_kernel_arg_mem(ckPathTraceKernel_DataInit_SPLIT_KERNEL, &narg, #name);
#include "kernel_textures.h"

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(start_sample), (void*)&start_sample));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(d_x), (void*)&d_x));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(d_y), (void*)&d_y));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(d_w), (void*)&d_w));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(d_h), (void*)&d_h));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(d_offset), (void*)&d_offset));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(d_stride), (void*)&d_stride));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void*)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(use_queues_flag), (void*)&use_queues_flag));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(work_array), (void*)&work_array));
#ifdef __WORK_STEALING__
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(work_pool_wgs), (void*)&work_pool_wgs));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(num_samples), (void*)&num_samples));
#endif
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DataInit_SPLIT_KERNEL, narg++, sizeof(num_parallel_samples), (void*)&num_parallel_samples));

		/* Set arguments for ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL */;
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(rng_coop), (void*)&rng_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(Ray_coop), (void*)&Ray_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(Intersection_coop), (void*)&Intersection_coop));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(d_w), (void*)&d_w));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(d_h), (void*)&d_h));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void*)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(use_queues_flag), (void*)&use_queues_flag));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, narg++, sizeof(num_parallel_samples), (void*)&num_parallel_samples));

		/* Set arguments for ckPathTracekernel_LampEmission_SPLIT_KERNEL kernel */
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(sd), (void *)&sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(throughput_coop), (void*)&throughput_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(PathRadiance_coop), (void*)&PathRadiance_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(Ray_coop), (void*)&Ray_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(Intersection_coop), (void*)&Intersection_coop));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(d_w), (void*)&d_w));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(d_h), (void*)&d_h));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void*)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(use_queues_flag), (void*)&use_queues_flag));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_LampEmission_SPLIT_KERNEL, narg++, sizeof(num_parallel_samples), (void*)&num_parallel_samples));

		/* Set arguments for ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL kernel */
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void*)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));

		/* Set arguments for ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL kernel */
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(sd), (void*)&sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(per_sample_output_buffers), (void*)&per_sample_output_buffers));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(d_rng_state), (void*)&d_rng_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(rng_coop), (void*)&rng_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(throughput_coop), (void*)&throughput_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(PathRadiance_coop), (void*)&PathRadiance_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(Ray_coop), (void*)&Ray_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(L_transparent_coop), (void*)&L_transparent_coop));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(d_w), (void*)&d_w));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(d_h), (void*)&d_h));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(d_x), (void*)&d_x));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(d_y), (void*)&d_y));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(d_stride), (void*)&d_stride));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(work_array), (void*)&work_array));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void*)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(end_sample), (void*)&end_sample));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(start_sample), (void*)&start_sample));
#ifdef __WORK_STEALING__
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(work_pool_wgs), (void*)&work_pool_wgs));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(num_samples), (void*)&num_samples));
#endif
		opencl_assert(clSetKernelArg(ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, narg++, sizeof(num_parallel_samples), (void*)&num_parallel_samples));

		/* Set arguments for ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL */
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(sd), (void*)&sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(rng_coop), (void*)&rng_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(Ray_coop), (void*)&Ray_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(Intersection_coop), (void*)&Intersection_coop));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void *)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void *)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void *)&dQueue_size));

		/* Set up arguments for ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL kernel */
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(sd), (void*)&sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(per_sample_output_buffers), (void*)&per_sample_output_buffers));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(rng_coop), (void*)&rng_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(throughput_coop), (void*)&throughput_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(L_transparent_coop), (void*)&L_transparent_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(PathRadiance_coop), (void*)&PathRadiance_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(Intersection_coop), (void*)&Intersection_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(AOAlpha_coop), (void*)&AOAlpha_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(AOBSDF_coop), (void*)&AOBSDF_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(AOLightRay_coop), (void*)&AOLightRay_coop));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(d_w), (void*)&d_w));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(d_h), (void*)&d_h));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(d_x), (void*)&d_x));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(d_y), (void*)&d_y));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(d_stride), (void*)&d_stride));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(work_array), (void*)&work_array));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void*)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));
#ifdef __WORK_STEALING__
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(start_sample), (void*)&start_sample));
#endif
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, narg++, sizeof(num_parallel_samples), (void*)&num_parallel_samples));

		/* Set up arguments for ckPathTraceKernel_Subsurface_SPLIT_KERNEL */
#ifdef __SUBSURFACE__
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(sd), (void*)&sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(PathRadiance_coop), (void*)&PathRadiance_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(rng_coop), (void*)&rng_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(throughput_coop), (void*)&throughput_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(Ray_coop), (void*)&Ray_coop));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_Subsurface_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));
#endif

		/* Set up arguments for ckPathTraceKernel_DirectLighting_SPLIT_KERNEL kernel */
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(sd), (void*)&sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(sd_dl), (void*)&sd_dl));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(rng_coop), (void*)&rng_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(ISLamp_coop), (void*)&ISLamp_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(LightRay_coop), (void*)&LightRay_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(BSDFEval_coop), (void*)&BSDFEval_coop));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void*)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));

		/* Set up arguments for ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL kernel */
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(sd_shadow), (void*)&sd_shadow));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(LightRay_coop), (void*)&LightRay_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(AOLightRay_coop), (void*)&AOLightRay_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(Intersection_coop_AO), (void*)&Intersection_coop_AO));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(Intersection_coop_DL), (void*)&Intersection_coop_DL));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void*)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, narg++, sizeof(total_num_rays), (void*)&total_num_rays));

		/* Set up arguments for ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL kernel */
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(kgbuffer), (void*)&kgbuffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(sd), (void*)&sd));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(rng_coop), (void*)&rng_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(throughput_coop), (void*)&throughput_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(PathRadiance_coop), (void*)&PathRadiance_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(Ray_coop), (void*)&Ray_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(PathState_coop), (void*)&PathState_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(LightRay_coop), (void*)&LightRay_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(ISLamp_coop), (void*)&ISLamp_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(BSDFEval_coop), (void*)&BSDFEval_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(AOLightRay_coop), (void*)&AOLightRay_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(AOBSDF_coop), (void*)&AOBSDF_coop));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(AOAlpha_coop), (void*)&AOAlpha_coop));

		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(ray_state), (void*)&ray_state));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(Queue_data), (void*)&Queue_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(Queue_index), (void*)&Queue_index));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(dQueue_size), (void*)&dQueue_size));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, narg++, sizeof(use_queues_flag), (void*)&use_queues_flag));

		/* Set up arguments for ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL */
		narg = 0;
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL, narg++, sizeof(d_data), (void *)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL, narg++, sizeof(d_buffer), (void *)&d_buffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL, narg++, sizeof(per_sample_output_buffers), (void *)&per_sample_output_buffers));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL, narg++, sizeof(num_parallel_samples), (void *)&num_parallel_samples));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL, narg++, sizeof(d_w), (void *)&d_w));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL, narg++, sizeof(d_h), (void *)&d_h));
		opencl_assert(clSetKernelArg(ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL, narg++, sizeof(d_stride), (void *)&d_stride));

		/* Enqueue ckPathTraceKernel_DataInit_SPLIT_KERNEL kernel */
		opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_DataInit_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
		bool activeRaysAvailable = true;

		/* Record number of time host intervention has been made */
		unsigned int numHostIntervention = 0;
		unsigned int numNextPathIterTimes = PathIteration_times;
		while(activeRaysAvailable) {
		/* Twice the global work size of other kernels for ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL */
		size_t global_size_shadow_blocked[2];
		global_size_shadow_blocked[0] = global_size[0] * 2;
		global_size_shadow_blocked[1] = global_size[1];

			/* Do path-iteration in host [Enqueue Path-iteration kernels] */
			for(int PathIter = 0; PathIter < PathIteration_times; PathIter++) {
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_SceneIntersect_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_LampEmission_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_QueueEnqueue_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_BG_BufferUpdate_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_Shader_Lighting_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_Holdout_Emission_Blurring_Pathtermination_AO_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
#ifdef __SUBSURFACE__
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_Subsurface_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
#endif
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_DirectLighting_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_ShadowBlocked_DirectLighting_SPLIT_KERNEL, 2, NULL, global_size_shadow_blocked, local_size, 0, NULL, NULL));
				opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_SetUpNextIteration_SPLIT_KERNEL, 2, NULL, global_size, local_size, 0, NULL, NULL));
			}

			/* Read ray-state into Host memory to decide if we should exit path-iteration in host */
			ciErr = clEnqueueReadBuffer(cqCommandQueue, ray_state, CL_TRUE, 0, global_size[0] * global_size[1] * sizeof(char), hostRayStateArray, 0, NULL, NULL);
			assert(ciErr == CL_SUCCESS);

			activeRaysAvailable = false;

			for(int rayStateIter = 0; rayStateIter < global_size[0] * global_size[1]; rayStateIter++) {
				if(int8_t(hostRayStateArray[rayStateIter]) != RAY_INACTIVE) {
					/* Not all rays are RAY_INACTIVE */
					activeRaysAvailable = true;
					break;
				}
			}

			if(activeRaysAvailable) {
				numHostIntervention++;

				PathIteration_times = PATH_ITER_INC_FACTOR;

				/*
				 * Host intervention done before all rays become RAY_INACTIVE;
				 * Set do more initial iterations for the next tile
				 */
				numNextPathIterTimes += PATH_ITER_INC_FACTOR;
			}
		}

		/* Execute SumALLRadiance kernel to accumulate radiance calculated in per_sample_output_buffers into RenderTile's output buffer */
		size_t sum_all_radiance_local_size[2] = { 16, 16 };
		size_t sum_all_radiance_global_size[2];
		sum_all_radiance_global_size[0] = (((d_w - 1) / sum_all_radiance_local_size[0]) + 1) * sum_all_radiance_local_size[0];
		sum_all_radiance_global_size[1] = (((d_h - 1) / sum_all_radiance_local_size[1]) + 1) * sum_all_radiance_local_size[1];
		opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, ckPathTraceKernel_SumAllRadiance_SPLIT_KERNEL, 2, NULL, sum_all_radiance_global_size, sum_all_radiance_local_size, 0, NULL, NULL));

		if(numHostIntervention == 0) {
			/* This means that we are executing kernel more than required
			 * Must avoid this for the next sample/tile
			 */
			PathIteration_times = ((numNextPathIterTimes - PATH_ITER_INC_FACTOR) <= 0) ?
								PATH_ITER_INC_FACTOR : numNextPathIterTimes - PATH_ITER_INC_FACTOR;
		}
		else {
			/*
			 * Number of path-iterations done for this tile is set as
			 * Initial path-iteration times for the next tile
			 */
			PathIteration_times = numNextPathIterTimes;
		}

		first_tile = false;
#else
		/* sample arguments */
		cl_int d_sample = sample;
		cl_uint narg = 0;

		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_buffer), (void*)&d_buffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_rng_state), (void*)&d_rng_state));

#define KERNEL_TEX(type, ttype, name) \
	set_kernel_arg_mem(ckPathTraceKernel, &narg, #name);
#include "kernel_textures.h"

		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_sample), (void*)&d_sample));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_x), (void*)&d_x));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_y), (void*)&d_y));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_w), (void*)&d_w));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_h), (void*)&d_h));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_offset), (void*)&d_offset));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_stride), (void*)&d_stride));

		enqueue_kernel(ckPathTraceKernel, d_w, d_h);
#endif
	}

	void set_kernel_arg_mem(cl_kernel kernel, cl_uint *narg, const char *name)
	{
		cl_mem ptr;

		MemMap::iterator i = mem_map.find(name);
		if(i != mem_map.end()) {
			ptr = CL_MEM_PTR(i->second);
		}
		else {
			/* work around NULL not working, even though the spec says otherwise */
			ptr = CL_MEM_PTR(null_mem);
		}

		opencl_assert(clSetKernelArg(kernel, (*narg)++, sizeof(ptr), (void*)&ptr));
	}

	void film_convert(DeviceTask& task, device_ptr buffer, device_ptr rgba_byte, device_ptr rgba_half)
	{
#ifdef __SPLIT_KERNEL__
		(void)task;
		(void)buffer;
		(void)rgba_byte;
		(void)rgba_half;
#else
		/* cast arguments to cl types */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_rgba = (rgba_byte)? CL_MEM_PTR(rgba_byte): CL_MEM_PTR(rgba_half);
		cl_mem d_buffer = CL_MEM_PTR(buffer);
		cl_int d_x = task.x;
		cl_int d_y = task.y;
		cl_int d_w = task.w;
		cl_int d_h = task.h;
		cl_float d_sample_scale = 1.0f/(task.sample + 1);
		cl_int d_offset = task.offset;
		cl_int d_stride = task.stride;

		/* sample arguments */
		cl_uint narg = 0;

		cl_kernel ckFilmConvertKernel = (rgba_byte)? ckFilmConvertByteKernel: ckFilmConvertHalfFloatKernel;

		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_rgba), (void*)&d_rgba));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_buffer), (void*)&d_buffer));

#define KERNEL_TEX(type, ttype, name) \
	set_kernel_arg_mem(ckFilmConvertKernel, &narg, #name);
#include "kernel_textures.h"

		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_sample_scale), (void*)&d_sample_scale));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_x), (void*)&d_x));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_y), (void*)&d_y));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_w), (void*)&d_w));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_h), (void*)&d_h));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_offset), (void*)&d_offset));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_stride), (void*)&d_stride));



		enqueue_kernel(ckFilmConvertKernel, d_w, d_h);
#endif
	}

	void shader(DeviceTask& task)
	{
#ifdef __SPLIT_KERNEL__
		(void)task;
#else
		/* cast arguments to cl types */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_input = CL_MEM_PTR(task.shader_input);
		cl_mem d_output = CL_MEM_PTR(task.shader_output);
		cl_int d_shader_eval_type = task.shader_eval_type;
		cl_int d_shader_x = task.shader_x;
		cl_int d_shader_w = task.shader_w;
		cl_int d_offset = task.offset;

		/* sample arguments */
		cl_uint narg = 0;

		cl_kernel kernel;

		if(task.shader_eval_type >= SHADER_EVAL_BAKE)
			kernel = ckBakeKernel;
		else
			kernel = ckShaderKernel;

		for(int sample = 0; sample < task.num_samples; sample++) {

			if(task.get_cancel())
				break;

			cl_int d_sample = sample;

			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_data), (void*)&d_data));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_input), (void*)&d_input));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_output), (void*)&d_output));

#define KERNEL_TEX(type, ttype, name) \
		set_kernel_arg_mem(kernel, &narg, #name);
#include "kernel_textures.h"

			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_shader_eval_type), (void*)&d_shader_eval_type));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_shader_x), (void*)&d_shader_x));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_shader_w), (void*)&d_shader_w));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_offset), (void*)&d_offset));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_sample), (void*)&d_sample));

			enqueue_kernel(kernel, task.shader_w, 1);

			task.update_progress(NULL);
		}
#endif
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

			/* keep rendering tiles until done */
			while(task->acquire_tile(this, tile)) {

#ifdef __SPLIT_KERNEL__
				/* The second argument is dummy */
				path_trace(tile, 0);
				tile.sample = tile.start_sample + tile.num_samples;
#else
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
#endif
				task->release_tile(tile);
			}
		}
	}

	class OpenCLDeviceTask : public DeviceTask {
	public:
		OpenCLDeviceTask(OpenCLDevice *device, DeviceTask& task)
		: DeviceTask(task)
		{
			run = function_bind(&OpenCLDevice::thread_run, device, this);
		}
	};

	int get_split_task_count(DeviceTask& /*task*/)
	{
		return 1;
	}

	void task_add(DeviceTask& task)
	{
		task_pool.push(new OpenCLDeviceTask(this, task));
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

Device *device_opencl_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new OpenCLDevice(info, stats, background);
}

bool device_opencl_init(void) {
	static bool initialized = false;
	static bool result = false;

	if(initialized)
		return result;

	initialized = true;

	result = clewInit() == CLEW_SUCCESS;

	return result;
}

void device_opencl_info(vector<DeviceInfo>& devices)
{
	vector<cl_device_id> device_ids;
	cl_uint num_devices = 0;
	vector<cl_platform_id> platform_ids;
	cl_uint num_platforms = 0;

	/* get devices */
	if(clGetPlatformIDs(0, NULL, &num_platforms) != CL_SUCCESS || num_platforms == 0)
		return;

	platform_ids.resize(num_platforms);

	if(clGetPlatformIDs(num_platforms, &platform_ids[0], NULL) != CL_SUCCESS)
		return;

	/* devices are numbered consecutively across platforms */
	int num_base = 0;

	for(int platform = 0; platform < num_platforms; platform++, num_base += num_devices) {
		num_devices = 0;
		if(clGetDeviceIDs(platform_ids[platform], opencl_device_type(), 0, NULL, &num_devices) != CL_SUCCESS || num_devices == 0)
			continue;

		device_ids.resize(num_devices);

		if(clGetDeviceIDs(platform_ids[platform], opencl_device_type(), num_devices, &device_ids[0], NULL) != CL_SUCCESS)
			continue;

		char pname[256];
		clGetPlatformInfo(platform_ids[platform], CL_PLATFORM_NAME, sizeof(pname), &pname, NULL);
		string platform_name = pname;

		/* add devices */
		for(int num = 0; num < num_devices; num++) {
			cl_device_id device_id = device_ids[num];
			char name[1024] = "\0";

			if(clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(name), &name, NULL) != CL_SUCCESS)
				continue;

			DeviceInfo info;

			info.type = DEVICE_OPENCL;
			info.description = string(name);
			info.num = num_base + num;
			info.id = string_printf("OPENCL_%d", info.num);
			/* we don't know if it's used for display, but assume it is */
			info.display_device = true;
			info.advanced_shading = opencl_kernel_use_advanced_shading(platform_name);
			info.pack_images = true;

			devices.push_back(info);
		}
	}
}

string device_opencl_capabilities(void)
{
	/* TODO(sergey): Not implemented yet. */
	return "";
}

CCL_NAMESPACE_END

#endif /* WITH_OPENCL */
