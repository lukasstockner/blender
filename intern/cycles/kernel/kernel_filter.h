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

#include "kernel_filter_pre.h"
#include "kernel_filter_util.h"

CCL_NAMESPACE_BEGIN

/* Not all features are included in the matrix norm. */
#define NORM_FEATURE_OFFSET 2
#define NORM_FEATURE_NUM 8

#ifdef __KERNEL_CUDA__
ccl_device void kernel_filter_construct_transform(KernelGlobals *kg, int sample, float const* __restrict__ buffer, int x, int y, float *transform, FilterStorage *storage, int4 rect, int transform_stride, int localIdx)
{
	__shared__ float shared_features[DENOISE_FEATURES*CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH];
	float *features = shared_features + localIdx*DENOISE_FEATURES;

	int buffer_w = align_up(rect.z - rect.x, 4);
	int buffer_h = (rect.w - rect.y);
	int pass_stride = buffer_h * buffer_w * kernel_data.film.num_frames;
	int num_frames = kernel_data.film.num_frames;
	int prev_frames = kernel_data.film.prev_frames;
	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(rect.x, x - kernel_data.integrator.half_window),
	                      max(rect.y, y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(rect.z, x + kernel_data.integrator.half_window + 1),
	                      min(rect.w, y + kernel_data.integrator.half_window + 1));
	float const* __restrict__ pixel_buffer;




	/* === Shift feature passes to have mean 0. === */
	float feature_means[DENOISE_FEATURES] = {0.0f};
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, pt, pixel_buffer, features, NULL, pass_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			feature_means[i] += features[i];
	} END_FOR_PIXEL_WINDOW

	float pixel_scale = 1.0f / ((high.y - low.y) * (high.x - low.x));
	for(int i = 0; i < DENOISE_FEATURES; i++)
		feature_means[i] *= pixel_scale;

	/* === Scale the shifted feature passes to a range of [-1; 1], will be baked into the transform later. === */
	float feature_scale[DENOISE_FEATURES];
	math_vector_zero(feature_scale, DENOISE_FEATURES);

	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			feature_scale[i] = max(feature_scale[i], fabsf(features[i]));
	} END_FOR_PIXEL_WINDOW

	for(int i = 0; i < DENOISE_FEATURES; i++)
		feature_scale[i] = 1.0f / max(feature_scale[i], 0.01f);



	/* === Generate the feature transformation. ===
	 * This transformation maps the DENOISE_FEATURES-dimentional feature space to a reduced feature (r-feature) space
	 * which generally has fewer dimensions. This mainly helps to prevent overfitting. */
	float feature_matrix[DENOISE_FEATURES*DENOISE_FEATURES], feature_matrix_norm = 0.0f;
	math_matrix_zero_lower(feature_matrix, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] *= feature_scale[i];
		math_add_gramian(feature_matrix, DENOISE_FEATURES, features, 1.0f);

		filter_get_feature_variance(px, py, pixel_buffer, features, feature_scale, pass_stride);
		for(int i = 0; i < NORM_FEATURE_NUM; i++)
			feature_matrix_norm += features[i + NORM_FEATURE_OFFSET]*kernel_data.integrator.filter_strength;
	} END_FOR_PIXEL_WINDOW
	math_lower_tri_to_full(feature_matrix, DENOISE_FEATURES);

	float singular[DENOISE_FEATURES];
	int rank = svd_cuda(feature_matrix, transform, transform_stride, singular, DENOISE_FEATURES);

	float singular_threshold = 0.01f + 2.0f * (sqrtf(feature_matrix_norm) / (sqrtf(rank) * 0.5f));

	rank = 0;
	for(int i = 0; i < DENOISE_FEATURES; i++, rank++) {
		float s = sqrtf(fabsf(singular[i]));
		if(i >= 2 && s < singular_threshold)
			break;
		/* Bake the feature scaling into the transformation matrix. */
		for(int j = 0; j < DENOISE_FEATURES; j++)
			transform[(rank*DENOISE_FEATURES + j)*transform_stride] *= feature_scale[j];
	}
	storage->rank = rank;

#ifdef WITH_CYCLES_DEBUG_FILTER
	storage->feature_matrix_norm = feature_matrix_norm;
	storage->singular_threshold = singular_threshold;
	for(int i = 0; i < DENOISE_FEATURES; i++) {
		storage->means[i] = feature_means[i];
		storage->scales[i] = feature_scale[i];
		storage->singular[i] = sqrtf(fabsf(singular[i]));
	}
#endif
}

