/*
* Copyright 2011-2018 Blender Foundation
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

CCL_NAMESPACE_BEGIN

ccl_device_noinline float svm_ao(KernelGlobals *kg,
                                 ShaderData *sd,
                                 ccl_addr_space PathState *state,
                                 float max_dist,
                                 int num_samples)
{
	/* Early out if no sampling needed. */
	if(max_dist <= 0.0f || num_samples < 1 || sd->object == OBJECT_NONE) {
		return 0.0f;
	}

	int hits = 0;
	float3 localX, localY, localZ;
	localZ = sd->Ng;
	make_orthonormals(localZ, &localX, &localY);

	for(int sample = 0; sample < num_samples; sample++) {
		float disk_u, disk_v;
		path_branched_rng_2D(kg, state->rng_hash, state, sample, num_samples,
		                     PRNG_BEVEL_U, &disk_u, &disk_v);

		float2 d = concentric_sample_disk(disk_u, disk_v);
		float cos_theta = safe_sqrtf(1.0f - dot(d, d));

		/* Create ray. */
		Ray ray;
		ray.P = ray_offset(sd->P, sd->Ng);
		ray.D = d.x*localX + d.y*localY + cos_theta*localZ;
		ray.t = max_dist;
		ray.time = sd->time;

		if(scene_intersect_local(kg,
		                         ray,
		                         NULL,
		                         sd->object,
		                         NULL,
		                         0)) {
			hits++;
		}
	}

	return ((float) hits) / num_samples;
}

ccl_device void svm_node_ao(KernelGlobals *kg,
                               ShaderData *sd,
                               ccl_addr_space PathState *state,
                               float *stack,
                               uint4 node)
{
	uint num_samples, radius_offset, out_offset, dummy;
	decode_node_uchar4(node.y, &num_samples, &radius_offset, &out_offset, &dummy);

	float radius = stack_load_float(stack, radius_offset);
	float fac = svm_ao(kg, sd, state, radius, num_samples);

	stack_store_float(stack, out_offset, fac);
}

CCL_NAMESPACE_END

