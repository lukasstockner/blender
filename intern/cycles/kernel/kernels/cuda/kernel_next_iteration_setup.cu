/*
 * Copyright 2011-2015 Blender Foundation
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
#define __KERNEL_CUDA__
#define __KERNEL_CUDA_SPLIT__
#define __SPLIT_KERNEL__

#include "split/kernel_next_iteration_setup.h"

extern "C" 
__global__ void kernel_cuda_path_trace_next_iteration_setup(
        ccl_global char *sd,                  /* Required for setting up ray for next iteration */
        ccl_global uint *rng_coop,            /* Required for setting up ray for next iteration */
        ccl_global float3 *throughput_coop,   /* Required for setting up ray for next iteration */
        PathRadiance *PathRadiance_coop,      /* Required for setting up ray for next iteration */
        ccl_global Ray *Ray_coop,             /* Required for setting up ray for next iteration */
        ccl_global PathState *PathState_coop, /* Required for setting up ray for next iteration */
        ccl_global Ray *LightRay_dl_coop,     /* Required for radiance update - direct lighting */
        ccl_global int *ISLamp_coop,          /* Required for radiance update - direct lighting */
        ccl_global BsdfEval *BSDFEval_coop,   /* Required for radiance update - direct lighting */
        ccl_global Ray *LightRay_ao_coop,     /* Required for radiance update - AO */
        ccl_global float3 *AOBSDF_coop,       /* Required for radiance update - AO */
        ccl_global float3 *AOAlpha_coop,      /* Required for radiance update - AO */
        ccl_global char *ray_state,           /* Denotes the state of each ray */
        ccl_global int *Queue_data,           /* Queue memory */
        ccl_global int *Queue_index,          /* Tracks the number of elements in each queue */
        int queuesize,                        /* Size (capacity) of each queue */
        ccl_global char *use_queues_flag)     /* flag to decide if scene_intersect kernel should
                                               * use queues to fetch ray index */
{
	ccl_local_var unsigned int local_queue_atomics;
	if(ccl_local_thread_x == 0 && ccl_local_thread_y == 0) {
		local_queue_atomics = 0;
	}
	ccl_local_barrier();

	if(ccl_thread_x == 0 && ccl_thread_y == 0) {
		/* If we are here, then it means that scene-intersect kernel
		* has already been executed atleast once. From the next time,
		* scene-intersect kernel may operate on queues to fetch ray index
		*/
		use_queues_flag[0] = 1;

		/* Mark queue indices of QUEUE_SHADOW_RAY_CAST_AO_RAYS and
		 * QUEUE_SHADOW_RAY_CAST_DL_RAYS queues that were made empty during the
		 * previous kernel.
		 */
		Queue_index[QUEUE_SHADOW_RAY_CAST_AO_RAYS] = 0;
		Queue_index[QUEUE_SHADOW_RAY_CAST_DL_RAYS] = 0;
	}

	char enqueue_flag = 0;
	int ray_index = ccl_thread_y*ccl_size_x + ccl_thread_x;
	ray_index = get_ray_index(ray_index,
	                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                          Queue_data,
	                          queuesize,
	                          0);

	if(ray_index != QUEUE_EMPTY_SLOT) {
		enqueue_flag = kernel_next_iteration_setup(NULL,
		                                           (ShaderData *)sd,
		                                           rng_coop,
		                                           throughput_coop,
		                                           PathRadiance_coop,
		                                           Ray_coop,
		                                           PathState_coop,
		                                           LightRay_dl_coop,
		                                           ISLamp_coop,
		                                           BSDFEval_coop,
		                                           LightRay_ao_coop,
		                                           AOBSDF_coop,
		                                           AOAlpha_coop,
		                                           ray_state,
		                                           use_queues_flag,
		                                           ray_index);
	}

	/* Enqueue RAY_UPDATE_BUFFER rays. */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                        enqueue_flag,
	                        queuesize,
	                        &local_queue_atomics,
	                        Queue_data,
	                        Queue_index);
}
