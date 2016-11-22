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

ccl_device float nlm_weight(int px, int py, int qx, int qy, float ccl_readonly_ptr p_buffer, float ccl_readonly_ptr q_buffer, int pass_stride, float a, float k_2, int f, int4 rect)
{
	int w = align_up(rect.z - rect.x, 4);

	int2 low_dPatch = make_int2(max(max(rect.x - qx, rect.x - px),  -f), max(max(rect.y - qy, rect.y - py),  -f));
	int2 high_dPatch = make_int2(min(min(rect.z - qx, rect.z - px), f+1), min(min(rect.w - qy, rect.w - py), f+1));

	int dIdx = low_dPatch.x + low_dPatch.y*w;
#ifdef __KERNEL_SSE41__
	__m128 a_sse = _mm_set1_ps(a), k_2_sse = _mm_set1_ps(k_2);
	__m128 dI_sse = _mm_setzero_ps();
	__m128 highX_sse = _mm_set1_ps(high_dPatch.x);
	for(int dy = low_dPatch.y; dy < high_dPatch.y; dy++) {
		int dx;
		for(dx = low_dPatch.x; dx < high_dPatch.x; dx+=4, dIdx+=4) {
			__m128 active = _mm_cmplt_ps(_mm_add_ps(_mm_set1_ps(dx), _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)), highX_sse);
			__m128 p_color[3], q_color[3], p_var[3], q_var[3];
			filter_get_pixel_color_sse(p_buffer + dIdx, active, p_color, pass_stride);
			filter_get_pixel_color_sse(q_buffer + dIdx, active, q_color, pass_stride);
			filter_get_pixel_variance_3_sse(p_buffer + dIdx, active, p_var, pass_stride);
			filter_get_pixel_variance_3_sse(q_buffer + dIdx, active, q_var, pass_stride);

			__m128 diff = _mm_sub_ps(p_color[0], q_color[0]);
			dI_sse = _mm_add_ps(dI_sse, _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(diff, diff), _mm_mul_ps(a_sse, _mm_add_ps(p_var[0], _mm_min_ps(p_var[0], q_var[0])))), _mm_rcp_ps(_mm_add_ps(_mm_set1_ps(1e-7f), _mm_mul_ps(k_2_sse, _mm_add_ps(p_var[0], q_var[0]))))));
			diff = _mm_sub_ps(p_color[1], q_color[1]);
			dI_sse = _mm_add_ps(dI_sse, _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(diff, diff), _mm_mul_ps(a_sse, _mm_add_ps(p_var[1], _mm_min_ps(p_var[1], q_var[1])))), _mm_rcp_ps(_mm_add_ps(_mm_set1_ps(1e-7f), _mm_mul_ps(k_2_sse, _mm_add_ps(p_var[1], q_var[1]))))));
			diff = _mm_sub_ps(p_color[2], q_color[2]);
			dI_sse = _mm_add_ps(dI_sse, _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(diff, diff), _mm_mul_ps(a_sse, _mm_add_ps(p_var[2], _mm_min_ps(p_var[2], q_var[2])))), _mm_rcp_ps(_mm_add_ps(_mm_set1_ps(1e-7f), _mm_mul_ps(k_2_sse, _mm_add_ps(p_var[2], q_var[2]))))));
		}
		dIdx += w-(dx - low_dPatch.x);
	}
	float dI = _mm_hsum_ss(dI_sse);
#else
	float dI = 0.0f;
	for(int dy = low_dPatch.y; dy < high_dPatch.y; dy++) {
		for(int dx = low_dPatch.x; dx < high_dPatch.x; dx++, dIdx++) {
			float3 diff = filter_get_pixel_color(p_buffer + dIdx, pass_stride) - filter_get_pixel_color(q_buffer + dIdx, pass_stride);
			float3 pvar = filter_get_pixel_variance3(p_buffer + dIdx, pass_stride);
			float3 qvar = filter_get_pixel_variance3(q_buffer + dIdx, pass_stride);

			dI += reduce_add((diff*diff - a*(pvar + min(pvar, qvar))) / (make_float3(1e-7f, 1e-7f, 1e-7f) + k_2*(pvar + qvar)));
		}
		dIdx += w-(high_dPatch.x - low_dPatch.x);
	}
#endif
	dI *= 1.0f / (3.0f * (high_dPatch.x - low_dPatch.x) * (high_dPatch.y - low_dPatch.y));

	return fast_expf(-max(0.0f, dI));
}

