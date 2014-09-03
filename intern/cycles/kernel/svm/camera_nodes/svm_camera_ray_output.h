/*
 * Copyright 2011-2014 Blender Foundation
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
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

/* Ray output node */

ccl_device void svm_camera_node_ray_output(KernelGlobals *kg,
                                           CameraData *cd,
                                           float *stack,
                                           uint4 node)
{
	uint ray_origin_offset, ray_direction_offset, ray_length_offset;
	uint time_offset;

	decode_node_uchar4(node.y,
	                   &ray_origin_offset,
	                   &ray_direction_offset,
	                   &ray_length_offset,
	                   &time_offset);

	cd->ray.P = stack_load_float3(stack, ray_origin_offset);
	cd->ray.D = stack_load_float3(stack, ray_direction_offset);
	cd->ray.t = stack_load_float(stack, ray_length_offset);
	cd->ray.time = stack_load_float(stack, time_offset);
}

CCL_NAMESPACE_END
