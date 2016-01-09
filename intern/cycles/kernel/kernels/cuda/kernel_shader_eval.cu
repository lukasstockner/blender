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

#include "split/kernel_shader_eval.h"
#include "kernel_split.cuh"

extern "C" 
SPLIT_KERNEL_BOUNDS
__global__ void kernel_cuda_path_trace_shader_eval(
        ccl_global char *sd,                   /* Output ShaderData structure to be filled */
        ccl_global uint *rng_coop,             /* Required for rbsdf calculation */
        ccl_global Ray *Ray_coop,              /* Required for setting up shader from ray */
        ccl_global PathState *PathState_coop,  /* Required for all functions in this kernel */
        Intersection *Intersection_coop,       /* Required for setting up shader from ray */
        ccl_global char *ray_state,            /* Denotes the state of each ray */
        ccl_global int *Queue_data,            /* queue memory */
        ccl_global int *Queue_index,           /* Tracks the number of elements in each queue */
        int queuesize)                         /* Size (capacity) of each queue */
{
	/* Enqeueue RAY_TO_REGENERATE rays into QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue. */
	ccl_local_var unsigned int local_queue_atomics;
	if(ccl_local_thread_x == 0 && ccl_local_thread_y == 0) {
		local_queue_atomics = 0;
	}
	ccl_local_barrier();

	int ray_index = ccl_thread_y*ccl_size_x + ccl_thread_x;
	ray_index = get_ray_index(ray_index,
	                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                          Queue_data,
	                          queuesize,
	                          0);

	char enqueue_flag = (IS_STATE(ray_state, ray_index, RAY_TO_REGENERATE)) ? 1 : 0;
	enqueue_ray_index_local(ray_index,
		                    QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
		                    enqueue_flag,
		                    queuesize,
		                    &local_queue_atomics,
		                    Queue_data,
		                    Queue_index);

	/* TODO(lukas): Seems to conflict with queue code below? */
	if(ray_index != QUEUE_EMPTY_SLOT) {

		/* Continue on with shader evaluation. */

		kernel_shader_eval(NULL,
			               (ShaderData *)sd,
			               rng_coop,
			               Ray_coop,
			               PathState_coop,
			               Intersection_coop,
			               ray_state,
			               ray_index);
	}
}
