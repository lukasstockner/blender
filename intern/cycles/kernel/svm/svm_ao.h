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
                                 float3 N,
                                 ccl_addr_space PathState *state,
                                 float max_dist,
                                 int num_samples,
                                 int flags)
{
	/* Early out if no sampling needed. */
	if(max_dist <= 0.0f || num_samples < 1 || sd->object == OBJECT_NONE) {
		return 0.0f;
	}

	float3 localX, localY, localZ;
	localZ = sd->Ng;

	if(flags & NODE_AO_INSIDE) {
		localZ = -localZ;
		N = -N;
	}

	make_orthonormals(localZ, &localX, &localY);

	float3 V, T1, T2;
	float a;

	float cosN = dot(N, localZ);
	bool sample_vndf = (cosN < 0.9999f);
	if(sample_vndf) {
		V = make_float3(dot(N, localX), dot(N, localY), cosN);
		/* Calculate orthonormal basis around V (eq. 1 and 2). */
		float plen = sqrtf(sqr(V.x) + sqr(V.y));
		float fac = 1.0f / plen;
		T1 = make_float3(fac*V.y, -fac*V.x, 0.0f);
		T2 = make_float3(-fac*V.x*V.z, -fac*V.y*V.z, plen);

		/* Evaluate probability of sampling the V-orthogonal half disk. */
		a = 1.0f / (1.0f + V.z);
	}

	int unoccluded = 0;
	for(int sample = 0; sample < num_samples; sample++) {
		float disk_u, disk_v;
		path_branched_rng_2D(kg, state->rng_hash, state, sample, num_samples,
		                     PRNG_BEVEL_U, &disk_u, &disk_v);

		float3 localD;
		if(sample_vndf) {
			float r = sqrtf(disk_u);
			float phi = M_PI_F * ((disk_v < a)? (disk_v/a) : (1.0f + (disk_v - a)/(1.0f - a)));
			float2 P = r*make_float2(cosf(phi), sinf(phi));
			if(disk_v >= a) {
				P.y *= V.z;
			}
			localD = P.x*T1 + P.y*T2 + sqrtf(max(0.0f, 1.0f - dot(P, P)))*V;
			localD.z = max(0.0f, localD.z);
		}
		else {
			float2 d = concentric_sample_disk(disk_u, disk_v);
			localD = make_float3(d.x, d.y, safe_sqrtf(1.0f - dot(d, d)));
		}

		/* Create ray. */
		Ray ray;
		ray.P = ray_offset(sd->P, localZ);
		ray.D = localD.x*localX + localD.y*localY + localD.z*localZ;
		ray.t = max_dist;
		ray.time = sd->time;

		if(flags & NODE_AO_ONLY_LOCAL) {
			if(!scene_intersect_local(kg,
			                          ray,
			                          NULL,
			                          sd->object,
			                          NULL,
			                          0)) {
				unoccluded++;
			}
		}
		else {
			Intersection isect;
			if(!scene_intersect(kg,
			                    ray,
			                    PATH_RAY_SHADOW_OPAQUE,
			                    &isect,
			                    NULL,
			                    0.0f, 0.0f)) {
				unoccluded++;
			}
		}
	}

	return ((float) unoccluded) / num_samples;
}

ccl_device void svm_node_ao(KernelGlobals *kg,
                            ShaderData *sd,
                            ccl_addr_space PathState *state,
                            float *stack,
                            uint4 node)
{
	uint flags, radius_offset, normal_offset, out_offset;
	decode_node_uchar4(node.y, &flags, &radius_offset, &normal_offset, &out_offset);

	float radius = stack_load_float_default(stack, radius_offset, node.z);
	float3 normal = stack_valid(normal_offset)? stack_load_float3(stack, normal_offset): sd->N;

	float fac = svm_ao(kg, sd, normal, state, radius, node.w, flags);

	stack_store_float(stack, out_offset, fac);
}

CCL_NAMESPACE_END

