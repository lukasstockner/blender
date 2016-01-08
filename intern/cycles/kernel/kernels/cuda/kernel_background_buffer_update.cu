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

#include "split/kernel_background_buffer_update.h"

extern "C" 
__global__ void kernel_cuda_path_trace_background_buffer_update(
        ccl_global char *sd,
        ccl_global float *per_sample_output_buffers,
        ccl_global uint *rng_state,
        ccl_global uint *rng_coop,             /* Required for buffer Update */
        ccl_global float3 *throughput_coop,    /* Required for background hit processing */
        PathRadiance *PathRadiance_coop,       /* Required for background hit processing and buffer Update */
        ccl_global Ray *Ray_coop,              /* Required for background hit processing */
        ccl_global PathState *PathState_coop,  /* Required for background hit processing */
        ccl_global float *L_transparent_coop,  /* Required for background hit processing and buffer Update */
        ccl_global char *ray_state,            /* Stores information on the current state of a ray */
        int sw, int sh, int sx, int sy, int stride,
        int rng_state_offset_x,
        int rng_state_offset_y,
        int rng_state_stride,
        ccl_global unsigned int *work_array,   /* Denotes work of each ray */
        ccl_global int *Queue_data,            /* Queues memory */
        ccl_global int *Queue_index,           /* Tracks the number of elements in each queue */
        int queuesize,                         /* Size (capacity) of each queue */
        int end_sample,
        int start_sample,
        int parallel_samples                  /* Number of samples to be processed in parallel */
#ifdef __WORK_STEALING__
        , ccl_global unsigned int *work_pool_wgs,
        unsigned int num_samples
#endif
#ifdef __KERNEL_DEBUG__
        , DebugData *debugdata_coop
#endif
) {
	ccl_local_var unsigned int local_queue_atomics;
	if(ccl_local_thread_x == 0 && ccl_local_thread_y == 0) {
		local_queue_atomics = 0;
	}
	ccl_local_barrier();

	int ray_index = ccl_thread_y*ccl_size_x + ccl_thread_x;
	if(ray_index == 0) {
		/* We will empty this queue in this kernel. */
		Queue_index[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] = 0;
	}
	char enqueue_flag = 0;
	ray_index = get_ray_index(ray_index,
	                          QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                          Queue_data,
	                          queuesize,
	                          1);

	/* TODO(lukas): Maybe exit earlier? barrier is problematic, though... */

	if(ray_index != QUEUE_EMPTY_SLOT) {
		enqueue_flag =
			kernel_background_buffer_update(NULL,
			                                (ShaderData *)sd,
			                                per_sample_output_buffers,
			                                rng_state,
			                                rng_coop,
			                                throughput_coop,
			                                PathRadiance_coop,
			                                Ray_coop,
			                                PathState_coop,
			                                L_transparent_coop,
			                                ray_state,
			                                sw, sh, sx, sy, stride,
			                                rng_state_offset_x,
			                                rng_state_offset_y,
			                                rng_state_stride,
			                                work_array,
			                                end_sample,
			                                start_sample,
#ifdef __WORK_STEALING__
			                                work_pool_wgs,
			                                num_samples,
#endif
#ifdef __KERNEL_DEBUG__
			                                debugdata_coop,
#endif
			                                parallel_samples,
			                                ray_index);
	}

	/* Enqueue RAY_REGENERATED rays into QUEUE_ACTIVE_AND_REGENERATED_RAYS;
	 * These rays will be made active during next SceneIntersectkernel.
	 */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                        enqueue_flag,
	                        queuesize,
	                        &local_queue_atomics,
	                        Queue_data,
	                        Queue_index);
}
