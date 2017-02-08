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

/* First step of the shadow prefiltering, performs the shadow division and stores all data
 * in a nice and easy rectangular array that can be passed to the NLM filter.
 *
 * Calculates:
 * unfiltered: Contains the two half images of the shadow feature pass
 * sampleVariance: The sample-based variance calculated in the kernel. Note: This calculation is biased in general, and especially here since the variance of the ratio can only be approximated.
 * sampleVarianceV: Variance of the sample variance estimation, quite noisy (since it's essentially the buffer variance of the two variance halves)
 * bufferVariance: The buffer-based variance of the shadow feature. Unbiased, but quite noisy.
 */
ccl_device void kernel_filter_divide_shadow(int sample,
                                            float **buffers,
                                            int x, int y,
                                            int *tile_x, int *tile_y,
                                            int *offset, int *stride,
                                            float *unfilteredA,
                                            float *unfilteredB,
                                            float *sampleVariance,
                                            float *sampleVarianceV,
                                            float *bufferVariance,
                                            int4 rect,
                                            int buffer_pass_stride,
                                            int buffer_denoising_offset,
                                            bool use_gradients)
{
	int xtile = (x < tile_x[1])? 0: ((x < tile_x[2])? 1: 2);
	int ytile = (y < tile_y[1])? 0: ((y < tile_y[2])? 1: 2);
	int tile = ytile*3+xtile;
	float *center_buffer = buffers[tile] + (offset[tile] + y*stride[tile] + x)*buffer_pass_stride;

	if(use_gradients && tile == 4) {
		center_buffer[0] = center_buffer[1] = center_buffer[2] = center_buffer[3] = 0.0f;
	}
	center_buffer += buffer_denoising_offset;

	int buffer_w = align_up(rect.z - rect.x, 4);
	int idx = (y-rect.y)*buffer_w + (x - rect.x);
	unfilteredA[idx] = center_buffer[15] / max(center_buffer[14], 1e-7f);
	unfilteredB[idx] = center_buffer[18] / max(center_buffer[17], 1e-7f);
	float varFac = 1.0f / (sample * (sample-1));
	sampleVariance[idx] = (center_buffer[16] + center_buffer[19]) * varFac;
	sampleVarianceV[idx] = 0.5f * (center_buffer[16] - center_buffer[19]) * (center_buffer[16] - center_buffer[19]) * varFac * varFac;
	bufferVariance[idx] = 0.5f * (unfilteredA[idx] - unfilteredB[idx]) * (unfilteredA[idx] - unfilteredB[idx]);
}

/* Load a regular feature from the render buffers into the denoise buffer.
 * Parameters:
 * - sample: The sample amount in the buffer, used to normalize the buffer.
 * - buffers: 9-Element Array containing pointers to the buffers of the 3x3 tiles around the current one.
 * - m_offset, v_offset: Render Buffer Pass offsets of mean and variance of the feature.
 * - x, y: Current pixel
 * - tile_x, tile_y: 4-Element Arrays containing the x/y coordinates of the start of the lower, current and upper tile as well as the end of the upper tile plus one.
 * - offset, stride: 9-Element Arrays containing offset and stride of the RenderBuffers.
 * - mean, variance: Target denoise buffers.
 * - rect: The prefilter area (lower pixels inclusive, upper pixels exclusive).
 */
ccl_device void kernel_filter_get_feature(int sample, float **buffers,
                                          int m_offset, int v_offset,
                                          int x, int y,
                                          int *tile_x, int *tile_y,
                                          int *offset, int *stride,
                                          float *mean, float *variance,
                                          int4 rect, int buffer_pass_stride,
                                          int buffer_denoising_offset, bool use_cross_denoising)
{
	int xtile = (x < tile_x[1])? 0: ((x < tile_x[2])? 1: 2);
	int ytile = (y < tile_y[1])? 0: ((y < tile_y[2])? 1: 2);
	int tile = ytile*3+xtile;
	float *center_buffer = buffers[tile] + (offset[tile] + y*stride[tile] + x)*buffer_pass_stride + buffer_denoising_offset;

	int buffer_w = align_up(rect.z - rect.x, 4);
	int idx = (y-rect.y)*buffer_w + (x - rect.x);

	/* TODO: This is an ugly hack! */
	if(m_offset >= 20 && m_offset <= 22 && use_cross_denoising) {
		int odd_sample = sample/2;
		mean[idx] = (center_buffer[m_offset] - center_buffer[m_offset+6]) / odd_sample;
		variance[idx] = center_buffer[v_offset] / (odd_sample * (sample-1));
	}
	else if(m_offset >= 26) {
		int even_sample = (sample+1)/2;
		mean[idx] = center_buffer[m_offset] / even_sample;
		variance[idx] = center_buffer[v_offset-6] / (even_sample * (sample-1));
	}
	else {
		mean[idx] = center_buffer[m_offset] / sample;
		variance[idx] = center_buffer[v_offset] / (sample * (sample-1));
	}
}

/* Combine A/B buffers.
 * Calculates the combined mean and the buffer variance. */
ccl_device void kernel_filter_combine_halves(int x, int y,
                                             float *mean, float *variance,
                                             float *a, float *b,
                                             int4 rect, int r)
{
	int buffer_w = align_up(rect.z - rect.x, 4);
	int idx = (y-rect.y)*buffer_w + (x - rect.x);

	if(mean)     mean[idx] = 0.5f * (a[idx]+b[idx]);
	if(variance) {
		if(r == 0) variance[idx] = 0.25f * (a[idx]-b[idx])*(a[idx]-b[idx]);
		else {
			variance[idx] = 0.0f;
			float values[25];
			int numValues = 0;
			for(int py = max(y-r, rect.y); py < min(y+r+1, rect.w); py++) {
				for(int px = max(x-r, rect.x); px < min(x+r+1, rect.z); px++) {
					int pidx = (py-rect.y)*buffer_w + (px-rect.x);
					values[numValues++] = 0.25f * (a[pidx]-b[pidx])*(a[pidx]-b[pidx]);
				}
			}
			/* Insertion-sort the variances (fast enough for 25 elements). */
			for(int i = 1; i < numValues; i++) {
				float v = values[i];
				int j;
				for(j = i-1; j >= 0 && values[j] > v; j--)
					values[j+1] = values[j];
				values[j+1] = v;
			}
			variance[idx] = values[(7*numValues)/8];
		}
	}
}

CCL_NAMESPACE_END
