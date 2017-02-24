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
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* TODO(lukas): Implement the actual colormanaged functions. */

ccl_device float3 xyz_to_rgb(KernelGlobals *kg, float3 xyz)
{
	if(kernel_data.film.colorspace == COLORSPACE_REC709) {
		return xyz_to_rec709(xyz);
	}
	else {
		return transform_direction(&kernel_data.film.xyz_to_rgb, xyz);
	}
}

ccl_device float linear_rgb_to_gray(KernelGlobals *kg, float3 c)
{
	if(kernel_data.film.colorspace == COLORSPACE_REC709) {
		return rec709_to_gray(c);
	}
	else {
		return dot(c, kernel_data.film.rgb_to_y);
	}
}

ccl_device float3 rec709_to_scene_linear(KernelGlobals *kg, float3 c)
{
	if(kernel_data.film.colorspace == COLORSPACE_REC709) {
		return c;
	}
	else {
		return transform_direction(&kernel_data.film.xyz_to_rgb, rec709_to_xyz(c));
	}
}

CCL_NAMESPACE_END
