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

ccl_device_inline void kernel_filter_nlm_calc_difference(int dx, int dy, float ccl_readonly_ptr weightImage, float ccl_readonly_ptr varianceImage, float *differenceImage, int4 rect, int w, int channel_offset, float a, float k_2)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			float diff = 0.0f;
			int numChannels = channel_offset? 3 : 1;
			for(int c = 0; c < numChannels; c++) {
				float cdiff = weightImage[c*channel_offset + y*w+x] - weightImage[c*channel_offset + (y+dy)*w+(x+dx)];
				float pvar = varianceImage[c*channel_offset + y*w+x];
				float qvar = varianceImage[c*channel_offset + (y+dy)*w+(x+dx)];
				diff += (cdiff*cdiff - a*(pvar + min(pvar, qvar))) / (1e-8f + k_2*(pvar+qvar));
			}
			differenceImage[y*w+x] = diff * (1.0f / numChannels);
		}
	}
}

ccl_device_inline void kernel_filter_nlm_blur(float ccl_readonly_ptr differenceImage, float *outImage, int4 rect, int w, int f)
{
#ifdef __KERNEL_SSE3__
	int aligned_lowx = (rect.x & ~(3));
	int aligned_highx = ((rect.z + 3) & ~(3));
#endif
	for(int y = rect.y; y < rect.w; y++) {
		const int low = max(rect.y, y-f);
		const int high = min(rect.w, y+f+1);
		for(int x = rect.x; x < rect.z; x++) {
			outImage[y*w+x] = 0.0f;
		}
		for(int y1 = low; y1 < high; y1++) {
#ifdef __KERNEL_SSE3__
			for(int x = aligned_lowx; x < aligned_highx; x+=4) {
				_mm_store_ps(outImage + y*w+x, _mm_add_ps(_mm_load_ps(outImage + y*w+x), _mm_load_ps(differenceImage + y*w+x)));
			}
#else
			for(int x = rect.x; x < rect.z; x++) {
				outImage[y*w+x] += differenceImage[y1*w+x];
			}
#endif
		}
		for(int x = rect.x; x < rect.z; x++) {
			outImage[y*w+x] *= 1.0f/(high - low);
		}
	}
}

ccl_device_inline void kernel_filter_nlm_calc_weight(float ccl_readonly_ptr differenceImage, float *outImage, int4 rect, int w, int f)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			outImage[y*w+x] = 0.0f;
		}
	}
	for(int dx = -f; dx <= f; dx++) {
		int pos_dx = max(0, dx);
		int neg_dx = min(0, dx);
		for(int y = rect.y; y < rect.w; y++) {
			for(int x = rect.x-neg_dx; x < rect.z-pos_dx; x++) {
				outImage[y*w+x] += differenceImage[y*w+dx+x];
			}
		}
	}
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			outImage[y*w+x] = expf(-max(outImage[y*w+x] * (1.0f/(high - low)), 0.0f));
		}
	}
}

ccl_device_inline void kernel_filter_nlm_update_output(int dx, int dy, float ccl_readonly_ptr differenceImage, float ccl_readonly_ptr image, float *outImage, float *accumImage, int4 rect, int w, int f)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			float sum = 0.0f;
			for(int x1 = low; x1 < high; x1++) {
				sum += differenceImage[y*w+x1];
			}
			float weight = sum * (1.0f/(high - low));
			if(outImage) {
				accumImage[y*w+x] += weight;
				outImage[y*w+x] += weight*image[(y+dy)*w+(x+dx)];
			}
			else {
				accumImage[y*w+x] = weight;
			}
		}
	}
}

ccl_device_inline void kernel_filter_nlm_normalize(float *outImage, float ccl_readonly_ptr accumImage, int4 rect, int w)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			outImage[y*w+x] /= accumImage[y*w+x];
		}
	}
}

CCL_NAMESPACE_END