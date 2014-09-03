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

/* Sample perspective node */

ccl_device void svm_camera_node_sample_perspective(KernelGlobals *kg,
                                                   CameraData *cd,
                                                   float *stack,
                                                   uint4 node)
{
	uint raster_offset, lens_offset, time_offset;
	decode_node_uchar4(node.y, &raster_offset, &lens_offset, &time_offset, NULL);

	/* TODO(sergey): Make sure unconnected sockets are fine here. */
	float2 raster = stack_load_float2(stack, raster_offset);
	float2 lens = stack_load_float2(stack, lens_offset);
	Ray ray;
	if(stack_valid(time_offset)) {
		ray.time = stack_load_float(stack, time_offset);
	}
	else {
		ray.time = TIME_INVALID;
	}
	camera_sample_perspective(kg, raster.x, raster.y, lens.x, lens.y, &ray);

	uint ray_origin_offset, ray_direction_offset, ray_length_offset;
	decode_node_uchar4(node.z,
	                   &ray_origin_offset,
	                   &ray_direction_offset,
	                   &ray_length_offset,
	                   NULL);

	if(stack_valid(ray_origin_offset)) {
		stack_store_float3(stack, ray_origin_offset, ray.P);
	}
	if(stack_valid(ray_direction_offset)) {
		stack_store_float3(stack, ray_direction_offset, ray.D);
	}
	if(stack_valid(ray_length_offset)) {
		stack_store_float(stack, ray_length_offset, ray.t);
	}
}

CCL_NAMESPACE_END
