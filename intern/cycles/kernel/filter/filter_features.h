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

#define ccl_get_feature(buffer, pass) (buffer)[(pass)*pass_stride]

/* Loop over the pixels in the range [low.x, high.x) x [low.y, high.y).
 * pixel_buffer always points to the current pixel in the first pass.
 * Repeat the loop for every secondary frame if there are any. */
#define FOR_PIXEL_WINDOW     for(int frame = 0; frame < tiles->num_frames; frame++) { \
                                 pixel.z = tiles->frames[frame]; \
                                 pixel_buffer = buffer + (low.y - rect.y)*buffer_w + (low.x - rect.x) + frame*frame_stride; \
                                 for(pixel.y = low.y; pixel.y < high.y; pixel.y++) { \
                                     for(pixel.x = low.x; pixel.x < high.x; pixel.x++, pixel_buffer++) {

#define END_FOR_PIXEL_WINDOW         } \
                                     pixel_buffer += buffer_w - (high.x - low.x); \
                                 } \
                             }

ccl_device_inline void filter_get_features(int3 pixel,
                                           const ccl_global float *ccl_restrict buffer,
                                           float *features, int feature_mode,
                                           const float *ccl_restrict mean,
                                           int pass_stride)
{
	if(feature_mode == FEATURE_MODE_RENDER) {
		features[0] = pixel.x;
		features[1] = pixel.y;
		features[2] = fabsf(ccl_get_feature(buffer, 0));
		features[3] = ccl_get_feature(buffer, 1);
		features[4] = ccl_get_feature(buffer, 2);
		features[5] = ccl_get_feature(buffer, 3);
		features[6] = ccl_get_feature(buffer, 4);
		features[7] = ccl_get_feature(buffer, 5);
		features[8] = ccl_get_feature(buffer, 6);
		features[9] = ccl_get_feature(buffer, 7);
		if(mean) {
			for(int i = 0; i < 10; i++)
				features[i] -= mean[i];
		}
	}
	else {
		kernel_assert(feature_mode == FEATURE_MODE_MULTIFRAME);
		features[0] = pixel.x;
		features[1] = pixel.y;
		features[2] = pixel.z;
		features[3] = fabsf(ccl_get_feature(buffer, 0));
		features[4] = ccl_get_feature(buffer, 1);
		features[5] = ccl_get_feature(buffer, 2);
		features[6] = ccl_get_feature(buffer, 3);
		features[7] = ccl_get_feature(buffer, 4);
		features[8] = ccl_get_feature(buffer, 5);
		features[9] = ccl_get_feature(buffer, 6);
		features[10] = ccl_get_feature(buffer, 7);
		if(mean) {
			for(int i = 0; i < 11; i++)
				features[i] -= mean[i];
		}
	}
}

ccl_device_inline void filter_get_feature_scales(int3 pixel,
                                                 const ccl_global float *ccl_restrict buffer,
                                                 float *scales, int feature_mode,
                                                 const float *ccl_restrict mean,
                                                 int pass_stride)
{
	if(feature_mode == FEATURE_MODE_RENDER) {
		scales[0] = fabsf(pixel.x - mean[0]);
		scales[1] = fabsf(pixel.y - mean[1]);
		scales[2] = fabsf(fabsf(ccl_get_feature(buffer, 0)) - mean[2]);
		scales[3] = len_squared(make_float3(ccl_get_feature(buffer, 1) - mean[3],
											ccl_get_feature(buffer, 2) - mean[4],
											ccl_get_feature(buffer, 3) - mean[5]));
		scales[4] = fabsf(ccl_get_feature(buffer, 4) - mean[6]);
		scales[5] = len_squared(make_float3(ccl_get_feature(buffer, 5) - mean[7],
											ccl_get_feature(buffer, 6) - mean[8],
											ccl_get_feature(buffer, 7) - mean[9]));
	}
	else {
		kernel_assert(feature_mode == FEATURE_MODE_MULTIFRAME);
		scales[0] = fabsf(pixel.x - mean[0]);
		scales[1] = fabsf(pixel.y - mean[1]);
		scales[2] = fabsf(pixel.z - mean[2]);
		scales[3] = fabsf(fabsf(ccl_get_feature(buffer, 0)) - mean[3]);
		scales[4] = len_squared(make_float3(ccl_get_feature(buffer, 1) - mean[4],
											ccl_get_feature(buffer, 2) - mean[5],
											ccl_get_feature(buffer, 3) - mean[6]));
		scales[5] = fabsf(ccl_get_feature(buffer, 4) - mean[7]);
		scales[6] = len_squared(make_float3(ccl_get_feature(buffer, 5) - mean[8],
											ccl_get_feature(buffer, 6) - mean[9],
											ccl_get_feature(buffer, 7) - mean[10]));
	}
}

