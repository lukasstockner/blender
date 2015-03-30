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

#include "kernel_split.h"

/* __SUBSURFACE__ feature is turned off */

/*
* This is the seventh kernel in the ray-tracing logic. This is the sixth of the path-iteration kernels */

__kernel void kernel_ocl_path_trace_Subsurface_SPLIT_KERNEL(
	ccl_global char *globals,
	ccl_constant KernelData *data,
	ccl_global char *shader_data,
	ccl_global PathRadiance *PathRadiance_coop,
	ccl_global PathState *PathState_coop,
	ccl_global uint *rng_coop,
	ccl_global float3 *throughput_coop,
	ccl_global Ray *Ray_coop,
	ccl_global char *ray_state,                  /* Denotes the state of each ray */
	ccl_global int *Queue_data,                  /* Queue memory */
	int queuesize                                /* Size (capacity) of each queue */
	)
{
	/* Load kernel globals structure */
	ccl_global KernelGlobals *kg = (ccl_global KernelGlobals *)globals;
	ccl_global ShaderData *sd = (ccl_global ShaderData *)shader_data;

	int thread_index = get_global_id(1) * get_global_size(0) + get_global_id(0);
	int ray_index = get_ray_index(thread_index, QUEUE_ACTIVE_AND_REGENERATED_RAYS, Queue_data, queuesize, 0);

	if(ray_index == QUEUE_EMPTY_SLOT) {
		return;
	}

	ccl_global uint *rng = 0x0;
	ccl_global float3 *throughput = 0x0;
	ccl_global PathRadiance *L = 0x0;
	ccl_global PathState *state = 0x0;

	/* Get pointers to this ray's datastructures */
	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {

		rng = &rng_coop[ray_index];
		throughput = &throughput_coop[ray_index];
		state = &PathState_coop[ray_index];
		L = &PathRadiance_coop[ray_index];

#ifdef __SUBSURFACE__
		/* bssrdf scatter to a different location on the same object, replacing
		 * the closures with a diffuse BSDF */
		if(sd.flag & SD_BSSRDF) {
			if(kernel_path_subsurface_scatter(kg, sd, L, state, rng, &Ray_coop[ray_index], *throughput))
				break;
		}
#endif
	}
}
