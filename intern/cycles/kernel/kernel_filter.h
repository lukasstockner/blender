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

#define FOR_PIXEL_WINDOW pre_buffer = prefiltered + (low.y - prefilter_rect.y)*prefilter_w + (low.x - prefilter_rect.x); \
                         for(int py = low.y; py < high.y; py++) { \
                             int ytile = (py < tile_y[1])? 0: ((py < tile_y[2])? 1: 2); \
                             for(int px = low.x; px < high.x; px++, pre_buffer++) { \
                                 int xtile = (px < tile_x[1])? 0: ((px < tile_x[2])? 1: 2); \
                                 int tile = ytile*3+xtile; \
                                 buffer = buffers[tile] + (offset[tile] + py*stride[tile] + px)*kernel_data.film.pass_stride + kernel_data.film.pass_denoising;

#define END_FOR_PIXEL_WINDOW } \
                             pre_buffer += prefilter_w - (high.x - low.x); \
                         }

#define FEATURE_PASSES 8 /* Normals, Albedo, Depth */

ccl_device_inline void filter_get_features(int x, int y, float *buffer, float *pre_buffer, float sample, float *features, float *mean, int pre_stride)
{
	features[0] = x;
	features[1] = y;
	features[2] = pre_buffer[ 0*pre_stride];
	features[3] = pre_buffer[ 2*pre_stride];
	features[4] = pre_buffer[ 4*pre_stride];
	features[5] = pre_buffer[12*pre_stride];
	features[6] = pre_buffer[14*pre_stride];
	features[7] = pre_buffer[ 6*pre_stride];
	features[8] = pre_buffer[ 8*pre_stride];
	features[9] = pre_buffer[10*pre_stride];
/*	features[2] = buffer[12] * sample_scale;
	features[3] = buffer[0] * sample_scale;
	features[4] = buffer[1] * sample_scale;
	features[5] = buffer[2] * sample_scale;
	features[6] = pre_buffer->x;
	features[7] = buffer[6] * sample_scale;
	features[8] = buffer[7] * sample_scale;
	features[9] = buffer[8] * sample_scale;*/
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

ccl_device_inline void filter_get_feature_variance(int x, int y, float *buffer, float *pre_buffer, float sample, float *features, float *scale, int pre_stride)
{
	features[0] = 0.0f;
	features[1] = 0.0f;
	features[2] = pre_buffer[ 1*pre_stride];
	features[3] = pre_buffer[ 3*pre_stride];
	features[4] = pre_buffer[ 5*pre_stride];
	features[5] = pre_buffer[13*pre_stride];
	features[6] = pre_buffer[15*pre_stride];
	features[7] = pre_buffer[ 7*pre_stride];
	features[8] = pre_buffer[ 9*pre_stride];
	features[9] = pre_buffer[11*pre_stride];
/*	features[2] = saturate(buffer[13] * sample_scale_var) * sample_scale;
	features[3] = saturate(buffer[3] * sample_scale_var) * sample_scale;
	features[4] = saturate(buffer[4] * sample_scale_var) * sample_scale;
	features[5] = saturate(buffer[5] * sample_scale_var) * sample_scale;
	features[6] = saturate(pre_buffer->y);
	features[7] = saturate(buffer[9] * sample_scale_var) * sample_scale;
	features[8] = saturate(buffer[10] * sample_scale_var) * sample_scale;
	features[9] = saturate(buffer[11] * sample_scale_var) * sample_scale;*/
#ifdef DENOISE_SECOND_ORDER_SCREEN
	features[10] = 0.0f;
	features[11] = 0.0f;
	features[12] = 0.0f;
#endif
	for(int i = 0; i < DENOISE_FEATURES; i++)
		features[i] *= scale[i]*scale[i];
}

ccl_device_inline float3 filter_get_pixel_color(float *buffer, float sample)
{
	float sample_scale = 1.0f/sample;
	return make_float3(buffer[20], buffer[21], buffer[22]) * sample_scale;
}

ccl_device_inline float filter_get_pixel_variance(float *buffer, float sample)
{
	float sample_scale_var = 1.0f/(sample * (sample - 1.0f));
	return average(make_float3(buffer[23], buffer[24], buffer[25])) * sample_scale_var;
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


/* General Non-Local Means filter implementation.
 * NLM essentially is an extension of the bilaterail filter: It also loops over all the pixels in a neighborhood, calculates a weight for each one and combines them.
 * The difference is the weighting function: While the Bilateral filter just looks that the two pixels (center=p and pixel in neighborhood=q) and calculates the weight from
 * their distance and color difference, NLM considers small patches around both pixels and compares those. That way, it is able to identify similar image regions and compute
 * better weights.
 * One important consideration is that the image used for comparing patches doesn't have to be the one that's being filtered.
 * This is used in two different ways in the denoiser: First, by splitting the samples in half, we get two unbiased estimates of the image.
 * Then, we can use one of the halves to calculate the weights for filtering the other one. This way, the weights are decorrelated from the image and the result is smoother.
 * The second use is for variance: Sample variance (generated in the kernel) tends to be quite smooth, but is biased.
 * On the other hand, buffer variance, calculated from the difference of the two half images, is unbiased, but noisy.
 * Therefore, by filtering the buffer variance based on weights from the sample variance, we get the same smooth structure, but the unbiased result.

 * Parameters:
 * - x, y: The position that is to be filtered (=p in the algorithm)
 * - noisyImage: The image that is being filtered
 * - weightImage: The image used for comparing patches and calculating weights
 * - variance: The variance of the weight image (!), used to account for noisy input
 * - filteredImage: Output image, only pixel (x, y) will be written
 * - rect: The coordinates of the corners of the four images in image space.
 * - r: The half radius of the area over which q is looped
 * - f: The size of the patches that are used for comparing pixels
 * - a: Can be tweaked to account for noisy variance, generally a=1
 * - k_2: Squared k parameter of the NLM filter, general strength control (higher k => smoother image)
 */
ccl_device void kernel_filter_non_local_means(int x, int y, float *noisyImage, float *weightImage, float *variance, float *filteredImage, int4 rect, int r, int f, float a, float k_2)
{
	int2 low  = make_int2(max(rect.x, x - r),
	                      max(rect.y, y - r));
	int2 high = make_int2(min(rect.z, x + r + 1),
	                      min(rect.w, y + r + 1));

	float sum_image = 0.0f, sum_weight = 0.0f;

	int w = rect.z-rect.x;
	int p_idx = (y-rect.y)*w + (x - rect.x);
	int q_idx = (low.y-rect.y)*w + (low.x-rect.x);
	/* Loop over the q's, center pixels of all relevant patches. */
	for(int qy = low.y; qy < high.y; qy++) {
		for(int qx = low.x; qx < high.x; qx++, q_idx++) {
			float dI = 0.0f;
			int2  low_dPatch = make_int2(max(max(rect.x - qx, rect.x - x),  -f), max(max(rect.y - qy, rect.y - y),  -f));
			int2 high_dPatch = make_int2(min(min(rect.z - qx, rect.z - x), f+1), min(min(rect.w - qy, rect.w - y), f+1));
			int dIdx = low_dPatch.x + low_dPatch.y*w;
			/* Loop over the pixels in the patch.
			 * Note that the patch must be small enough to be fully inside the rect, both at p and q.
			 * Do avoid doing all the coordinate calculations twice, the code here computes both weights at once. */
			for(int dy = low_dPatch.y; dy < high_dPatch.y; dy++) {
				for(int dx = low_dPatch.x; dx < high_dPatch.x; dx++, dIdx++) {
					float diff = weightImage[p_idx+dIdx] - weightImage[q_idx+dIdx];
					dI += (diff*diff - a*(variance[p_idx+dIdx] + min(variance[p_idx+dIdx], variance[q_idx+dIdx]))) / (1e-7f + k_2*(variance[p_idx+dIdx] + variance[q_idx+dIdx]));
				}
				dIdx += w-(high_dPatch.x - low_dPatch.x);
			}
			dI /= ((high_dPatch.x - low_dPatch.x) * (high_dPatch.y - low_dPatch.y));

			float wI = expf(-max(0.0f, dI));
			sum_image += wI*noisyImage[q_idx];
			sum_weight += wI;
		}
		q_idx += w-(high.x-low.x);
	}

	filteredImage[p_idx] = sum_image / sum_weight;
}

/* First step of the prefiltering, performs the shadow division and stores all data
 * in a nice and easy rectangular array that can be passed to the NLM filter.
 *
 * Calculates:
 * unfiltered: Contains the two half images of the shadow feature pass
 * sampleVariance: The sample-based variance calculated in the kernel. Note: This calculation is biased in general, and especially here since the variance of the ratio can only be approximated.
 * sampleVarianceV: Variance of the sample variance estimation, quite noisy (since it's essentially the buffer variance of the two variance halves)
 * bufferVariance: The buffer-based variance of the shadow feature. Unbiased, but quite noisy.
 */
ccl_device void kernel_filter_divide_shadow(KernelGlobals *kg, int sample, float **buffers, int x, int y, int *tile_x, int *tile_y, int *offset, int *stride, float *unfiltered, float *sampleVariance, float *sampleVarianceV, float *bufferVariance, int4 prefilter_rect)
{
	int xtile = (x < tile_x[1])? 0: ((x < tile_x[2])? 1: 2);
	int ytile = (y < tile_y[1])? 0: ((y < tile_y[2])? 1: 2);
	int tile = ytile*3+xtile;
	float *center_buffer = buffers[tile] + (offset[tile] + y*stride[tile] + x)*kernel_data.film.pass_stride + kernel_data.film.pass_denoising;

	int idx = (y-prefilter_rect.y)*(prefilter_rect.z-prefilter_rect.x) + (x - prefilter_rect.x);
	int Bofs = (prefilter_rect.w - prefilter_rect.y)*(prefilter_rect.z - prefilter_rect.x);
	unfiltered[idx] = center_buffer[15] / max(center_buffer[14], 1e-7f);
	unfiltered[idx+Bofs] = center_buffer[18] / max(center_buffer[17], 1e-7f);
	float varFac = 1.0f / (sample * (sample-1));
	sampleVariance[idx] = (center_buffer[16] + center_buffer[19]) * varFac;
	sampleVarianceV[idx] = 0.5f * (center_buffer[16] - center_buffer[19]) * (center_buffer[16] - center_buffer[19]) * varFac;
	bufferVariance[idx] = 0.5f * (unfiltered[idx] - unfiltered[idx+Bofs]) * (unfiltered[idx] - unfiltered[idx+Bofs]);
}

ccl_device void kernel_filter_get_feature(KernelGlobals *kg, int sample, float **buffers, int m_offset, int v_offset, int x, int y, int *tile_x, int *tile_y, int *offset, int *stride, float *mean, float *variance, int4 prefilter_rect)
{
	int xtile = (x < tile_x[1])? 0: ((x < tile_x[2])? 1: 2);
	int ytile = (y < tile_y[1])? 0: ((y < tile_y[2])? 1: 2);
	int tile = ytile*3+xtile;
	float *center_buffer = buffers[tile] + (offset[tile] + y*stride[tile] + x)*kernel_data.film.pass_stride + kernel_data.film.pass_denoising;

	int idx = (y-prefilter_rect.y)*(prefilter_rect.z-prefilter_rect.x) + (x - prefilter_rect.x);
	mean[idx] = center_buffer[m_offset] / sample;
	variance[idx] = center_buffer[v_offset] / (sample * (sample-1));
}

ccl_device void kernel_filter_combine_halves(int x, int y, float *mean, float *variance, float *a, float *b, int4 prefilter_rect)
{
	int idx = (y-prefilter_rect.y)*(prefilter_rect.z-prefilter_rect.x) + (x - prefilter_rect.x);

	if(mean)     mean[idx] = 0.5f * (a[idx]+b[idx]);
	if(variance) variance[idx] = 0.5f * (a[idx]-b[idx])*(a[idx]-b[idx]);
}

/* Since the filtering may be performed across tile edged, all the neighboring tiles have to be passed along as well.
 * tile_x/y contain the x/y positions of the tile grid, 4 entries each:
 * - Start of the lower/left neighbor
 * - Start of the own tile
 * - Start of the upper/right neighbor
 * - Start of the next upper/right neighbor (not accessed)
 * buffers contains the nine buffer pointers (y-major ordering, starting with the lower left tile), offset and stride the respective parameters of the tile.
 */
ccl_device void kernel_filter_estimate_params(KernelGlobals *kg, int sample, float **buffers, int x, int y, int *tile_x, int *tile_y, int *offset, int *stride, FilterStorage *storage, float *prefiltered, int4 filter_rect, int4 prefilter_rect)
{
	storage += (y-filter_rect.y)*(filter_rect.z-filter_rect.x) + (x-filter_rect.x);
	int prefilter_w = (prefilter_rect.z - prefilter_rect.x);
	int pre_stride = (prefilter_rect.w - prefilter_rect.y) * (prefilter_rect.z - prefilter_rect.x);

	/* Temporary storage, used in different steps of the algorithm. */
	float tempmatrix[(2*DENOISE_FEATURES+1)*(2*DENOISE_FEATURES+1)], tempvector[4*DENOISE_FEATURES+1];
	float *buffer, features[DENOISE_FEATURES];
	float *pre_buffer;

	/* === Get center pixel color and variance. === */
	float *center_buffer = buffers[4] + (offset[4] + y*stride[4] + x)*kernel_data.film.pass_stride + kernel_data.film.pass_denoising;
	float *center_pre_buffer = prefiltered + (y - prefilter_rect.y)*prefilter_w + (x - prefilter_rect.x);
	float3 center_color  = filter_get_pixel_color(center_buffer, sample);
	float sqrt_center_variance = sqrtf(filter_get_pixel_variance(center_buffer, sample));




	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(tile_x[0], x - kernel_data.integrator.half_window),
	                      max(tile_y[0], y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(tile_x[3], x + kernel_data.integrator.half_window + 1),
	                      min(tile_y[3], y + kernel_data.integrator.half_window + 1));




	/* === Shift feature passes to have mean 0. === */
	float feature_means[DENOISE_FEATURES] = {0.0f};
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, buffer, pre_buffer, sample, features, NULL, pre_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			feature_means[i] += features[i];
	} END_FOR_PIXEL_WINDOW

	float pixel_scale = 1.0f / ((high.y - low.y) * (high.x - low.x));
	for(int i = 0; i < DENOISE_FEATURES; i++)
		feature_means[i] *= pixel_scale;

	/* === Scale the shifted feature passes to a range of [-1; 1], will be baked into the transform later. === */
	float *feature_scale = tempvector;
	math_vector_zero(feature_scale, DENOISE_FEATURES);

	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, buffer, pre_buffer, sample, features, feature_means, pre_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			feature_scale[i] = max(feature_scale[i], fabsf(features[i]));
	} END_FOR_PIXEL_WINDOW

	for(int i = 0; i < DENOISE_FEATURES; i++)
		feature_scale[i] = 1.0f / max(feature_scale[i], 0.01f);




	/* === Generate the feature transformation. ===
	 * This transformation maps the DENOISE_FEATURES-dimentional feature space to a reduced feature (r-feature) space
	 * which generally has fewer dimensions. This mainly helps to prevent overfitting. */
	float* feature_matrix = tempmatrix, feature_matrix_norm = 0.0f;
	math_matrix_zero_lower(feature_matrix, DENOISE_FEATURES);
#ifdef FULL_EIGENVALUE_NORM
	float *perturbation_matrix = tempmatrix + DENOISE_FEATURES*DENOISE_FEATURES;
	math_matrix_zero_lower(perturbation_matrix, FEATURE_PASSES);
#endif
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, buffer, pre_buffer, sample, features, feature_means, pre_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] *= feature_scale[i];
		math_add_gramian(feature_matrix, DENOISE_FEATURES, features, 1.0f);

		filter_get_feature_variance(px, py, buffer, pre_buffer, sample, features, feature_scale, pre_stride);
#ifdef FULL_EIGENVALUE_NORM
		math_add_gramian(perturbation_matrix, FEATURE_PASSES, features, 1.0f);
#else
		for(int i = 0; i < FEATURE_PASSES; i++)
			feature_matrix_norm += features[i];
#endif
	} END_FOR_PIXEL_WINDOW
	math_lower_tri_to_full(feature_matrix, DENOISE_FEATURES);

	float *feature_transform = &storage->transform[0], *singular = tempvector + DENOISE_FEATURES;
	int rank = svd(feature_matrix, feature_transform, singular, DENOISE_FEATURES);