ccl_device void kernel_filter_estimate_bandwidths(KernelGlobals *kg, int sample, float const* __restrict__ buffer, int x, int y, float const* __restrict__ transform, FilterStorage *storage, int4 rect, int transform_stride, int localIdx)
{
	__shared__ float shared_features[DENOISE_FEATURES*CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH];
	float *features = shared_features + localIdx*DENOISE_FEATURES;

	int buffer_w = align_up(rect.z - rect.x, 4);
	int buffer_h = (rect.w - rect.y);
	int pass_stride = buffer_h * buffer_w * kernel_data.film.num_frames;
	int num_frames = kernel_data.film.num_frames;
	int prev_frames = kernel_data.film.prev_frames;
	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(rect.x, x - kernel_data.integrator.half_window),
	                      max(rect.y, y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(rect.z, x + kernel_data.integrator.half_window + 1),
	                      min(rect.w, y + kernel_data.integrator.half_window + 1));
	float const* __restrict__ pixel_buffer;
	float const* __restrict__ center_buffer = buffer + (y - rect.y) * buffer_w + (x - rect.x);

	int rank = storage->rank;

	float feature_means[DENOISE_FEATURES];
	filter_get_features(x, y, 0, center_buffer, feature_means, NULL, pass_stride);

	/* === Estimate bandwidth for each r-feature dimension. ===
	 * To do so, the second derivative of the pixel color w.r.t. the particular r-feature
	 * has to be estimated. That is done by least-squares-fitting a model that includes
	 * both the r-feature vector z as well as z^T*z and using the resulting parameter for
	 * that dimension of the z^T*z vector times two as the derivative. */
	int matrix_size = 2*rank + 1; /* Constant term (1 dim) + z (rank dims) + z^T*z (rank dims) */
	float XtX[(2*DENOISE_FEATURES+1)*(2*DENOISE_FEATURES+1)], design_row[2*DENOISE_FEATURES+1];
	float3 XtY[2*DENOISE_FEATURES+1];

	math_matrix_zero_lower(XtX, matrix_size);
	math_vec3_zero(XtY, matrix_size);
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		float weight = filter_fill_design_row_cuda(features, rank, design_row, transform, transform_stride, NULL);
	
		if(weight == 0.0f) continue;
		weight /= max(1.0f, filter_get_pixel_variance(pixel_buffer, pass_stride));

		math_add_gramian(XtX, matrix_size, design_row, weight);
		math_add_vec3(XtY, matrix_size, design_row, weight * filter_get_pixel_color(pixel_buffer, pass_stride));
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
	for(int i = 0; i < rank; i++)
		storage->bandwidth[i] = sqrtf(2.0f * average(fabs(XtY[1+rank+i])) + 0.16f);
	for(int i = rank; i < DENOISE_FEATURES; i++)
		storage->bandwidth[i] = 0.0f;
}

ccl_device void kernel_filter_estimate_bias_variance(KernelGlobals *kg, int sample, float const* __restrict__ buffer, int x, int y, float const* __restrict__ transform, FilterStorage *storage, int4 rect, int candidate, int transform_stride, int localIdx)
{
	__shared__ float shared_features[DENOISE_FEATURES*CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH];
	float *features = shared_features + DENOISE_FEATURES*localIdx;

	int buffer_w = align_up(rect.z - rect.x, 4);
	int buffer_h = (rect.w - rect.y);
	int pass_stride = buffer_h * buffer_w * kernel_data.film.num_frames;
	int num_frames = kernel_data.film.num_frames;
	int prev_frames = kernel_data.film.prev_frames;
	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(rect.x, x - kernel_data.integrator.half_window),
	                      max(rect.y, y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(rect.z, x + kernel_data.integrator.half_window + 1),
	                      min(rect.w, y + kernel_data.integrator.half_window + 1));
	float const* __restrict__ pixel_buffer;
	float const* __restrict__ center_buffer = buffer + (y - rect.y) * buffer_w + (x - rect.x);
	float3 center_color  = filter_get_pixel_color(center_buffer, pass_stride);
	float sqrt_center_variance = sqrtf(filter_get_pixel_variance(center_buffer, pass_stride));

	int rank = storage->rank;

	float feature_means[DENOISE_FEATURES];
	filter_get_features(x, y, 0, center_buffer, feature_means, NULL, pass_stride);


	float g_bandwidth_factor[DENOISE_FEATURES];
	const float candidate_bw[6] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
	for(int i = 0; i < rank; i++)
		/* Divide by the candidate bandwidth since the bandwidth_factor actually is the inverse of the bandwidth. */
		g_bandwidth_factor[i] = storage->bandwidth[i]/candidate_bw[candidate];

	int matrix_size = rank+1;
	float XtX[(DENOISE_FEATURES+1)*(DENOISE_FEATURES+1)], design_row[DENOISE_FEATURES+1];
	math_matrix_zero_lower(XtX, matrix_size);

	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
		float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		float weight = filter_fill_design_row_cuda(features, rank, design_row, transform, transform_stride, g_bandwidth_factor);

		if(weight == 0.0f) continue;
		weight /= max(1.0f, variance);

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
		float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
		float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		float weight = filter_fill_design_row_cuda(features, rank, design_row, transform, transform_stride, g_bandwidth_factor);

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

	storage->est_bias[candidate] = average(est_color - center_color);
	storage->est_variance[candidate] = est_variance;
}

