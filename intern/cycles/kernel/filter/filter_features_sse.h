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

CCL_NAMESPACE_BEGIN

#define ccl_get_feature_sse(pass) _mm_loadu_ps(buffer + (pass)*pass_stride)

/* Loop over the pixels in the range [low.x, high.x) x [low.y, high.y), 4 at a time.
 * pixel_buffer always points to the first of the 4 current pixel in the first pass.
 * x4 and y4 contain the coordinates of the four pixels, active_pixels contains a mask that's set for all pixels within the window. */

#ifdef DENOISE_TEMPORAL
#define FOR_PIXEL_WINDOW_SSE pixel_buffer = buffer + (low.y - rect.y)*buffer_w + (low.x - rect.x); \
                             for(int t = 0; t < num_frames; t++) { \
                                 __m128 t4 = _mm_set1_ps((t == 0)? 0: ((t <= prev_frames)? (t-prev_frames-1): (t - prev_frames))); \
                                 for(pixel.y = low.y; pixel.y < high.y; pixel.y++) { \
                                     __m128 y4 = _mm_set1_ps(pixel.y); \
                                     for(pixel.x = low.x; pixel.x < high.x; pixel.x += 4, pixel_buffer += 4) { \
                                         __m128 x4 = _mm_add_ps(_mm_set1_ps(pixel.x), _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)); \
                                         __m128 active_pixels = _mm_cmplt_ps(x4, _mm_set1_ps(high.x));

#define END_FOR_PIXEL_WINDOW_SSE     } \
                                     pixel_buffer += buffer_w - (pixel.x - low.x); \
                                 } \
                                 pixel_buffer += buffer_w * (buffer_h - (high.y - low.y)); \
                             }
#else
#define FOR_PIXEL_WINDOW_SSE     pixel_buffer = buffer + (low.y - rect.y)*buffer_w + (low.x - rect.x); \
                                 for(pixel.y = low.y; pixel.y < high.y; pixel.y++) { \
                                     __m128 y4 = _mm_set1_ps(pixel.y); \
                                     for(pixel.x = low.x; pixel.x < high.x; pixel.x += 4, pixel_buffer += 4) { \
                                         __m128 x4 = _mm_add_ps(_mm_set1_ps(pixel.x), _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)); \
                                         __m128 active_pixels = _mm_cmplt_ps(x4, _mm_set1_ps(high.x));

#define END_FOR_PIXEL_WINDOW_SSE     } \
                                     pixel_buffer += buffer_w - (pixel.x - low.x); \
                                 }
#endif

ccl_device_inline void filter_get_features_sse(__m128 x, __m128 y, __m128 t, __m128 active_pixels, float ccl_readonly_ptr buffer, __m128 *features, __m128 *mean, int pass_stride)
{
	__m128 *feature = features;
	*(feature++) = x;
	*(feature++) = y;
#ifdef DENOISE_TEMPORAL
	*(feature++) = t;
#endif
	*(feature++) = ccl_get_feature_sse(6);
	*(feature++) = ccl_get_feature_sse(0);
	*(feature++) = ccl_get_feature_sse(2);
	*(feature++) = ccl_get_feature_sse(4);
	*(feature++) = ccl_get_feature_sse(8);
	*(feature++) = ccl_get_feature_sse(10);
	*(feature++) = ccl_get_feature_sse(12);
	*(feature++) = ccl_get_feature_sse(14);
	if(mean) {
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] = _mm_mask_ps(_mm_sub_ps(features[i], mean[i]), active_pixels);
	}
	else {
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] = _mm_mask_ps(features[i], active_pixels);
	}
#ifdef DENOISE_SECOND_ORDER_SCREEN
	features[10] = _mm_mul_ps(features[0], features[0]);
	features[11] = _mm_mul_ps(features[1], features[1]);
	features[12] = _mm_mul_ps(features[0], features[1]);
#endif
}