#ifdef FULL_EIGENVALUE_NORM
	float *eigenvector_guess = tempvector + 2*DENOISE_FEATURES;
	for(int i = 0; i < DENOISE_FEATURES; i++)
		eigenvector_guess[i] = 1.0f;
	float singular_threshold = 0.01f + 2.0f * sqrtf(math_largest_eigenvalue(perturbation_matrix, FEATURE_PASSES, eigenvector_guess, tempvector + 3*DENOISE_FEATURES));
#else
	float singular_threshold = 0.01f + 2.0f * (sqrtf(feature_matrix_norm) / (sqrtf(rank) * 0.5f));
#endif

	rank = 0;
	for(int i = 0; i < DENOISE_FEATURES; i++, rank++) {
		float s = sqrtf(fabsf(singular[i]));
		if(i >= 2 && s < singular_threshold)
			break;
		/* Bake the feature scaling into the transformation matrix. */
		for(int j = 0; j < DENOISE_FEATURES; j++)
			feature_transform[rank*DENOISE_FEATURES + j] *= feature_scale[j];
	}

#ifdef WITH_CYCLES_DEBUG_FILTER
	storage->feature_matrix_norm = feature_matrix_norm;
	storage->singular_threshold = singular_threshold;
	for(int i = 0; i < DENOISE_FEATURES; i++) {
		storage->means[i] = feature_means[i];
		storage->scales[i] = feature_scale[i];
		storage->singular[i] = sqrtf(fabsf(singular[i]));
	}
