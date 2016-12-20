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

ccl_device void kernel_filter_construct_transform(KernelGlobals *kg, int sample, float ccl_readonly_ptr buffer, int x, int y, float *transform, CUDAFilterStorage *storage, int4 rect, int transform_stride, int localIdx)
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
	float ccl_readonly_ptr pixel_buffer;
	int3 pixel;




	/* === Shift feature passes to have mean 0. === */
	float feature_means[DENOISE_FEATURES] = {0.0f};
	FOR_PIXEL_WINDOW {
		filter_get_features(pixel, pixel_buffer, features, NULL, pass_stride);
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
		filter_get_feature_scales(pixel, pixel_buffer, features, feature_means, pass_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			feature_scale[i] = max(feature_scale[i], features[i]);
	} END_FOR_PIXEL_WINDOW

	filter_calculate_scale(feature_scale);



	/* === Generate the feature transformation. ===
	 * This transformation maps the DENOISE_FEATURES-dimentional feature space to a reduced feature (r-feature) space
	 * which generally has fewer dimensions. This mainly helps to prevent overfitting. */
	float feature_matrix[DENOISE_FEATURES*DENOISE_FEATURES], feature_matrix_norm = 0.0f;
	math_matrix_zero_lower(feature_matrix, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW {
		filter_get_features(pixel, pixel_buffer, features, feature_means, pass_stride);
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] *= feature_scale[i];
		math_add_gramian(feature_matrix, DENOISE_FEATURES, features, 1.0f);

		filter_get_feature_variance(pixel_buffer, features, feature_scale, pass_stride);
		for(int i = 0; i < NORM_FEATURE_NUM; i++)
			feature_matrix_norm += features[i + NORM_FEATURE_OFFSET]*kernel_data.integrator.filter_strength;
	} END_FOR_PIXEL_WINDOW

	int rank = math_jacobi_eigendecomposition(feature_matrix, transform, DENOISE_FEATURES, transform_stride);

	float singular_threshold = 0.01f + 2.0f * (sqrtf(feature_matrix_norm) / (sqrtf(rank) * 0.5f));
	singular_threshold *= singular_threshold;

	rank = 0;
	for(int i = 0; i < DENOISE_FEATURES; i++, rank++) {
		float s = feature_matrix[i*DENOISE_FEATURES+i];
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
		storage->singular[i] = feature_matrix[i*DENOISE_FEATURES+i];
	}
#endif
}

ccl_device void kernel_filter_estimate_bandwidths(KernelGlobals *kg, int sample, float ccl_readonly_ptr buffer, int x, int y, float ccl_readonly_ptr transform, CUDAFilterStorage *storage, int4 rect, int transform_stride, int localIdx)
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
	float ccl_readonly_ptr pixel_buffer;
	int3 pixel;
	float ccl_readonly_ptr center_buffer = buffer + (y - rect.y) * buffer_w + (x - rect.x);

	int rank = storage->rank;

	float feature_means[DENOISE_FEATURES];
	filter_get_features(make_int3(x, y, 0), center_buffer, feature_means, NULL, pass_stride);

	/* === Estimate bandwidth for each r-feature dimension. ===
	 * To do so, the second derivative of the pixel color w.r.t. the particular r-feature
	 * has to be estimated. That is done by least-squares-fitting a model that includes
	 * both the r-feature vector z as well as z^T*z and using the resulting parameter for
	 * that dimension of the z^T*z vector times two as the derivative. */
	int matrix_size = 2*rank + 1; /* Constant term (1 dim) + z (rank dims) + z^T*z (rank dims) */
	float XtWX[(2*DENOISE_FEATURES+1)*(2*DENOISE_FEATURES+1)], design_row[2*DENOISE_FEATURES+1];
	float3 XtWy[2*DENOISE_FEATURES+1];

	math_matrix_zero_lower(XtWX, matrix_size);
	math_vec3_zero(XtWy, matrix_size);
	FOR_PIXEL_WINDOW {
		float weight = filter_get_design_row_transform_weight(pixel, pixel_buffer, feature_means, pass_stride, features, rank, design_row, transform, transform_stride, NULL);
	
		if(weight == 0.0f) continue;
		weight /= max(1.0f, filter_get_pixel_variance(pixel_buffer, pass_stride));

		filter_extend_design_row_quadratic(rank, design_row);
		math_add_gramian(XtWX, matrix_size, design_row, weight);
		math_add_vec3(XtWy, matrix_size, design_row, weight * filter_get_pixel_color(pixel_buffer, pass_stride));
	} END_FOR_PIXEL_WINDOW

	math_solve_normal_equation(XtWX, XtWy, matrix_size);

	/* Calculate the inverse of the r-feature bandwidths. */
	for(int i = 0; i < rank; i++)
		storage->bandwidth[i] = sqrtf(2.0f * average(fabs(XtWy[1+rank+i])) + 0.16f);
	for(int i = rank; i < DENOISE_FEATURES; i++)
		storage->bandwidth[i] = 0.0f;
}

