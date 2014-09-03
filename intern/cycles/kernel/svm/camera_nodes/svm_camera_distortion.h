/*
 * Copyright 2011-2013 Blender Foundation
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

/* Polynomial distortion */

ccl_device void svm_node_camera_polynomial_distortion(KernelGlobals *kg,
                                                      CameraData *cd,
                                                      uint4 node,
                                                      float *stack,
                                                      int *offset)
{
	/* Read extra data. */
	uint4 node1 = read_node(kg, offset);
	const float k1 = __int_as_float(node.y);
	const float k2 = __int_as_float(node.z);
	const float k3 = __int_as_float(node.w);
	const int invert = __float_as_int(node1.y);
	const int raster_in_offset = node1.z;
	const int raster_out_offset =node1.w;
	const float focal_length = kernel_data.cam.focal_length *
		kernel_data.cam.width / kernel_data.cam.sensorwidth;

	float2 raster_in, raster_out;
	raster_in = stack_load_float2(stack, raster_in_offset);

	float2 principal_point = make_float2(kernel_data.cam.width * 0.5f,
	                                     kernel_data.cam.height * 0.5f);

	if(invert) {
		util_invert_polynomial_distortion(raster_in,
		                                  focal_length,
		                                  principal_point,
		                                  k1, k2, k3,
		                                  &raster_out);
	}
	else {
		util_apply_polynomial_distortion(raster_in,
		                                 focal_length,
		                                 principal_point,
		                                 k1, k2, k3,
		                                 &raster_out);
	}

	stack_store_float2(stack, raster_out_offset, raster_out);
}

CCL_NAMESPACE_END