#endif

	/* From here on, the mean of the features will be shifted to the central pixel's values. */
	filter_get_features(x, y, center_buffer, center_pre_buffer, sample, feature_means, NULL, pre_stride);




	/* === Estimate bandwidth for each r-feature dimension. ===
	 * To do so, the second derivative of the pixel color w.r.t. the particular r-feature
	 * has to be estimated. That is done by least-squares-fitting a model that includes
	 * both the r-feature vector z as well as z^T*z and using the resulting parameter for
	 * that dimension of the z^T*z vector times two as the derivative. */
	int matrix_size = 2*rank + 1; /* Constant term (1 dim) + z (rank dims) + z^T*z (rank dims) */
	float *XtX = tempmatrix, *design_row = tempvector;
	float3 XtY[2*DENOISE_FEATURES+1];

	math_matrix_zero_lower(XtX, matrix_size);
	math_vec3_zero(XtY, matrix_size);
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, buffer, pre_buffer, sample, features, feature_means, pre_stride);
		float weight = filter_fill_design_row(features, rank, design_row, feature_transform, NULL);
	
		if(weight == 0.0f) continue;
		weight /= max(1.0f, filter_get_pixel_variance(buffer, sample));

		math_add_gramian(XtX, matrix_size, design_row, weight);
		math_add_vec3(XtY, matrix_size, design_row, weight * filter_get_pixel_color(buffer, sample));
	} END_FOR_PIXEL_WINDOW

	/* Solve the normal equation of the linear least squares system: Decompose A = X^T*X into L
	 * so that L is a lower triangular matrix and L*L^T = A. Then, solve
	 * A*x = (L*L^T)*x = L*(L^T*x) = X^T*y by first solving L*b = X^T*y and then L^T*x = b through
	 * forward- and backsubstitution. */
	math_matrix_add_diagonal(XtX, matrix_size, 1e-3f); /* Improve the numerical stability. */
	math_cholesky(XtX, matrix_size); /* Decompose A=X^T*x to L. */
	math_substitute_forward_vec3(XtX, matrix_size, XtY); /* Solve L*b = X^T*y. */
	math_substitute_back_vec3(XtX, matrix_size, XtY); /* Solve L^T*x = b. */

	/* Calculate the inverse of the r-feature bandwidths. */
	float *bandwidth_factor = &storage->bandwidth[0];
	for(int i = 0; i < rank; i++)
		bandwidth_factor[i] = sqrtf(2.0f * average(fabs(XtY[1+rank+i])) + 0.16f);
	for(int i = rank; i < DENOISE_FEATURES; i++)
		bandwidth_factor[i] = 0.0f;




	/* === Estimate bias and variance for different candidate global bandwidths. === */
	const float candidate_bw[6] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
	double lsq_bias[LSQ_SIZE], lsq_variance[LSQ_SIZE];
	math_lsq_init(lsq_bias);
	math_lsq_init(lsq_variance);
	for(int g = 0; g < 6; g++) {
		float g_bandwidth_factor[DENOISE_FEATURES];
		for(int i = 0; i < rank; i++)
			/* Divide by the candidate bandwidth since the bandwidth_factor actually is the inverse of the bandwidth. */
			g_bandwidth_factor[i] = bandwidth_factor[i]/candidate_bw[g];

		matrix_size = rank+1;
		math_matrix_zero_lower(XtX, matrix_size);

		FOR_PIXEL_WINDOW {
			float3 color = filter_get_pixel_color(buffer, sample);
			float variance = filter_get_pixel_variance(buffer, sample);
			if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

			filter_get_features(px, py, buffer, pre_buffer, sample, features, feature_means, pre_stride);
			float weight = filter_fill_design_row(features, rank, design_row, feature_transform, g_bandwidth_factor);

			if(weight == 0.0f) continue;
			weight /= max(1.0f, filter_get_pixel_variance(buffer, sample));

			math_add_gramian(XtX, matrix_size, design_row, weight);
		} END_FOR_PIXEL_WINDOW

		math_matrix_add_diagonal(XtX, matrix_size, 1e-4f); /* Improve the numerical stability. */
		math_cholesky(XtX, matrix_size);
		math_inverse_lower_tri_inplace(XtX, matrix_size);

		float r_feature_weight[DENOISE_FEATURES+1];
		math_vector_zero(r_feature_weight, matrix_size);
		for(int col = 0; col < matrix_size; col++)
			for(int row = col; row < matrix_size; row++)
				r_feature_weight[col] += XtX[row]*XtX[col*matrix_size+row];

		float3 est_color = make_float3(0.0f, 0.0f, 0.0f), est_pos_color = make_float3(0.0f, 0.0f, 0.0f);
		float est_variance = 0.0f, est_pos_variance = 0.0f;
		float pos_weight = 0.0f;

		FOR_PIXEL_WINDOW {
			float3 color = filter_get_pixel_color(buffer, sample);
			float variance = filter_get_pixel_variance(buffer, sample);
			if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

			filter_get_features(px, py, buffer, pre_buffer, sample, features, feature_means, pre_stride);
			float weight = filter_fill_design_row(features, rank, design_row, feature_transform, g_bandwidth_factor);

			if(weight == 0.0f) continue;
			weight /= max(1.0f, variance);
			weight *= math_dot(design_row, r_feature_weight, matrix_size);

			est_color += weight * color;
			est_variance += weight*weight * max(variance, 0.0f);

			if(weight >= 0.0f) {
				est_pos_color += weight * color;
				est_pos_variance += weight*weight * max(variance, 0.0f);
				pos_weight += weight;
			}
		} END_FOR_PIXEL_WINDOW

		if(est_color.x < 0.0f || est_color.y < 0.0f || est_color.z < 0.0f) {
			float fac = 1.0f / max(pos_weight, 1e-5f);
			est_color = est_pos_color * fac;
			est_variance = est_pos_variance * fac*fac;
		}

		math_lsq_add(lsq_bias, (double) (candidate_bw[g]*candidate_bw[g]), (double) average(est_color - center_color));
		math_lsq_add(lsq_variance, pow(candidate_bw[g], -rank), max(sample*est_variance, 0.0f));
	}




	/* === Estimate optimal global bandwidth. === */
	double bias_coef = math_lsq_solve(lsq_bias, NULL);
	double variance_zeroth;
	double variance_coef = math_lsq_solve(lsq_variance, &variance_zeroth);
	if(variance_coef < 0.0) {
		variance_coef = -variance_coef;
		variance_zeroth = 0.0;
	}
	float optimal_bw = (float) pow((rank * variance_coef) / (4.0 * bias_coef*bias_coef * sample), 1.0 / (rank + 4));