ccl_device_inline void filter_calculate_scale(float *scale, int feature_mode)
{
	if(feature_mode == FEATURE_MODE_RENDER) {
		scale[0] = 1.0f/max(scale[0], 0.01f);
		scale[1] = 1.0f/max(scale[1], 0.01f);
		scale[2] = 1.0f/max(scale[2], 0.01f);
		scale[6] = 1.0f/max(scale[4], 0.01f);
		scale[7] = scale[8] = scale[9] = 1.0f/max(sqrtf(scale[5]), 0.01f);
		scale[3] = scale[4] = scale[5] = 1.0f/max(sqrtf(scale[3]), 0.01f);
	}
	else {
		kernel_assert(feature_mode == FEATURE_MODE_MULTIFRAME);
		scale[0] = 1.0f/max(scale[0], 0.01f);
		scale[1] = 1.0f/max(scale[1], 0.01f);
		scale[2] = 1.0f/max(scale[2], 0.01f);
		scale[3] = 1.0f/max(scale[3], 0.01f);
		scale[7] = 1.0f/max(scale[5], 0.01f);
		scale[8] = scale[9] = scale[10] = 1.0f/max(sqrtf(scale[6]), 0.01f);
		scale[4] = scale[5] = scale[6] = 1.0f/max(sqrtf(scale[4]), 0.01f);
	}
}

ccl_device_inline float3 filter_get_color(const ccl_global float *ccl_restrict buffer,
                                          int pass_stride, int feature_mode)
{
	return make_float3(ccl_get_feature(buffer, 8), ccl_get_feature(buffer, 9), ccl_get_feature(buffer, 10));
}

ccl_device_inline void design_row_add(float *design_row,
                                      int rank,
                                      const ccl_global float *ccl_restrict transform,
                                      int stride,
                                      int row,
                                      float feature,
                                      int transform_row_stride)
{
	for(int i = 0; i < rank; i++) {
		design_row[1+i] += transform[(row*transform_row_stride + i)*stride]*feature;
	}
}

/* Fill the design row. */
ccl_device_inline void filter_get_design_row_transform(int3 p_pixel,
                                                       const ccl_global float *ccl_restrict p_buffer,
                                                       int3 q_pixel,
                                                       const ccl_global float *ccl_restrict q_buffer,
                                                       int pass_stride,
                                                       int rank,
                                                       float *design_row,
                                                       const ccl_global float *ccl_restrict transform,
                                                       int stride,
                                                       int feature_mode)
{
	const int feature_num = 10;
	design_row[0] = 1.0f;
	math_vector_zero(design_row+1, rank);
#define DESIGN_ROW_ADD(I, F) design_row_add(design_row, rank, transform, stride, I, F, feature_num);
	if(feature_mode == FEATURE_MODE_RENDER) {
		DESIGN_ROW_ADD(0,       q_pixel.x - p_pixel.x);
		DESIGN_ROW_ADD(1,       q_pixel.y - p_pixel.y);
		DESIGN_ROW_ADD(2, fabsf(ccl_get_feature(q_buffer, 0)) - fabsf(ccl_get_feature(p_buffer, 0)));
		DESIGN_ROW_ADD(3,       ccl_get_feature(q_buffer, 1)  -       ccl_get_feature(p_buffer, 1));
		DESIGN_ROW_ADD(4,       ccl_get_feature(q_buffer, 2)  -       ccl_get_feature(p_buffer, 2));
		DESIGN_ROW_ADD(5,       ccl_get_feature(q_buffer, 3)  -       ccl_get_feature(p_buffer, 3));
		DESIGN_ROW_ADD(6,       ccl_get_feature(q_buffer, 4)  -       ccl_get_feature(p_buffer, 4));
		DESIGN_ROW_ADD(7,       ccl_get_feature(q_buffer, 5)  -       ccl_get_feature(p_buffer, 5));
		DESIGN_ROW_ADD(8,       ccl_get_feature(q_buffer, 6)  -       ccl_get_feature(p_buffer, 6));
		DESIGN_ROW_ADD(9,       ccl_get_feature(q_buffer, 7)  -       ccl_get_feature(p_buffer, 7));
	}
	else {
		kernel_assert(feature_mode == FEATURE_MODE_MULTIFRAME);
		DESIGN_ROW_ADD(0,       q_pixel.x - p_pixel.x);
		DESIGN_ROW_ADD(1,       q_pixel.y - p_pixel.y);
		DESIGN_ROW_ADD(2,       q_pixel.z - p_pixel.z);
		DESIGN_ROW_ADD(3, fabsf(ccl_get_feature(q_buffer, 0)) - fabsf(ccl_get_feature(p_buffer, 0)));
		DESIGN_ROW_ADD(4,       ccl_get_feature(q_buffer, 1)  -       ccl_get_feature(p_buffer, 1));
		DESIGN_ROW_ADD(5,       ccl_get_feature(q_buffer, 2)  -       ccl_get_feature(p_buffer, 2));
		DESIGN_ROW_ADD(6,       ccl_get_feature(q_buffer, 3)  -       ccl_get_feature(p_buffer, 3));
		DESIGN_ROW_ADD(7,       ccl_get_feature(q_buffer, 4)  -       ccl_get_feature(p_buffer, 4));
		DESIGN_ROW_ADD(8,       ccl_get_feature(q_buffer, 5)  -       ccl_get_feature(p_buffer, 5));
		DESIGN_ROW_ADD(9,       ccl_get_feature(q_buffer, 6)  -       ccl_get_feature(p_buffer, 6));
		DESIGN_ROW_ADD(10,      ccl_get_feature(q_buffer, 7)  -       ccl_get_feature(p_buffer, 7));
	}
}

CCL_NAMESPACE_END