ccl_device void kernel_filter_non_local_means_3(int x, int y, float ccl_readonly_ptr noisyImage[3], float ccl_readonly_ptr weightImage[3], float ccl_readonly_ptr variance[3], float *filteredImage[3], int4 rect, int r, int f, float a, float k_2)
{
	int2 low  = make_int2(max(rect.x, x - r),
	                      max(rect.y, y - r));
	int2 high = make_int2(min(rect.z, x + r + 1),
	                      min(rect.w, y + r + 1));

	float sum_image[3] = {0.0f}, sum_weight = 0.0f;

	int w = align_up(rect.z - rect.x, 4);
	int p_idx = (y-rect.y)*w + (x - rect.x);
	int q_idx = (low.y-rect.y)*w + (low.x-rect.x);
#ifdef __KERNEL_SSE41__
	__m128 a_sse = _mm_set1_ps(a), k_2_sse = _mm_set1_ps(k_2);
#endif
	/* Loop over the q's, center pixels of all relevant patches. */
	for(int qy = low.y; qy < high.y; qy++) {
		for(int qx = low.x; qx < high.x; qx++, q_idx++) {
			int2  low_dPatch = make_int2(max(max(rect.x - qx, rect.x - x),  -f), max(max(rect.y - qy, rect.y - y),  -f));
			int2 high_dPatch = make_int2(min(min(rect.z - qx, rect.z - x), f+1), min(min(rect.w - qy, rect.w - y), f+1));
			/* Loop over the pixels in the patch.
			 * Note that the patch must be small enough to be fully inside the rect, both at p and q.
			 * Do avoid doing all the coordinate calculations twice, the code here computes both weights at once. */
#ifdef __KERNEL_SSE41__
			__m128 dI_sse = _mm_setzero_ps();
			__m128 highX_sse = _mm_set1_ps(high_dPatch.x);
			for(int k = 0; k < 3; k++) {
				int dIdx = low_dPatch.x + low_dPatch.y*w;
				for(int dy = low_dPatch.y; dy < high_dPatch.y; dy++) {
					int dx;
					for(dx = low_dPatch.x; dx < high_dPatch.x; dx+=4, dIdx+=4) {
						__m128 diff = _mm_sub_ps(_mm_loadu_ps(weightImage[k] + p_idx + dIdx), _mm_loadu_ps(weightImage[k] + q_idx + dIdx));
						__m128 pvar = _mm_loadu_ps(variance[k] + p_idx + dIdx);
						__m128 qvar = _mm_loadu_ps(variance[k] + q_idx + dIdx);
						__m128 d = _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(diff, diff), _mm_mul_ps(a_sse, _mm_add_ps(pvar, _mm_min_ps(pvar, qvar)))), _mm_rcp_ps(_mm_add_ps(_mm_set1_ps(1e-7f), _mm_mul_ps(k_2_sse, _mm_add_ps(pvar, qvar)))));
						dI_sse = _mm_add_ps(dI_sse, _mm_mask_ps(d, _mm_cmplt_ps(_mm_add_ps(_mm_set1_ps(dx), _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)), highX_sse)));
					}
					dIdx += w-(dx - low_dPatch.x);
				}
			}
			float dI = _mm_hsum_ss(dI_sse);
#else
			float dI = 0.0f;
			for(int k = 0; k < 3; k++) {
				int dIdx = low_dPatch.x + low_dPatch.y*w;
				for(int dy = low_dPatch.y; dy < high_dPatch.y; dy++) {
					for(int dx = low_dPatch.x; dx < high_dPatch.x; dx++, dIdx++) {
						float diff = weightImage[k][p_idx+dIdx] - weightImage[k][q_idx+dIdx];
						dI += (diff*diff - a*(variance[k][p_idx+dIdx] + min(variance[k][p_idx+dIdx], variance[k][q_idx+dIdx]))) * (1.0f / (1e-7f + k_2*(variance[k][p_idx+dIdx] + variance[k][q_idx+dIdx])));
					}
					dIdx += w-(high_dPatch.x - low_dPatch.x);
				}
			}
#endif
			dI *= 1.0f / (3.0f * (high_dPatch.x - low_dPatch.x) * (high_dPatch.y - low_dPatch.y));

			float wI = fast_expf(-max(0.0f, dI));
			sum_image[0] += wI*noisyImage[0][q_idx];
			sum_image[1] += wI*noisyImage[1][q_idx];
			sum_image[2] += wI*noisyImage[2][q_idx];
			sum_weight += wI;
		}
		q_idx += w-(high.x-low.x);
	}

	filteredImage[0][p_idx] = sum_image[0] / sum_weight;
	filteredImage[1][p_idx] = sum_image[1] / sum_weight;
	filteredImage[2][p_idx] = sum_image[2] / sum_weight;
}

CCL_NAMESPACE_END