#ifdef WITH_CYCLES_DEBUG_FILTER
	double h2 = ((double) optimal_bw) * ((double) optimal_bw);
	double bias = bias_coef*h2;
	double variance = (variance_zeroth + variance_coef*pow(optimal_bw, -rank)) / sample;
	storage->log_rmse_per_sample = ( (float) log(max(bias*bias + variance, 1e-20)) - 4.0f*logf(sample)/(rank + 4) );
#endif

	/* === Store the calculated data for the second kernel. === */
	storage->rank = rank;
	storage->global_bandwidth = optimal_bw;
}




ccl_device void kernel_filter_final_pass(KernelGlobals *kg, int sample, float **buffers, int x, int y, int *tile_x, int *tile_y, int *offset, int *stride, FilterStorage *storage, float *prefiltered, int4 filter_rect, int4 prefilter_rect)
{
	storage += (y-filter_rect.y)*(filter_rect.z-filter_rect.x) + (x-filter_rect.x);
	int prefilter_w = (prefilter_rect.z - prefilter_rect.x);
	int pre_stride = (prefilter_rect.w - prefilter_rect.y) * (prefilter_rect.z - prefilter_rect.x);
	float *buffer, features[DENOISE_FEATURES];
	float *pre_buffer;

	/* === Get center pixel. === */
	float *center_buffer = buffers[4] + (offset[4] + y*stride[4] + x)*kernel_data.film.pass_stride + kernel_data.film.pass_denoising;
	float3 center_color  = filter_get_pixel_color(center_buffer, sample);
	float *center_pre_buffer = prefiltered + (y - prefilter_rect.y)*prefilter_w + (x - prefilter_rect.x);
	float sqrt_center_variance = sqrtf(filter_get_pixel_variance(center_buffer, sample));
	float feature_means[DENOISE_FEATURES];
	filter_get_features(x, y, center_buffer, center_pre_buffer, sample, feature_means, NULL, pre_stride);




	/* === Fetch stored data from the previous kernel. === */
	float *feature_transform = &storage->transform[0];
	float *bandwidth_factor = &storage->bandwidth[0];
	int rank = storage->rank;
	/* Apply a median filter to the 3x3 window aroung the current pixel. */
	int sort_idx = 0;
	float global_bandwidths[9];
	for(int py = max(y-1, filter_rect.y); py < min(y+2, filter_rect.w); py++) {
		for(int px = max(x-1, filter_rect.x); px < min(x+2, filter_rect.z); px++) {
			int ofs = (py-y)*(filter_rect.z - filter_rect.x) + (px-x);
			if(storage[ofs].rank != rank) continue;
			global_bandwidths[sort_idx++] = storage[ofs].global_bandwidth;
		}
	}
	/* Insertion-sort the global bandwidths (fast enough for 9 elements). */
	for(int i = 1; i < sort_idx; i++) {
		float v = global_bandwidths[i];
		int j;
		for(j = i-1; j >= 0 && global_bandwidths[j] > v; j--)
			global_bandwidths[j+1] = global_bandwidths[j];
		global_bandwidths[j+1] = v;
	}
	float global_bandwidth = global_bandwidths[sort_idx/2];




	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(tile_x[0], x - kernel_data.integrator.half_window),
	                      max(tile_y[0], y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(tile_x[3], x + kernel_data.integrator.half_window + 1),
	                      min(tile_y[3], y + kernel_data.integrator.half_window + 1));





	/* === Calculate the final pixel color. === */
	float XtX[(DENOISE_FEATURES+1)*(DENOISE_FEATURES+1)], design_row[DENOISE_FEATURES+1];
	for(int i = 0; i < rank; i++)
		/* Same as above, divide by the bandwidth since the bandwidth_factor actually is the inverse of the bandwidth. */
		bandwidth_factor[i] /= global_bandwidth;

	int matrix_size = rank+1;
	math_matrix_zero_lower(XtX, matrix_size);

	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(buffer, sample);
		float variance = filter_get_pixel_variance(buffer, sample);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, buffer, pre_buffer, sample, features, feature_means, pre_stride);
		float weight = filter_fill_design_row(features, rank, design_row, feature_transform, bandwidth_factor);

		if(weight == 0.0f) continue;
		weight /= max(1.0f, filter_get_pixel_variance(buffer, sample));

		math_add_gramian(XtX, matrix_size, design_row, weight);
	} END_FOR_PIXEL_WINDOW