ccl_device void kernel_filter_estimate_bias_variance(KernelGlobals *kg, int sample, float ccl_readonly_ptr buffer, int x, int y, float ccl_readonly_ptr transform, CUDAFilterStorage *storage, int4 rect, int candidate, int transform_stride, int localIdx)
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
	float ccl_readonly_ptr pixel_buffer;
	int3 pixel;
	float ccl_readonly_ptr center_buffer = buffer + (y - rect.y) * buffer_w + (x - rect.x);
	float3 center_color  = filter_get_pixel_color(center_buffer, pass_stride);
	float sqrt_center_variance = sqrtf(filter_get_pixel_variance(center_buffer, pass_stride));

	int rank = storage->rank;

	float feature_means[DENOISE_FEATURES];
	filter_get_features(make_int3(x, y, 0), center_buffer, feature_means, NULL, pass_stride);


	float g_bandwidth_factor[DENOISE_FEATURES];
	const float candidate_bw[6] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
	for(int i = 0; i < rank; i++)
		/* Divide by the candidate bandwidth since the bandwidth_factor actually is the inverse of the bandwidth. */
		g_bandwidth_factor[i] = storage->bandwidth[i]/candidate_bw[candidate];

	int matrix_size = rank+1;
	float XtWX[(DENOISE_FEATURES+1)*(DENOISE_FEATURES+1)], design_row[DENOISE_FEATURES+1];
	math_matrix_zero_lower(XtWX, matrix_size);

	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
		float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		float weight = filter_get_design_row_transform_weight(pixel, pixel_buffer, feature_means, pass_stride, features, rank, design_row, transform, transform_stride, g_bandwidth_factor);

		if(weight == 0.0f) continue;
		weight /= max(1.0f, variance);

		math_add_gramian(XtWX, matrix_size, design_row, weight);
	} END_FOR_PIXEL_WINDOW

	math_matrix_add_diagonal(XtWX, matrix_size, 1e-4f); /* Improve the numerical stability. */
	math_cholesky(XtWX, matrix_size);
	math_inverse_lower_tri_inplace(XtWX, matrix_size);

	float r_feature_weight[DENOISE_FEATURES+1];
	math_vector_zero(r_feature_weight, matrix_size);
	for(int col = 0; col < matrix_size; col++)
		for(int row = col; row < matrix_size; row++)
			r_feature_weight[col] += XtWX[row]*XtWX[col*matrix_size+row];

	float3 est_color = make_float3(0.0f, 0.0f, 0.0f), est_pos_color = make_float3(0.0f, 0.0f, 0.0f);
	float est_variance = 0.0f, est_pos_variance = 0.0f;
	float pos_weight = 0.0f;

	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(pixel_buffer, pass_stride);
		float variance = filter_get_pixel_variance(pixel_buffer, pass_stride);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		float weight = filter_get_design_row_transform_weight(pixel, pixel_buffer, feature_means, pass_stride, features, rank, design_row, transform, transform_stride, g_bandwidth_factor);

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

ccl_device void kernel_filter_calculate_bandwidth(KernelGlobals *kg, int sample, CUDAFilterStorage *storage)
{
	const float candidate_bw[6] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
	double bias_XtX = 0.0, bias_XtY = 0.0, var_XtX = 0.0, var_XtY = 0.0;
	for(int g = 0; g < 6; g++) {
		double bias_x = (double) (candidate_bw[g]*candidate_bw[g]);
		bias_XtX += bias_x*bias_x;
		bias_XtY += bias_x * (double) storage->est_bias[g];
		double var_x = 1.0 / (pow(candidate_bw[g], storage->rank) * sample);
		var_XtX += var_x*var_x;
		var_XtY += var_x * (double) storage->est_variance[g];
	}

	/* === Estimate optimal global bandwidth. === */
	double bias_coef = bias_XtY / bias_XtX;
	double variance_coef = var_XtY / var_XtX;
	float optimal_bw = (float) pow((storage->rank * variance_coef) / (4.0 * bias_coef*bias_coef * sample), 1.0 / (storage->rank + 4));
	storage->global_bandwidth = clamp(optimal_bw, 0.05f, 2.0f);
}

CCL_NAMESPACE_END