ccl_device_inline void filter_get_feature_scales_sse(__m128 x, __m128 y, __m128 t, __m128 active_pixels, float ccl_readonly_ptr buffer, __m128 *scales, __m128 *mean, int pass_stride)
{
	*(scales++) = _mm_mask_ps(_mm_fabs_ps(_mm_sub_ps(x, *(mean++))), active_pixels); //X
	*(scales++) = _mm_mask_ps(_mm_fabs_ps(_mm_sub_ps(y, *(mean++))), active_pixels); //Y
#ifdef DENOISE_TEMPORAL
	*(scales++) = _mm_mask_ps(_mm_fabs_ps(_mm_sub_ps(t, *(mean++))), active_pixels); //T
#endif

	*(scales++) = _mm_mask_ps(_mm_fabs_ps(_mm_sub_ps(ccl_get_feature_sse(6), *(mean++))), active_pixels); //Depth

	__m128 diff = _mm_sub_ps(ccl_get_feature_sse(0), mean[0]);
	__m128 scale3 = _mm_mul_ps(diff, diff);
	diff = _mm_sub_ps(ccl_get_feature_sse(2), mean[1]);
	scale3 = _mm_add_ps(scale3, _mm_mul_ps(diff, diff));
	diff = _mm_sub_ps(ccl_get_feature_sse(4), mean[2]);
	scale3 = _mm_add_ps(scale3, _mm_mul_ps(diff, diff));
	mean += 3;
	*(scales++) = _mm_mask_ps(scale3, active_pixels); //NormalX
	*(scales++) = _mm_mask_ps(scale3, active_pixels); //NormalY
	*(scales++) = _mm_mask_ps(scale3, active_pixels); //NormalZ

	*(scales++) = _mm_mask_ps(_mm_fabs_ps(_mm_sub_ps(ccl_get_feature_sse(8), *(mean++))), active_pixels); //Shadow

	diff = _mm_sub_ps(ccl_get_feature_sse(10), mean[0]);
	scale3 = _mm_mul_ps(diff, diff);
	diff = _mm_sub_ps(ccl_get_feature_sse(12), mean[1]);
	scale3 = _mm_add_ps(scale3, _mm_mul_ps(diff, diff));
	diff = _mm_sub_ps(ccl_get_feature_sse(14), mean[2]);
	scale3 = _mm_add_ps(scale3, _mm_mul_ps(diff, diff));
	mean += 3;
	*(scales++) = _mm_mask_ps(scale3, active_pixels); //AlbedoR
	*(scales++) = _mm_mask_ps(scale3, active_pixels); //AlbedoG
	*(scales++) = _mm_mask_ps(scale3, active_pixels); //AlbedoB
}

ccl_device_inline void filter_calculate_scale_sse(__m128 *scale)
{
	scale[0] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(scale[0]), _mm_set1_ps(0.01f))); //X
	scale[1] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(scale[1]), _mm_set1_ps(0.01f))); //Y
	scale += 2;
#ifdef DENOISE_TEMPORAL
	scale[0] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(scale[0]), _mm_set1_ps(0.01f))); //T
	scale++;
#endif
	
	scale[0] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(scale[0]), _mm_set1_ps(0.01f))); //Depth

	scale[1] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(_mm_sqrt_ps(scale[1])), _mm_set1_ps(0.01f))); //NormalX
	scale[2] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(_mm_sqrt_ps(scale[2])), _mm_set1_ps(0.01f))); //NormalY
	scale[3] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(_mm_sqrt_ps(scale[3])), _mm_set1_ps(0.01f))); //NormalZ
	
	scale[4] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(scale[4]), _mm_set1_ps(0.01f))); //Shadow

	scale[5] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(_mm_sqrt_ps(scale[5])), _mm_set1_ps(0.01f))); //AlbedoR
	scale[6] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(_mm_sqrt_ps(scale[6])), _mm_set1_ps(0.01f))); //AlbedoG
	scale[7] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(_mm_sqrt_ps(scale[7])), _mm_set1_ps(0.01f))); //AlbedoB
}

ccl_device_inline void filter_get_feature_variance_sse(__m128 x, __m128 y, __m128 active_pixels, float ccl_readonly_ptr buffer, __m128 *features, __m128 *scale, int pass_stride)
{
	__m128 *feature = features;
	*(feature++) = _mm_setzero_ps();
	*(feature++) = _mm_setzero_ps();
#ifdef DENOISE_TEMPORAL
	*(feature++) = _mm_setzero_ps();
#endif
	*(feature++) = _mm_mask_ps(ccl_get_feature_sse( 7), active_pixels);
	*(feature++) = _mm_mask_ps(ccl_get_feature_sse( 1), active_pixels);
	*(feature++) = _mm_mask_ps(ccl_get_feature_sse( 3), active_pixels);
	*(feature++) = _mm_mask_ps(ccl_get_feature_sse( 5), active_pixels);
	*(feature++) = _mm_setzero_ps();//_mm_mask_ps(ccl_get_feature_sse( 9), active_pixels);
	*(feature++) = _mm_mask_ps(ccl_get_feature_sse(11), active_pixels);
	*(feature++) = _mm_mask_ps(ccl_get_feature_sse(13), active_pixels);
	*(feature++) = _mm_mask_ps(ccl_get_feature_sse(15), active_pixels);
#ifdef DENOISE_SECOND_ORDER_SCREEN
	features[10] = _mm_setzero_ps();
	features[11] = _mm_setzero_ps();
	features[12] = _mm_setzero_ps();
#endif
	for(int i = 0; i < DENOISE_FEATURES; i++)
		features[i] = _mm_mul_ps(features[i], _mm_mul_ps(scale[i], scale[i]));
}

