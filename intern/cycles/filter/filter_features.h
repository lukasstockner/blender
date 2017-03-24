/*
 * Copyright 2011-2017 Blender Foundation
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

#define ccl_get_feature(pass) buffer[(pass)*pass_stride]

/* Loop over the pixels in the range [low.x, high.x) x [low.y, high.y).
 * pixel_buffer always points to the current pixel in the first pass. */
#define FOR_PIXEL_WINDOW     pixel_buffer = buffer + (low.y - rect.y)*buffer_w + (low.x - rect.x); \
                             for(pixel.y = low.y; pixel.y < high.y; pixel.y++) { \
                                 for(pixel.x = low.x; pixel.x < high.x; pixel.x++, pixel_buffer++) {

#define END_FOR_PIXEL_WINDOW     } \
                                 pixel_buffer += buffer_w - (high.x - low.x); \
                             }

ccl_device_inline void filter_get_feature_mean(int2 pixel, ccl_global float ccl_readonly_ptr buffer, float *features, int pass_stride)
{
	features[0] = pixel.x;
	features[1] = pixel.y;
	features[2] = ccl_get_feature(0);
	features[3] = ccl_get_feature(1);
	features[4] = ccl_get_feature(2);
	features[5] = ccl_get_feature(3);
	features[6] = ccl_get_feature(4);
	features[7] = ccl_get_feature(5);
	features[8] = ccl_get_feature(6);
	features[9] = ccl_get_feature(7);
}

ccl_device_inline void filter_get_features(int2 pixel, ccl_global float ccl_readonly_ptr buffer, ccl_local_param float *features, float ccl_readonly_ptr mean, int pass_stride)
{
	features[0] = pixel.x;
	features[1] = pixel.y;
	features[2] = ccl_get_feature(0);
	features[3] = ccl_get_feature(1);
	features[4] = ccl_get_feature(2);
	features[5] = ccl_get_feature(3);
	features[6] = ccl_get_feature(4);
	features[7] = ccl_get_feature(5);
	features[8] = ccl_get_feature(6);
	features[9] = ccl_get_feature(7);
	if(mean) {
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] -= mean[i];
	}
}

ccl_device_inline void filter_get_feature_scales(int2 pixel, ccl_global float ccl_readonly_ptr buffer, ccl_local_param float *scales, float ccl_readonly_ptr mean, int pass_stride)
{
	scales[0] = fabsf(pixel.x - mean[0]);
	scales[1] = fabsf(pixel.y - mean[1]);
	scales[2] = fabsf(ccl_get_feature(0) - mean[2]);
	scales[3] = len_squared(make_float3(ccl_get_feature(1) - mean[3],
	                                    ccl_get_feature(2) - mean[4],
	                                    ccl_get_feature(3) - mean[5]));
	scales[4] = fabsf(ccl_get_feature(4) - mean[6]);
	scales[5] = len_squared(make_float3(ccl_get_feature(5) - mean[7],
	                                    ccl_get_feature(6) - mean[8],
	                                    ccl_get_feature(7) - mean[9]));
}

ccl_device_inline void filter_calculate_scale(float *scale)
{
	scale[0] = 1.0f/max(scale[0], 0.01f);
	scale[1] = 1.0f/max(scale[1], 0.01f);
	scale[2] = 1.0f/max(scale[2], 0.01f);
	scale[6] = 1.0f/max(scale[4], 0.01f);
	scale[7] = scale[8] = scale[9] = 1.0f/max(sqrtf(scale[5]), 0.01f);
	scale[3] = scale[4] = scale[5] = 1.0f/max(sqrtf(scale[3]), 0.01f);
}

ccl_device_inline float3 filter_get_pixel_color(ccl_global float ccl_readonly_ptr buffer, int pass_stride)
{
	return make_float3(ccl_get_feature(0), ccl_get_feature(1), ccl_get_feature(2));
}

ccl_device_inline float filter_get_pixel_variance(ccl_global float ccl_readonly_ptr buffer, int pass_stride)
{
	return average(make_float3(ccl_get_feature(0), ccl_get_feature(1), ccl_get_feature(2)));
}

ccl_device_inline bool filter_firefly_rejection(float3 pixel_color, float pixel_variance, float3 center_color, float sqrt_center_variance)
{
	float color_diff = average(fabs(pixel_color - center_color));
	float variance = sqrt_center_variance + sqrtf(pixel_variance) + 0.005f;
	return (color_diff > 3.0f*variance);
}

/* Fill the design row without computing the weight. */
ccl_device_inline void filter_get_design_row_transform(int2 pixel,
                                                       ccl_global float ccl_readonly_ptr buffer,
                                                       float ccl_readonly_ptr feature_means,
                                                       int pass_stride,
                                                       ccl_local_param float *features,
                                                       int rank,
                                                       float *design_row,
                                                       ccl_global float ccl_readonly_ptr feature_transform,
                                                       int transform_stride)
{
	filter_get_features(pixel, buffer, features, feature_means, pass_stride);
	design_row[0] = 1.0f;
	for(int d = 0; d < rank; d++) {
#ifdef __KERNEL_GPU__
		float x = math_vector_dot_strided(features, feature_transform + d*DENOISE_FEATURES*transform_stride, transform_stride, DENOISE_FEATURES);
#else
		float x = math_vector_dot(features, feature_transform + d*DENOISE_FEATURES, DENOISE_FEATURES);
#endif
		design_row[1+d] = x;
	}
}

CCL_NAMESPACE_END
