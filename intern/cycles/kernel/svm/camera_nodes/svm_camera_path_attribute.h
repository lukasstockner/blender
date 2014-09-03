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

/* Path attribute node */

ccl_device void svm_camera_node_path_attribute(KernelGlobals *kg,
                                               CameraData *cd,
                                               float *stack,
                                               uint type,
                                               uint out_offset)
{
	switch(type) {
		case NODE_CAMERA_PATH_ATTRIBUTE_RASTER:
			stack_store_float2(stack, out_offset, cd->raster);
			break;
		case NODE_CAMERA_PATH_ATTRIBUTE_LENS:
			stack_store_float2(stack, out_offset, cd->lens);
			break;
		case NODE_CAMERA_PATH_ATTRIBUTE_TIME:
			stack_store_float(stack, out_offset, cd->ray.time);
			break;
	}

}

CCL_NAMESPACE_END
