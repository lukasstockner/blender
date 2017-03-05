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

ccl_device_inline float4 color_transform(KernelGlobals *kg, float4 color, int data)
{
	int lut = data & COLORSPACE_LUT_MASK;
	if(data & COLORSPACE_USE_LUT_FLAG) {
		color.x = kernel_tex_fetch(__color_management, COLORSPACE_TRANSFORM_NUM*9 + lut*256 + float_to_byte(color.x));
		color.y = kernel_tex_fetch(__color_management, COLORSPACE_TRANSFORM_NUM*9 + lut*256 + float_to_byte(color.y));
		color.z = kernel_tex_fetch(__color_management, COLORSPACE_TRANSFORM_NUM*9 + lut*256 + float_to_byte(color.z));
	}
	else if(lut) {
		switch(lut) {
			case 1:
				color.x = color_srgb_to_linear(color.x);
				color.y = color_srgb_to_linear(color.y);
				color.z = color_srgb_to_linear(color.z);
				break;
			default:
				kernel_assert(false);
				break;
		}
	}

	if(data & COLORSPACE_USE_TRANSFORM_FLAG) {
		int transform = (data & COLORSPACE_TRANSFORM_MASK) >> COLORSPACE_TRANSFORM_SHIFT;
		float4 transformed_color;
		transformed_color.x = kernel_tex_fetch(__color_management, transform*9+0) * color.x +
		                      kernel_tex_fetch(__color_management, transform*9+1) * color.y +
		                      kernel_tex_fetch(__color_management, transform*9+2) * color.z;
		transformed_color.y = kernel_tex_fetch(__color_management, transform*9+3) * color.x +
		                      kernel_tex_fetch(__color_management, transform*9+4) * color.y +
		                      kernel_tex_fetch(__color_management, transform*9+5) * color.z;
		transformed_color.z = kernel_tex_fetch(__color_management, transform*9+6) * color.x +
		                      kernel_tex_fetch(__color_management, transform*9+7) * color.y +
		                      kernel_tex_fetch(__color_management, transform*9+8) * color.z;
		transformed_color.w = color.w;
		return transformed_color;
	}
	return color;
}

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