ccl_device void kernel_filter_calculate_bandwidth(KernelGlobals *kg, int sample, FilterStorage *storage)
{
	double lsq_bias[LSQ_SIZE], lsq_variance[LSQ_SIZE];
	math_lsq_init(lsq_bias);
	math_lsq_init(lsq_variance);

	const float candidate_bw[6] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
	for(int g = 0; g < 6; g++) {
		math_lsq_add(lsq_bias, (double) (candidate_bw[g]*candidate_bw[g]), (double) storage->est_bias[g]);
		math_lsq_add(lsq_variance, pow(candidate_bw[g], -storage->rank), max(sample*storage->est_variance[g], 0.0f));
	}

	/* === Estimate optimal global bandwidth. === */
	double bias_coef = math_lsq_solve(lsq_bias, NULL);
	double variance_coef = math_lsq_solve(lsq_variance, NULL);
	if(variance_coef < 0.0) {
		variance_coef = -variance_coef;
	}
	storage->global_bandwidth = (float) pow((storage->rank * variance_coef) / (4.0 * bias_coef*bias_coef * sample), 1.0 / (storage->rank + 4));
}

ccl_device void kernel_filter_final_pass(KernelGlobals *kg, int sample, float const* __restrict__ buffer, int x, int y, int offset, int stride, float *buffers, float const* __restrict__ transform, FilterStorage *storage, int4 filter_area, int4 rect, int transform_stride, int localIdx)
{
	__shared__ float shared_features[DENOISE_FEATURES*CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH];
	float *features = shared_features + DENOISE_FEATURES*localIdx;

	int buffer_w = align_up(rect.z - rect.x, 4);
	int buffer_h = (rect.w - rect.y);
	int pass_stride = buffer_h * buffer_w * kernel_data.film.num_frames;
	int num_frames = kernel_data.film.num_frames;
	int prev_frames = kernel_data.film.prev_frames;
	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(rect.x, x - kernel_data.integrator.half_window),
	                      max(rect.y, y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(rect.z, x + kernel_data.integrator.half_window + 1),
	                      min(rect.w, y + kernel_data.integrator.half_window + 1));
	float const* __restrict__ pixel_buffer;
	/* === Get center pixel. === */
	float const* __restrict__ center_buffer = buffer + (y - rect.y) * buffer_w + (x - rect.x);
	float3 center_color  = filter_get_pixel_color(center_buffer, pass_stride);
	float sqrt_center_variance = sqrtf(filter_get_pixel_variance(center_buffer, pass_stride));

	float feature_means[DENOISE_FEATURES];
	filter_get_features(x, y, 0, center_buffer, feature_means, NULL, pass_stride);




	/* === Fetch stored data from the previous kernel. === */
	float *bandwidth_factor = &storage->bandwidth[0];
	int rank = storage->rank;
	/* Apply a median filter to the 3x3 window aroung the current pixel. */
	int sort_idx = 0;
	float global_bandwidths[9];
	for(int dy = max(-1, filter_area.y - y); dy < min(2, filter_area.y+filter_area.w - y); dy++) {
		for(int dx = max(-1, filter_area.x - x); dx < min(2, filter_area.x+filter_area.z - x); dx++) {
			int ofs = dy*filter_area.z + dx;
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









	/* === Calculate the final pixel color. === */
	float XtX[(DENOISE_FEATURES+1)*(DENOISE_FEATURES+1)], design_row[DENOISE_FEATURES+1];
	for(int i = 0; i < rank; i++)
		/* Same as above, divide by the bandwidth since the bandwidth_factor actually is the inverse of the bandwidth. */
		bandwidth_factor[i] /= global_bandwidth;

	int matrix_size = rank+1;
	math_matrix_zero_lower(XtX, matrix_size);

	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
		float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		float weight = filter_fill_design_row_cuda(features, rank, design_row, transform, transform_stride, bandwidth_factor);

		if(weight == 0.0f) continue;
		weight /= max(1.0f, variance);

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
		float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
		float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		float weight = filter_fill_design_row_cuda(features, rank, design_row, transform, transform_stride, bandwidth_factor);

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

	float *combined_buffer = buffers + (offset + y*stride + x)*kernel_data.film.pass_stride;
	if(kernel_data.film.pass_no_denoising)
		final_color += make_float3(combined_buffer[kernel_data.film.pass_no_denoising],
		                           combined_buffer[kernel_data.film.pass_no_denoising+1],
		                           combined_buffer[kernel_data.film.pass_no_denoising+2]);

	combined_buffer[0] = final_color.x;
	combined_buffer[1] = final_color.y;
	combined_buffer[2] = final_color.z;

#ifdef WITH_CYCLES_DEBUG_FILTER
	storage->log_rmse_per_sample -= 2.0f * logf(linear_rgb_to_gray(final_color) + 0.001f);
#endif
}

#else

#  ifdef __KERNEL_SSE3__
ccl_device void kernel_filter_estimate_params(KernelGlobals *kg, int sample, float *buffer, int x, int y, FilterStorage *storage, int4 rect)
{
	int buffer_w = align_up(rect.z - rect.x, 4);
	int buffer_h = (rect.w - rect.y);
	int pass_stride = buffer_h * buffer_w * kernel_data.film.num_frames;
	int num_frames = kernel_data.film.num_frames;
	int prev_frames = kernel_data.film.prev_frames;

	__m128 features[DENOISE_FEATURES];
	float *pixel_buffer;

	int2 low  = make_int2(max(rect.x, x - kernel_data.integrator.half_window),
	                      max(rect.y, y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(rect.z, x + kernel_data.integrator.half_window + 1),
	                      min(rect.w, y + kernel_data.integrator.half_window + 1));

	__m128 feature_means[DENOISE_FEATURES] = {_mm_setzero_ps()};
	FOR_PIXEL_WINDOW_SSE {
		filter_get_features_sse(x4, y4, t4, active_pixels, pixel_buffer, features, NULL, pass_stride);
		math_add_vector_sse(feature_means, DENOISE_FEATURES, features);
	} END_FOR_PIXEL_WINDOW_SSE

	__m128 pixel_scale = _mm_set1_ps(1.0f / ((high.y - low.y) * (high.x - low.x)));
	for(int i = 0; i < DENOISE_FEATURES; i++) {
		feature_means[i] = _mm_mul_ps(_mm_hsum_ps(feature_means[i]), pixel_scale);
	}

	__m128 feature_scale[DENOISE_FEATURES] = {_mm_setzero_ps()};
	FOR_PIXEL_WINDOW_SSE {
		filter_get_features_sse(x4, y4, t4, active_pixels, pixel_buffer, features, feature_means, pass_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			feature_scale[i] = _mm_max_ps(feature_scale[i], _mm_fabs_ps(features[i]));
	} END_FOR_PIXEL_WINDOW_SSE

	for(int i = 0; i < DENOISE_FEATURES; i++)
		feature_scale[i] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(feature_scale[i]), _mm_set1_ps(0.01f)));

	__m128 feature_matrix_sse[DENOISE_FEATURES*DENOISE_FEATURES];
	__m128 feature_matrix_norm = _mm_setzero_ps();
	math_matrix_zero_lower_sse(feature_matrix_sse, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW_SSE {
		filter_get_features_sse(x4, y4, t4, active_pixels, pixel_buffer, features, feature_means, pass_stride);
		math_mul_vector_sse(features, DENOISE_FEATURES, feature_scale);
		math_add_gramian_sse(feature_matrix_sse, DENOISE_FEATURES, features, _mm_set1_ps(1.0f));

		filter_get_feature_variance_sse(x4, y4, active_pixels, pixel_buffer, features, feature_scale, pass_stride);
		math_mul_vector_scalar_sse(features, DENOISE_FEATURES, _mm_set1_ps(kernel_data.integrator.filter_strength));
		for(int i = 0; i < NORM_FEATURE_NUM; i++)
			feature_matrix_norm = _mm_add_ps(feature_matrix_norm, features[i + NORM_FEATURE_OFFSET]);
	} END_FOR_PIXEL_WINDOW_SSE

	float feature_matrix[DENOISE_FEATURES*DENOISE_FEATURES];
	math_hsum_matrix_lower(feature_matrix, DENOISE_FEATURES, feature_matrix_sse);

	math_lower_tri_to_full(feature_matrix, DENOISE_FEATURES);

	float *feature_transform = &storage->transform[0], singular[DENOISE_FEATURES];
	__m128 feature_transform_sse[DENOISE_FEATURES*DENOISE_FEATURES];
	int rank = svd(feature_matrix, feature_transform, singular, DENOISE_FEATURES);
	float singular_threshold = 0.01f + 2.0f * (sqrtf(_mm_hsum_ss(feature_matrix_norm)) / (sqrtf(rank) * 0.5f));

	rank = 0;
	for(int i = 0; i < DENOISE_FEATURES; i++, rank++) {
		float s = sqrtf(fabsf(singular[i]));
		if(i >= 2 && s < singular_threshold)
			break;
		/* Bake the feature scaling into the transformation matrix. */
		for(int j = 0; j < DENOISE_FEATURES; j++) {
			feature_transform[rank*DENOISE_FEATURES + j] *= _mm_cvtss_f32(feature_scale[j]);
			feature_transform_sse[rank*DENOISE_FEATURES + j] = _mm_set1_ps(feature_transform[rank*DENOISE_FEATURES + j]);
		}
	}

	/* From here on, the mean of the features will be shifted to the central pixel's values. */
	float feature_means_scalar[DENOISE_FEATURES];
	float const* __restrict__ center_buffer = buffer + (y - rect.y) * buffer_w + (x - rect.x);
	filter_get_features(x, y, 0, center_buffer, feature_means_scalar, NULL, pass_stride);
	for(int i = 0; i < DENOISE_FEATURES; i++)
		feature_means[i] = _mm_set1_ps(feature_means_scalar[i]);


	/* === Estimate bandwidth for each r-feature dimension. ===
	 * To do so, the second derivative of the pixel color w.r.t. the particular r-feature
	 * has to be estimated. That is done by least-squares-fitting a model that includes
	 * both the r-feature vector z as well as z^T*z and using the resulting parameter for
	 * that dimension of the z^T*z vector times two as the derivative. */
	int matrix_size = 2*rank + 1; /* Constant term (1 dim) + z (rank dims) + z^T*z (rank dims) */
	__m128 XtX_sse[(2*DENOISE_FEATURES+1)*(2*DENOISE_FEATURES+1)], design_row[(2*DENOISE_FEATURES+1)];
	float3 XtY[2*DENOISE_FEATURES+1];

	math_matrix_zero_lower_sse(XtX_sse, matrix_size);
	math_vec3_zero(XtY, matrix_size);
	FOR_PIXEL_WINDOW_SSE {
		filter_get_features_sse(x4, y4, t4, active_pixels, pixel_buffer, features, feature_means, pass_stride);
		__m128 weight = filter_fill_design_row_sse(features, active_pixels, rank, design_row, feature_transform_sse, NULL);
		active_pixels = _mm_and_ps(active_pixels, _mm_cmpneq_ps(weight, _mm_setzero_ps()));

		if(!_mm_movemask_ps(active_pixels)) continue;
		weight = _mm_mul_ps(weight, _mm_rcp_ps(_mm_max_ps(_mm_set1_ps(1.0f), filter_get_pixel_variance_sse(pixel_buffer, active_pixels, pass_stride))));

		math_add_gramian_sse(XtX_sse, matrix_size, design_row, weight);

		__m128 color[3];
		filter_get_pixel_color_sse(pixel_buffer, active_pixels, color, pass_stride);
		math_mul_vector_scalar_sse(color, 3, weight);
		for(int row = 0; row < matrix_size; row++) {
			__m128 color_row[3] = {color[0], color[1], color[2]};
			math_mul_vector_scalar_sse(color_row, 3, design_row[row]);
			XtY[row] += math_sum_float3(color_row);
		}
	} END_FOR_PIXEL_WINDOW_SSE

	float XtX[(2*DENOISE_FEATURES+1)*(2*DENOISE_FEATURES+1)];
	math_hsum_matrix_lower(XtX, matrix_size, XtX_sse);

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


	float3 center_color  = filter_get_pixel_color(center_buffer, pass_stride);
	float sqrt_center_variance = sqrtf(filter_get_pixel_variance(center_buffer, pass_stride));
	__m128 center_color_sse[3] = {_mm_set1_ps(center_color.x), _mm_set1_ps(center_color.y), _mm_set1_ps(center_color.z)};
	__m128 sqrt_center_variance_sse = _mm_set1_ps(sqrt_center_variance);

	const float candidate_bw[6] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
	double lsq_bias[LSQ_SIZE], lsq_variance[LSQ_SIZE];
	math_lsq_init(lsq_bias);
	math_lsq_init(lsq_variance);
	for(int g = 0; g < 6; g++) {
		__m128 g_bandwidth_factor[DENOISE_FEATURES];
		for(int i = 0; i < rank; i++)
			/* Divide by the candidate bandwidth since the bandwidth_factor actually is the inverse of the bandwidth. */
			g_bandwidth_factor[i] = _mm_set1_ps(bandwidth_factor[i]/candidate_bw[g]);

		matrix_size = rank+1;
		math_matrix_zero_lower_sse(XtX_sse, matrix_size);

		FOR_PIXEL_WINDOW_SSE {
			__m128 color[3];
			filter_get_pixel_color_sse(pixel_buffer, active_pixels, color, pass_stride);
			__m128 variance = filter_get_pixel_variance_sse(pixel_buffer, active_pixels, pass_stride);
			active_pixels = _mm_and_ps(active_pixels, filter_firefly_rejection_sse(color, variance, center_color_sse, sqrt_center_variance_sse));
			if(!_mm_movemask_ps(active_pixels)) continue;

			filter_get_features_sse(x4, y4, t4, active_pixels, pixel_buffer, features, feature_means, pass_stride);
			__m128 weight = filter_fill_design_row_sse(features, active_pixels, rank, design_row, feature_transform_sse, g_bandwidth_factor);
			active_pixels = _mm_and_ps(active_pixels, _mm_cmpneq_ps(weight, _mm_setzero_ps()));
			if(!_mm_movemask_ps(active_pixels)) continue;

			weight = _mm_mul_ps(weight, _mm_rcp_ps(_mm_max_ps(_mm_set1_ps(1.0f), variance)));

			math_add_gramian_sse(XtX_sse, matrix_size, design_row, weight);
		} END_FOR_PIXEL_WINDOW_SSE
		math_hsum_matrix_lower(XtX, matrix_size, XtX_sse);

		math_matrix_add_diagonal(XtX, matrix_size, 1e-4f); /* Improve the numerical stability. */
		math_cholesky(XtX, matrix_size);
		math_inverse_lower_tri_inplace(XtX, matrix_size);

		float r_feature_weight_scalar[DENOISE_FEATURES+1];
		math_vector_zero(r_feature_weight_scalar, matrix_size);
		for(int col = 0; col < matrix_size; col++)
			for(int row = col; row < matrix_size; row++)
				r_feature_weight_scalar[col] += XtX[row]*XtX[col*matrix_size+row];
		__m128 r_feature_weight[DENOISE_FEATURES+1];
		for(int col = 0; col < matrix_size; col++)
			r_feature_weight[col] = _mm_set1_ps(r_feature_weight_scalar[col]);

		__m128 est_pos_color[3] = {_mm_setzero_ps()}, est_color[3] = {_mm_setzero_ps()};
		__m128 est_variance = _mm_setzero_ps(), est_pos_variance = _mm_setzero_ps(), pos_weight_sse = _mm_setzero_ps();

		FOR_PIXEL_WINDOW_SSE {
			__m128 color[3];
			filter_get_pixel_color_sse(pixel_buffer, active_pixels, color, pass_stride);
			__m128 variance = filter_get_pixel_variance_sse(pixel_buffer, active_pixels, pass_stride);
			active_pixels = _mm_and_ps(active_pixels, filter_firefly_rejection_sse(color, variance, center_color_sse, sqrt_center_variance_sse));

			filter_get_features_sse(x4, y4, t4, active_pixels, pixel_buffer, features, feature_means, pass_stride);
			__m128 weight = filter_fill_design_row_sse(features, active_pixels, rank, design_row, feature_transform_sse, g_bandwidth_factor);
			active_pixels = _mm_and_ps(active_pixels, _mm_cmpneq_ps(weight, _mm_setzero_ps()));

			/* Early out if all pixels were masked away. */
			if(!_mm_movemask_ps(active_pixels)) continue;

			weight = _mm_mul_ps(weight, _mm_mul_ps(math_dot_sse(design_row, r_feature_weight, matrix_size), _mm_rcp_ps(_mm_max_ps(_mm_set1_ps(1.0f), variance))));

			math_mul_vector_scalar_sse(color, 3, weight);
			math_add_vector_sse(est_color, 3, color);
			__m128 variance_inc = _mm_mul_ps(_mm_mul_ps(weight, weight), _mm_max_ps(variance, _mm_setzero_ps()));
			est_variance = _mm_add_ps(est_variance, variance_inc);

			__m128 posmask = _mm_and_ps(active_pixels, _mm_cmpgt_ps(weight, _mm_setzero_ps()));
			math_mask_vector_sse(color, 3, posmask);
			math_add_vector_sse(est_pos_color, 3, color);
			est_pos_variance = _mm_add_ps(est_pos_variance, _mm_mask_ps(variance_inc, posmask));
			pos_weight_sse = _mm_add_ps(pos_weight_sse, _mm_mask_ps(weight, posmask));
		} END_FOR_PIXEL_WINDOW_SSE

		float3 est_color_scalar = math_sum_float3(est_color);
		float est_variance_scalar = _mm_hsum_ss(est_variance);

		if(est_color_scalar.x < 0.0f || est_color_scalar.y < 0.0f || est_color_scalar.z < 0.0f) {
			float fac = 1.0f / max(_mm_hsum_ss(pos_weight_sse), 1e-5f);
			est_color_scalar = math_sum_float3(est_pos_color) * fac;
			est_variance_scalar = _mm_hsum_ss(est_pos_variance) * fac*fac;
		}

		math_lsq_add(lsq_bias, (double) (candidate_bw[g]*candidate_bw[g]), (double) average(est_color_scalar - center_color));
		math_lsq_add(lsq_variance, pow(candidate_bw[g], -rank), max(sample*est_variance_scalar, 0.0f));
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

#  else

ccl_device void kernel_filter_estimate_params(KernelGlobals *kg, int sample, float const* __restrict__ buffer, int x, int y, FilterStorage *storage, int4 rect)
{
	float features[DENOISE_FEATURES];

	int buffer_w = align_up(rect.z - rect.x, 4);
	int buffer_h = (rect.w - rect.y);
	int pass_stride = buffer_h * buffer_w * kernel_data.film.num_frames;
	int num_frames = kernel_data.film.num_frames;
	int prev_frames = kernel_data.film.prev_frames;

	/* Temporary storage, used in different steps of the algorithm. */
	float tempmatrix[(2*DENOISE_FEATURES+1)*(2*DENOISE_FEATURES+1)];
	float tempvector[2*DENOISE_FEATURES+1];
	float const* __restrict__ pixel_buffer;

	/* === Get center pixel color and variance. === */
	float const* __restrict__ center_buffer = buffer + (y - rect.y) * buffer_w + (x - rect.x);
	float3 center_color  = filter_get_pixel_color(center_buffer, pass_stride);
	float sqrt_center_variance = sqrtf(filter_get_pixel_variance(center_buffer, pass_stride));




	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(rect.x, x - kernel_data.integrator.half_window),
	                      max(rect.y, y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(rect.z, x + kernel_data.integrator.half_window + 1),
	                      min(rect.w, y + kernel_data.integrator.half_window + 1));




	/* === Shift feature passes to have mean 0. === */
	float feature_means[DENOISE_FEATURES] = {0.0f};
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, pt, pixel_buffer, features, NULL, pass_stride);
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
		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
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
	math_matrix_zero_lower(perturbation_matrix, NORM_FEATURE_NUM);
#endif
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] *= feature_scale[i];
		math_add_gramian(feature_matrix, DENOISE_FEATURES, features, 1.0f);

		filter_get_feature_variance(px, py, pixel_buffer, features, feature_scale, pass_stride);
#ifdef FULL_EIGENVALUE_NORM
		math_add_gramian(perturbation_matrix, NORM_FEATURE_NUM, features, kernel_data.integrator.filter_strength);
#else
		for(int i = 0; i < NORM_FEATURE_NUM; i++)
			feature_matrix_norm += features[i + NORM_FEATURE_OFFSET]*kernel_data.integrator.filter_strength;
#endif
	} END_FOR_PIXEL_WINDOW
	math_lower_tri_to_full(feature_matrix, DENOISE_FEATURES);

	float *feature_transform = &storage->transform[0], *singular = tempvector + DENOISE_FEATURES;
	int rank = svd(feature_matrix, feature_transform, singular, DENOISE_FEATURES);

#ifdef FULL_EIGENVALUE_NORM
	float tempvector_2[2*DENOISE_FEATURES];
	for(int i = 0; i < DENOISE_FEATURES; i++)
		tempvector_2[i] = 1.0f;
	float singular_threshold = 0.01f + 2.0f * sqrtf(math_largest_eigenvalue(perturbation_matrix, NORM_FEATURE_NUM, tempvector_2, tempvector_2 + DENOISE_FEATURES));
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
	filter_get_features(x, y, 0, center_buffer, feature_means, NULL, pass_stride);




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
		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		float weight = filter_fill_design_row(features, rank, design_row, feature_transform, NULL);
	
		if(weight == 0.0f) continue;
		weight /= max(1.0f, filter_get_pixel_variance(pixel_buffer, pass_stride));

		math_add_gramian(XtX, matrix_size, design_row, weight);
		math_add_vec3(XtY, matrix_size, design_row, weight * filter_get_pixel_color(pixel_buffer, pass_stride));
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
			float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
			float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
			if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

			filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
			float weight = filter_fill_design_row(features, rank, design_row, feature_transform, g_bandwidth_factor);

			if(weight == 0.0f) continue;
			weight /= max(1.0f, variance);

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
			float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
			float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
			if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

			filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
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

#  endif // __KERNEL_SSE3__

ccl_device void kernel_filter_final_pass(KernelGlobals *kg, int sample, float *buffer, int x, int y, int offset, int stride, float *buffers, FilterStorage *storage, int4 filter_area, int4 rect)
{
	int buffer_w = align_up(rect.z - rect.x, 4);
	int buffer_h = (rect.w - rect.y);
	int pass_stride = buffer_h * buffer_w * kernel_data.film.num_frames;
	int num_frames = kernel_data.film.num_frames;
	int prev_frames = kernel_data.film.prev_frames;

	float features[DENOISE_FEATURES];
	float *pixel_buffer;

	/* === Get center pixel. === */
	float *center_buffer = buffer + (y - rect.y) * buffer_w + (x - rect.x);
	float3 center_color  = filter_get_pixel_color(center_buffer, pass_stride);
	float sqrt_center_variance = sqrtf(filter_get_pixel_variance(center_buffer, pass_stride));

	float feature_means[DENOISE_FEATURES];
	filter_get_features(x, y, 0, center_buffer, feature_means, NULL, pass_stride);




	/* === Fetch stored data from the previous kernel. === */
	float *feature_transform = &storage->transform[0];
	float *bandwidth_factor = &storage->bandwidth[0];
	int rank = storage->rank;
	/* Apply a median filter to the 3x3 window aroung the current pixel. */
	int sort_idx = 0;
	float global_bandwidths[9];
	for(int dy = max(-1, filter_area.y - y); dy < min(2, filter_area.y+filter_area.w - y); dy++) {
		for(int dx = max(-1, filter_area.x - x); dx < min(2, filter_area.x+filter_area.z - x); dx++) {
			int ofs = dy*filter_area.z + dx;
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
	int2 low  = make_int2(max(rect.x, x - kernel_data.integrator.half_window),
	                      max(rect.y, y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(rect.z, x + kernel_data.integrator.half_window + 1),
	                      min(rect.w, y + kernel_data.integrator.half_window + 1));





	/* === Calculate the final pixel color. === */
	float XtX[(DENOISE_FEATURES+1)*(DENOISE_FEATURES+1)], design_row[DENOISE_FEATURES+1];
	for(int i = 0; i < rank; i++)
		/* Same as above, divide by the bandwidth since the bandwidth_factor actually is the inverse of the bandwidth. */
		bandwidth_factor[i] /= global_bandwidth;

	int matrix_size = rank+1;
	math_matrix_zero_lower(XtX, matrix_size);

	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
		float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
		float weight = filter_fill_design_row(features, rank, design_row, feature_transform, bandwidth_factor);

		if(weight == 0.0f) continue;
		weight /= max(1.0f, variance);

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
		float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
		float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, pt, pixel_buffer, features, feature_means, pass_stride);
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

	float *combined_buffer = buffers + (offset + y*stride + x)*kernel_data.film.pass_stride;
	if(kernel_data.film.pass_no_denoising)
		final_color += make_float3(combined_buffer[kernel_data.film.pass_no_denoising],
		                           combined_buffer[kernel_data.film.pass_no_denoising+1],
		                           combined_buffer[kernel_data.film.pass_no_denoising+2]);

	combined_buffer[0] = final_color.x;
	combined_buffer[1] = final_color.y;
	combined_buffer[2] = final_color.z;

#ifdef WITH_CYCLES_DEBUG_FILTER
	storage->log_rmse_per_sample -= 2.0f * logf(linear_rgb_to_gray(final_color) + 0.001f);
#endif
}

#endif // __KERNEL_CUDA__

CCL_NAMESPACE_END