ccl_device_inline void filter_get_pixel_color_sse(float ccl_readonly_ptr buffer, __m128 active_pixels, __m128 *color, int pass_stride)
{
	color[0] = _mm_mask_ps(ccl_get_feature_sse(16), active_pixels);
	color[1] = _mm_mask_ps(ccl_get_feature_sse(18), active_pixels);
	color[2] = _mm_mask_ps(ccl_get_feature_sse(20), active_pixels);
}

ccl_device_inline void filter_get_pixel_variance_3_sse(float ccl_readonly_ptr buffer, __m128 active_pixels, __m128 *var, int pass_stride)
{
	var[0] = _mm_mask_ps(ccl_get_feature_sse(17), active_pixels);
	var[1] = _mm_mask_ps(ccl_get_feature_sse(19), active_pixels);
	var[2] = _mm_mask_ps(ccl_get_feature_sse(21), active_pixels);
}

ccl_device_inline __m128 filter_get_pixel_variance_sse(float ccl_readonly_ptr buffer, __m128 active_pixels, int pass_stride)
{
	return _mm_mask_ps(_mm_mul_ps(_mm_set1_ps(1.0f/3.0f), _mm_add_ps(_mm_add_ps(ccl_get_feature_sse(17), ccl_get_feature_sse(19)), ccl_get_feature_sse(21))), active_pixels);
}

ccl_device_inline __m128 filter_fill_design_row_sse(__m128 *features, __m128 active_pixels, int rank, __m128 *design_row, __m128 *feature_transform, __m128 *bandwidth_factor)
{
	__m128 weight = _mm_mask_ps(_mm_set1_ps(1.0f), active_pixels);
	design_row[0] = weight;
	for(int d = 0; d < rank; d++) {
		__m128 x = math_dot_sse(features, feature_transform + d*DENOISE_FEATURES, DENOISE_FEATURES);
		__m128 x2 = _mm_mul_ps(x, bandwidth_factor[d]);
		x2 = _mm_mul_ps(x2, x2);
		weight = _mm_mask_ps(_mm_mul_ps(weight, _mm_mul_ps(_mm_set1_ps(0.75f), _mm_sub_ps(_mm_set1_ps(1.0f), x2))), _mm_and_ps(_mm_cmplt_ps(x2, _mm_set1_ps(1.0f)), active_pixels));
		design_row[1+d] = x;
	}
	return weight;
}

ccl_device_inline __m128 filter_fill_design_row_quadratic_sse(__m128 *features, __m128 active_pixels, int rank, __m128 *design_row, __m128 *feature_transform)
{
	__m128 weight = _mm_mask_ps(_mm_set1_ps(1.0f), active_pixels);
	design_row[0] = weight;
	for(int d = 0; d < rank; d++) {
		__m128 x = math_dot_sse(features, feature_transform + d*DENOISE_FEATURES, DENOISE_FEATURES);
		__m128 x2 = _mm_mul_ps(x, x);
		weight = _mm_mask_ps(_mm_mul_ps(weight, _mm_mul_ps(_mm_set1_ps(0.75f), _mm_sub_ps(_mm_set1_ps(1.0f), x2))), _mm_and_ps(_mm_cmplt_ps(x2, _mm_set1_ps(1.0f)), active_pixels));
		design_row[1+d] = x;
		design_row[1+rank+d] = x2;
	}
	return weight;
}

ccl_device_inline __m128 filter_firefly_rejection_sse(__m128 *pixel_color, __m128 pixel_variance, __m128 *center_color, __m128 sqrt_center_variance)
{
	__m128 color_diff = _mm_mul_ps(_mm_set1_ps(1.0f/9.0f), _mm_add_ps(_mm_add_ps(_mm_fabs_ps(_mm_sub_ps(pixel_color[0], center_color[0])), _mm_fabs_ps(_mm_sub_ps(pixel_color[1], center_color[1]))), _mm_fabs_ps(_mm_sub_ps(pixel_color[2], center_color[2]))));
	__m128 variance = _mm_add_ps(_mm_add_ps(sqrt_center_variance, _mm_sqrt_ps(pixel_variance)), _mm_set1_ps(0.005f));
	return _mm_cmple_ps(color_diff, variance);;
}

CCL_NAMESPACE_END