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

#include "split/kernel_split_common.h"

/*
 * The kernel "kernel_queue_enqueue" enqueues rays of
 * different ray state into their appropriate Queues;
 * 1. Rays that have been determined to hit the background from the
 * "kernel_scene_intersect" kernel
 * are enqueued in QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS;
 * 2. Rays that have been determined to be actively participating in path-iteration will be enqueued into QUEUE_ACTIVE_AND_REGENERATED_RAYS.
 *
 * The input and output of the kernel is as follows,
 *
 * ray_state -------------------------------------------|--- kernel_queue_enqueue --|--- Queue_data (QUEUE_ACTIVE_AND_REGENERATED_RAYS & QUEUE_HITBF_BUFF_UPDATE_TOREGEN_RAYS)
 * Queue_index(QUEUE_ACTIVE_AND_REGENERATED_RAYS) ------|                           |--- Queue_index (QUEUE_ACTIVE_AND_REGENERATED_RAYS & QUEUE_HITBF_BUFF_UPDATE_TOREGEN_RAYS)
 * Queue_index(QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS) ---|                           |
 * queuesize -------------------------------------------|                           |
 *
 * Note on Queues :
 * State of queues during the first time this kernel is called :
 * At entry,
 * Both QUEUE_ACTIVE_AND_REGENERATED_RAYS and QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be empty.
 * At exit,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE rays
 * QUEUE_HITBF_BUFF_UPDATE_TOREGEN_RAYS will be filled with RAY_HIT_BACKGROUND rays.
 *
 * State of queue during other times this kernel is called :
 * At entry,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be empty.
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will contain RAY_TO_REGENERATE and RAY_UPDATE_BUFFER rays.
 * At exit,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE rays.
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with RAY_TO_REGENERATE, RAY_UPDATE_BUFFER, RAY_HIT_BACKGROUND rays.
 */

extern "C" 
__global__ void kernel_cuda_path_trace_queue_enqueue(
        ccl_global int *Queue_data,   /* Queue memory */
        ccl_global int *Queue_index,  /* Tracks the number of elements in each queue */
        ccl_global char *ray_state,   /* Denotes the state of each ray */
        int queuesize)                /* Size (capacity) of each queue */
{
	/* We have only 2 cases (Hit/Not-Hit) */
	ccl_local_var unsigned int local_queue_atomics[2];

	int lidx = ccl_local_thread_y*ccl_local_size_x + ccl_local_thread_x;
	int ray_index = ccl_thread_y*ccl_size_x + ccl_thread_x;

	if(lidx < 2 ) {
		local_queue_atomics[lidx] = 0;
	}
	ccl_local_barrier();

	int queue_number = -1;

	if(IS_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND)) {
		queue_number = QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS;
	}
	else if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		queue_number = QUEUE_ACTIVE_AND_REGENERATED_RAYS;
	}

	unsigned int my_lqidx;
	if(queue_number != -1) {
		my_lqidx = get_local_queue_index(queue_number, local_queue_atomics);
	}
	ccl_local_barrier();

	if(lidx == 0) {
		local_queue_atomics[QUEUE_ACTIVE_AND_REGENERATED_RAYS] =
		        get_global_per_queue_offset(QUEUE_ACTIVE_AND_REGENERATED_RAYS,
		                                    local_queue_atomics,
		                                    Queue_index);
		local_queue_atomics[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] =
		        get_global_per_queue_offset(QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
		                                    local_queue_atomics,
		                                    Queue_index);
	}
	ccl_local_barrier();

	unsigned int my_gqidx;
	if(queue_number != -1) {
		my_gqidx = get_global_queue_index(queue_number,
		                                  queuesize,
		                                  my_lqidx,
		                                  local_queue_atomics);
		Queue_data[my_gqidx] = ray_index;
	}
}
