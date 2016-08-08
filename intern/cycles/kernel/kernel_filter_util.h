/*
 * Copyright 2011-2016 Blender Foundation
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

#include "util_math_matrix.h"

CCL_NAMESPACE_BEGIN

#define ccl_get_feature(pass) buffer[(pass)*pass_stride]

/* Loop over the pixels in the range [low.x, high.x) x [low.y, high.y).
 * pixel_buffer always points to the current pixel in the first pass. */
#define FOR_PIXEL_WINDOW     pixel_buffer = buffer + (low.y - rect.y)*buffer_w + (low.x - rect.x); \
                             for(int py = low.y; py < high.y; py++) { \
                                 for(int px = low.x; px < high.x; px++, pixel_buffer++) {

#define END_FOR_PIXEL_WINDOW     } \
                                 pixel_buffer += buffer_w - (high.x - low.x); \
                             }

ccl_device_inline void filter_get_features(int x, int y, float *buffer, float *features, float *mean, int pass_stride)
{
	features[0] = x;
	features[1] = y;
	features[2] = ccl_get_feature(0);
	features[3] = ccl_get_feature(2);
	features[4] = ccl_get_feature(4);
	features[5] = ccl_get_feature(6);
	features[6] = ccl_get_feature(8);
	features[7] = ccl_get_feature(10);
	features[8] = ccl_get_feature(12);
	features[9] = ccl_get_feature(14);
	if(mean) {
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] -= mean[i];
	}
#ifdef DENOISE_SECOND_ORDER_SCREEN
	features[10] = features[0]*features[0];
	features[11] = features[1]*features[1];
	features[12] = features[0]*features[1];
#endif
}

ccl_device_inline void filter_get_feature_variance(int x, int y, float *buffer, float *features, float *scale, int pass_stride)
{
	features[0] = 0.0f;
	features[1] = 0.0f;
	features[2] = ccl_get_feature(1);
	features[3] = ccl_get_feature(3);
	features[4] = ccl_get_feature(5);
	features[5] = ccl_get_feature(7);
	features[6] = ccl_get_feature(9);
	features[7] = ccl_get_feature(11);
	features[8] = ccl_get_feature(13);
	features[9] = ccl_get_feature(15);
#ifdef DENOISE_SECOND_ORDER_SCREEN
	features[10] = 0.0f;
	features[11] = 0.0f;
	features[12] = 0.0f;
#endif
	for(int i = 0; i < DENOISE_FEATURES; i++)
		features[i] *= scale[i]*scale[i];
}

ccl_device_inline float3 filter_get_pixel_color(float *buffer, int pass_stride)
{
	return make_float3(ccl_get_feature(16), ccl_get_feature(18), ccl_get_feature(20));
}

ccl_device_inline float filter_get_pixel_variance(float *buffer, int pass_stride)
{
	return average(make_float3(ccl_get_feature(17), ccl_get_feature(19), ccl_get_feature(21)));
}

ccl_device_inline float filter_fill_design_row(float *features, int rank, float *design_row, float *feature_transform, float *bandwidth_factor)
{
	design_row[0] = 1.0f;
	float weight = 1.0f;
	for(int d = 0; d < rank; d++) {
		float x = math_dot(features, feature_transform + d*DENOISE_FEATURES, DENOISE_FEATURES);
		float x2 = x*x;
		if(bandwidth_factor) x2 *= bandwidth_factor[d]*bandwidth_factor[d];
		if(x2 < 1.0f) {
			/* Pixels are weighted by Epanechnikov kernels. */
			weight *= 0.75f * (1.0f - x2);
		}
		else {
			weight = 0.0f;
			break;
		}
		design_row[1+d] = x;
		if(!bandwidth_factor) design_row[1+rank+d] = x2;
	}
	return weight;
}

ccl_device_inline bool filter_firefly_rejection(float3 pixel_color, float pixel_variance, float3 center_color, float sqrt_center_variance)
{
	float color_diff = average(fabs(pixel_color - center_color));
	float variance = sqrt_center_variance + sqrtf(pixel_variance) + 0.005f;
	return (color_diff > 3.0f*variance);
}

CCL_NAMESPACE_END
