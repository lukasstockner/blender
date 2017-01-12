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
	dI *= 1.0f / (3.0f * (high_dPatch.x - low_dPatch.x) * (high_dPatch.y - low_dPatch.y));

	return fast_expf(-max(0.0f, dI));
}

ccl_device_inline void kernel_filter_nlm_calc_difference(int x, int y, int dx, int dy, float ccl_readonly_ptr weightImage, float ccl_readonly_ptr varianceImage, float *differenceImage, int4 rect, int w, int channel_offset, float a, float k_2)
{
	float diff = 0.0f;
	int numChannels = channel_offset? 3 : 1;
	for(int c = 0; c < numChannels; c++) {
		float cdiff = weightImage[c*channel_offset + y*w+x] - weightImage[c*channel_offset + (y+dy)*w+(x+dx)];
		float pvar = varianceImage[c*channel_offset + y*w+x];
		float qvar = varianceImage[c*channel_offset + (y+dy)*w+(x+dx)];
		diff += (cdiff*cdiff - a*(pvar + min(pvar, qvar))) / (1e-8f + k_2*(pvar+qvar));
	}
	if(numChannels > 1) {
		diff *= 1.0f/numChannels;
	}
	differenceImage[y*w+x] = diff;
}

ccl_device_inline void kernel_filter_nlm_blur(int x, int y, float ccl_readonly_ptr differenceImage, float *outImage, int4 rect, int w, int f)
{
	float sum = 0.0f;
	const int low = max(rect.y, y-f);
	const int high = min(rect.w, y+f+1);
	for(int y1 = low; y1 < high; y1++) {
		sum += differenceImage[y1*w+x];
	}
	sum *= 1.0f/(high-low);
	outImage[y*w+x] = sum;
}

ccl_device_inline void kernel_filter_nlm_calc_weight(int x, int y, float ccl_readonly_ptr differenceImage, float *outImage, int4 rect, int w, int f)
{
	float sum = 0.0f;
	const int low = max(rect.x, x-f);
	const int high = min(rect.z, x+f+1);
	for(int x1 = low; x1 < high; x1++) {
		sum += differenceImage[y*w+x1];
	}
	sum *= 1.0f/(high-low);
	outImage[y*w+x] = expf(-max(sum, 0.0f));
}

ccl_device_inline void kernel_filter_nlm_update_output(int x, int y, int dx, int dy, float ccl_readonly_ptr differenceImage, float ccl_readonly_ptr image, float *outImage, float *accumImage, int4 rect, int w, int f)
{
	float sum = 0.0f;
	const int low = max(rect.x, x-f);
	const int high = min(rect.z, x+f+1);
	for(int x1 = low; x1 < high; x1++) {
		sum += differenceImage[y*w+x1];
	}
	sum *= 1.0f/(high-low);
	if(outImage) {
		accumImage[y*w+x] += sum;
		outImage[y*w+x] += sum*image[(y+dy)*w+(x+dx)];
	}
	else {
		accumImage[y*w+x] = sum;
	}
}

ccl_device_inline void kernel_filter_nlm_normalize(int x, int y, float *outImage, float ccl_readonly_ptr accumImage, int4 rect, int w)
{
	outImage[y*w+x] /= accumImage[y*w+x];
}

CCL_NAMESPACE_END