#ifdef WITH_CYCLES_DEBUG_FILTER
	storage->filtered_global_bandwidth = global_bandwidth;
	storage->sum_weight = XtX[0];
#endif

	math_matrix_add_diagonal(XtX, matrix_size, 1e-4f); /* Improve the numerical stability. */
	math_cholesky(XtX, matrix_size);
	math_inverse_lower_tri_inplace(XtX, matrix_size);

	float r_feature_weight[DENOISE_FEATURES+1];
	math_vector_zero(r_feature_weight, matrix_size);
	for(int col = 0; col < matrix_size; col++)
		for(int row = col; row < matrix_size; row++)
			r_feature_weight[col] += XtX[row]*XtX[col*matrix_size+row];

	float3 final_color = make_float3(0.0f, 0.0f, 0.0f);
	float3 final_pos_color = make_float3(0.0f, 0.0f, 0.0f);
	float pos_weight = 0.0f;
	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(buffer, sample);
		float variance = filter_get_pixel_variance(buffer, sample);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, buffer, pre_buffer, sample, features, feature_means, pre_stride);
		float weight = filter_fill_design_row(features, rank, design_row, feature_transform, bandwidth_factor);

		if(weight == 0.0f) continue;
		weight /= max(1.0f, variance);
		weight *= math_dot(design_row, r_feature_weight, matrix_size);

		final_color += weight * color;

		if(weight >= 0.0f) {
			final_pos_color += weight * color;
			pos_weight += weight;
		}
	} END_FOR_PIXEL_WINDOW

	if(final_color.x < 0.0f || final_color.y < 0.0f || final_color.z < 0.0f) {
		final_color = final_pos_color / max(pos_weight, 1e-5f);
	}
	final_color *= sample;

	center_buffer -= kernel_data.film.pass_denoising;
	if(kernel_data.film.pass_no_denoising)
		final_color += make_float3(center_buffer[kernel_data.film.pass_no_denoising],
		                           center_buffer[kernel_data.film.pass_no_denoising+1],
		                           center_buffer[kernel_data.film.pass_no_denoising+2]);

	center_buffer[0] = final_color.x;
	center_buffer[1] = final_color.y;
	center_buffer[2] = final_color.z;

#ifdef WITH_CYCLES_DEBUG_FILTER
	storage->log_rmse_per_sample -= 2.0f * logf(linear_rgb_to_gray(final_color) + 0.001f);
#endif
}

CCL_NAMESPACE_END
