/*
 * Adapted from code Copyright 2009-2010 NVIDIA Corporation,
 * and code copyright 2009-2012 Intel Corporation
 *
 * Modifications Copyright 2011-2014, Blender Foundation.
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

/* This is a template QBVH traversal function, where various features can be
 * enabled/disabled. This way we can compile optimized versions for each case
 * without new features slowing things down.
 *
 * BVH_INSTANCING: object instancing
 * BVH_HAIR: hair curve rendering
 * BVH_HAIR_MINIMUM_WIDTH: hair curve rendering with minimum width
 * BVH_MOTION: motion blur rendering
 *
 */

#define FEATURE(f) (((BVH_FUNCTION_FEATURES) & (f)) != 0)

ccl_device bool BVH_FUNCTION_NAME
(KernelGlobals *kg, const Ray *ray, Intersection *isect, const uint visibility
#if FEATURE(BVH_HAIR_MINIMUM_WIDTH)
,uint *lcg_state, float difl, float extmax
#endif
)
{
	/* todo:
	 * - Visibility test
	 * - Subsurface Scattering and Hair Min Width
	 * - Check on Embree traversal code for further improvements
	 */

	/* traversal stack in CUDA thread-local memory */
	int traversalStack[BVH_STACK_SIZE];
	traversalStack[0] = ENTRYPOINT_SENTINEL;

	/* traversal variables in registers */
	int stackPtr = 0;
	int nodeAddr = kernel_data.bvh.root;

	/* ray parameters in registers */
	float3 P = ray->P;
	float3 dir = bvh_clamp_direction(ray->D);
	float3 idir = bvh_inverse_direction(dir);
	int object = OBJECT_NONE;

#if FEATURE(BVH_MOTION)
	Transform ob_tfm;
#endif

	isect->t = ray->t;
	isect->object = OBJECT_NONE;
	isect->prim = PRIM_NONE;
	isect->u = 0.0f;
	isect->v = 0.0f;

	/* traversal loop */
	do {
		do
		{
			/* traverse internal nodes */
			while(nodeAddr >= 0 && nodeAddr != ENTRYPOINT_SENTINEL)
			{
				int traverseChild, nodeAddrChild[4];

				qbvh_node_intersect(kg, &traverseChild, nodeAddrChild, P, idir, isect->t, nodeAddr);

				if(traverseChild & 1) {
					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild[0];
				}
				if(traverseChild & 2) {
					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild[1];
				}
				if(traverseChild & 4) {
					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild[2];
				}
				if(traverseChild & 8) {
					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild[3];
				}

				nodeAddr = traversalStack[stackPtr];
				--stackPtr;
			}

			/* if node is leaf, fetch triangle list */
			if(nodeAddr < 0) {
				float4 leaf = kernel_tex_fetch(__bvh_nodes, (-nodeAddr-1)*QBVH_NODE_SIZE+(QBVH_NODE_SIZE-2));
				int primAddr = __float_as_int(leaf.x);

#if FEATURE(BVH_INSTANCING)
				if(primAddr >= 0) {
#endif
					int primAddr2 = __float_as_int(leaf.y);

					/* pop */
					nodeAddr = traversalStack[stackPtr];
					--stackPtr;

					/* triangle intersection */
					while(primAddr < primAddr2) {
						bool hit;
						uint type = kernel_tex_fetch(__prim_type, primAddr);

						switch(type & PRIMITIVE_ALL) {
							case PRIMITIVE_TRIANGLE: {
								hit = triangle_intersect(kg, isect, P, dir, visibility, object, primAddr);
								break;
							}
#if FEATURE(BVH_MOTION)
							case PRIMITIVE_MOTION_TRIANGLE: {
								hit = motion_triangle_intersect(kg, isect, P, dir, ray->time, visibility, object, primAddr);
								break;
							}
#endif
#if FEATURE(BVH_HAIR)
							case PRIMITIVE_CURVE:
							case PRIMITIVE_MOTION_CURVE: {
								if(kernel_data.curve.curveflags & CURVE_KN_INTERPOLATE)
									hit = bvh_cardinal_curve_intersect(kg, isect, P, dir, visibility, object, primAddr, ray->time, type, lcg_state, difl, extmax);
								else
									hit = bvh_curve_intersect(kg, isect, P, dir, visibility, object, primAddr, ray->time, type, lcg_state, difl, extmax);
								break;
							}
#endif
							default:
								hit = false;
								break;
						}

						/* shadow ray early termination */
						if(hit && visibility == PATH_RAY_SHADOW_OPAQUE)
							return true;

						primAddr++;
					}
#if FEATURE(BVH_INSTANCING)
				}
				else {
						/* instance push */
						object = kernel_tex_fetch(__prim_object, -primAddr-1);

#if FEATURE(BVH_MOTION)
						bvh_instance_motion_push(kg, object, ray, &P, &dir, &idir, &isect->t, &ob_tfm);
#else
						bvh_instance_push(kg, object, ray, &P, &dir, &idir, &isect->t);
#endif
						++stackPtr;
						traversalStack[stackPtr] = ENTRYPOINT_SENTINEL;

						nodeAddr = kernel_tex_fetch(__object_node, object);
					}
#endif
			}
		} while(nodeAddr != ENTRYPOINT_SENTINEL);

#if FEATURE(BVH_INSTANCING)
		if(stackPtr >= 0) {
			kernel_assert(object != OBJECT_NONE);

			/* instance pop */
#if FEATURE(BVH_MOTION)
			bvh_instance_motion_pop(kg, object, ray, &P, &dir, &idir, &isect->t, &ob_tfm);
#else
			bvh_instance_pop(kg, object, ray, &P, &dir, &idir, &isect->t);
#endif

			object = OBJECT_NONE;
			nodeAddr = traversalStack[stackPtr];
			--stackPtr;
		}
#endif

	} while(nodeAddr != ENTRYPOINT_SENTINEL);

	return (isect->prim != PRIM_NONE);
}

#undef FEATURE
#undef BVH_FUNCTION_NAME
#undef BVH_FUNCTION_FEATURES
