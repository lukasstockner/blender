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

#define FOR_PIXEL_WINDOW for(int py = low.y; py < high.y; py++) { \
                             int ytile = (py < tile_y[1])? 0: ((py < tile_y[2])? 1: 2); \
                                 for(int px = low.x; px < high.x; px++) { \
                                     int xtile = (px < tile_x[1])? 0: ((px < tile_x[2])? 1: 2); \
                                     int tile = ytile*3+xtile; \
                                     buffer = buffers[tile] + (offset[tile] + py*stride[tile] + px)*kernel_data.film.pass_stride + kernel_data.film.pass_denoising;

#define END_FOR_PIXEL_WINDOW }}

#define FEATURE_PASSES 7 /* Normals, Albedo, Depth */

ccl_device_inline void filter_get_features(int x, int y, float *buffer, float sample, float *features, float *mean)
{
	float sample_scale = 1.0f/sample;
	features[0] = buffer[0] * sample_scale;
	features[1] = buffer[1] * sample_scale;
	features[2] = buffer[2] * sample_scale;
	features[3] = buffer[6] * sample_scale;
	features[4] = buffer[7] * sample_scale;
	features[5] = buffer[8] * sample_scale;
	features[6] = buffer[12] * sample_scale;
	features[7] = x;
	features[8] = y;
	if(mean) {
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] -= mean[i];
	}
}

ccl_device_inline void filter_get_feature_variance(int x, int y, float *buffer, float sample, float *features, float *scale)
{
	float sample_scale = 1.0f/sample;
	float sample_scale_var = 1.0f/(sample - 1.0f);
	features[0] = saturate(buffer[3] * sample_scale_var) * sample_scale;
	features[1] = saturate(buffer[4] * sample_scale_var) * sample_scale;
	features[2] = saturate(buffer[5] * sample_scale_var) * sample_scale;
	features[3] = saturate(buffer[9] * sample_scale_var) * sample_scale;
	features[4] = saturate(buffer[10] * sample_scale_var) * sample_scale;
	features[5] = saturate(buffer[11] * sample_scale_var) * sample_scale;
	features[6] = saturate(buffer[13] * sample_scale_var) * sample_scale;
	features[7] = 0.0f;
	features[8] = 0.0f;
	for(int i = 0; i < DENOISE_FEATURES; i++)
		features[i] *= scale[i]*scale[i];
}

ccl_device_inline float3 filter_get_pixel_color(float *buffer, float sample)
{
	float sample_scale = 1.0f/sample;
	return make_float3(buffer[14], buffer[15], buffer[16]) * sample_scale;
}

ccl_device_inline float filter_get_pixel_variance(float *buffer, float sample)
{
	float sample_scale_var = 1.0f/(sample * (sample - 1.0f));
	return average(make_float3(buffer[17], buffer[18], buffer[19])) * sample_scale_var;
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

/* Since the filtering may be performed across tile edged, all the neighboring tiles have to be passed along as well.
 * tile_x/y contain the x/y positions of the tile grid, 4 entries each:
 * - Start of the lower/left neighbor
 * - Start of the own tile
 * - Start of the upper/right neighbor
 * - Start of the next upper/right neighbor (not accessed)
 * buffers contains the nine buffer pointers (y-major ordering, starting with the lower left tile), offset and stride the respective parameters of the tile.
 */
ccl_device void kernel_filter_estimate_params(KernelGlobals *kg, int sample, float **buffers, int x, int y, int *tile_x, int *tile_y, int *offset, int *stride, FilterStorage *storage)
{
	storage += (y - tile_y[1])*(tile_y[2] - tile_y[1]) + (x - tile_x[1]);

	/* Temporary storage, used in different steps of the algorithm. */
	float tempmatrix[(2*DENOISE_FEATURES+1)*(2*DENOISE_FEATURES+1)], tempvector[2*DENOISE_FEATURES+1];
	float *buffer, features[DENOISE_FEATURES];

	/* === Get center pixel color and variance. === */
	float *center_buffer = buffers[4] + (offset[4] + y*stride[4] + x)*kernel_data.film.pass_stride + kernel_data.film.pass_denoising;
	float3 center_color    = make_float3(center_buffer[14], center_buffer[15], center_buffer[16]) / sample;
	float sqrt_center_variance = sqrtf(average(make_float3(center_buffer[17], center_buffer[18], center_buffer[19])) / (sample * (sample - 1.0f)));




	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(tile_x[0], x - kernel_data.integrator.half_window),
	                      max(tile_y[0], y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(tile_x[3], x + kernel_data.integrator.half_window + 1),
	                      min(tile_y[3], y + kernel_data.integrator.half_window + 1));




	/* === Shift feature passes to have mean 0. === */
	float feature_means[DENOISE_FEATURES] = {0.0f};
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, buffer, sample, features, NULL);
		for(int i = 0; i < FEATURE_PASSES; i++)
			feature_means[i] += features[i];
	} END_FOR_PIXEL_WINDOW

	float pixel_scale = 1.0f / ((high.y - low.y) * (high.x - low.x));
	for(int i = 0; i < FEATURE_PASSES; i++)
		feature_means[i] *= pixel_scale;
	feature_means[7] = x;
	feature_means[8] = y;

	/* === Scale the shifted feature passes to a range of [-1; 1], will be baked into the transform later. === */
	float *feature_scale = tempvector;
	math_vector_zero(feature_scale, DENOISE_FEATURES);

	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, buffer, sample, features, feature_means);
		for(int i = 0; i < FEATURE_PASSES; i++)
			feature_scale[i] = max(feature_scale[i], fabsf(features[i]));
	} END_FOR_PIXEL_WINDOW

	for(int i = 0; i < FEATURE_PASSES; i++)
		feature_scale[i] = 1.0f / max(feature_scale[i], 0.01f);
	feature_scale[7] = feature_scale[8] = 1.0f / kernel_data.integrator.half_window;




	/* === Generate the feature transformation. ===
	 * This transformation maps the DENOISE_FEATURES-dimentional feature space to a reduced feature (r-feature) space
	 * which generally has fewer dimensions. This mainly helps to prevent overfitting. */
	float* feature_matrix = tempmatrix, feature_matrix_norm = 0.0f;
	math_matrix_zero_lower(feature_matrix, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW {
		filter_get_features(px, py, buffer, sample, features, feature_means);
		for(int i = 0; i < FEATURE_PASSES; i++)
			features[i] *= feature_scale[i];
		math_add_gramian(feature_matrix, DENOISE_FEATURES, features, 1.0f);

		filter_get_feature_variance(px, py, buffer, sample, features, feature_scale);
		for(int i = 0; i < FEATURE_PASSES; i++)
			feature_matrix_norm += features[i];
	} END_FOR_PIXEL_WINDOW
	math_lower_tri_to_full(feature_matrix, DENOISE_FEATURES);

	float *feature_transform = &storage->transform[0], *singular = tempvector + DENOISE_FEATURES;
	int rank = svd(feature_matrix, feature_transform, singular, DENOISE_FEATURES);
	float singular_threshold = 0.01f + 2.0f * (sqrtf(feature_matrix_norm) / (sqrtf(rank) * 0.5f));

	rank = 0;
	for(int i = 0; i < DENOISE_FEATURES; i++, rank++) {
		float s = sqrtf(singular[i]);
		if(i >= 2 && s < singular_threshold)
			break;
		/* Bake the feature scaling into the transformation matrix. */
		for(int j = 0; j < DENOISE_FEATURES; j++)
			feature_transform[rank*DENOISE_FEATURES + j] *= feature_scale[j];
	}

	/* From here on, the mean of the features will be shifted to the central pixel's values. */
	filter_get_features(x, y, center_buffer, sample, feature_means, NULL);




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
		filter_get_features(px, py, buffer, sample, features, feature_means);
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




	/* === Estimate bias and variance for different candidate global bandwidths. === */
	const float candidate_bw[6] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
	double lsq_bias[LSQ_SIZE], lsq_variance[LSQ_SIZE];
	math_lsq_init(lsq_bias);
	math_lsq_init(lsq_variance);
	for(int g = 0; g < 6; g++) {
		float g_bandwidth_factor[DENOISE_FEATURES];
		for(int i = 0; i < rank; i++)
			g_bandwidth_factor[i] = candidate_bw[g]*bandwidth_factor[i];

		matrix_size = rank+1;
		math_matrix_zero_lower(XtX, matrix_size);

		FOR_PIXEL_WINDOW {
			float3 color = filter_get_pixel_color(buffer, sample);
			float variance = filter_get_pixel_variance(buffer, sample);
			if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

			filter_get_features(px, py, buffer, sample, features, feature_means);
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

			filter_get_features(px, py, buffer, sample, features, feature_means);
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
	double bias_coef = math_lsq_solve(lsq_bias);
	double variance_coef = math_lsq_solve(lsq_variance);
	float optimal_bw = (float) pow((rank * variance_coef) / (4.0 * bias_coef*bias_coef * sample), 1.0 / (rank + 4));




	/* === Store the calculated data for the second kernel. === */
	storage->rank = rank;
	storage->global_bandwidth = optimal_bw;
}




ccl_device void kernel_filter_final_pass(KernelGlobals *kg, int sample, float **buffers, int x, int y, int *tile_x, int *tile_y, int *offset, int *stride, FilterStorage *storage)
{
	storage += (y - tile_y[1])*(tile_y[2] - tile_y[1]) + (x - tile_x[1]);
	float *buffer, features[DENOISE_FEATURES];

	/* === Get center pixel. === */
	float *center_buffer = buffers[4] + (offset[4] + y*stride[4] + x)*kernel_data.film.pass_stride + kernel_data.film.pass_denoising;
	float3 center_color    = make_float3(center_buffer[14], center_buffer[15], center_buffer[16]) / sample;
	float sqrt_center_variance = sqrtf(average(make_float3(center_buffer[17], center_buffer[18], center_buffer[19])) / (sample * (sample - 1.0f)));
	float feature_means[DENOISE_FEATURES];
	filter_get_features(x, y, center_buffer, sample, feature_means, NULL);




	/* === Fetch stored data from the previous kernel. === */
	float *feature_transform = &storage->transform[0];
	float *bandwidth_factor = &storage->bandwidth[0];
	int rank = storage->rank;
	/* Apply a median filter to the 3x3 window aroung the current pixel. */
	int sort_idx = 0;
	float global_bandwidths[9];
	for(int py = max(y-1, tile_y[1]); py < min(y+2, tile_y[2]); py++) {
		for(int px = max(x-1, tile_x[1]); px < min(x+2, tile_x[2]); px++) {
			int ofs = (py-y)*(tile_y[2] - tile_y[1]) + (px-x);
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
		bandwidth_factor[i] *= global_bandwidth;

	int matrix_size = rank+1;
	math_matrix_zero_lower(XtX, matrix_size);

	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(buffer, sample);
		float variance = filter_get_pixel_variance(buffer, sample);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, buffer, sample, features, feature_means);
		float weight = filter_fill_design_row(features, rank, design_row, feature_transform, bandwidth_factor);

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

	float3 final_color = make_float3(0.0f, 0.0f, 0.0f);
	float3 final_pos_color = make_float3(0.0f, 0.0f, 0.0f);
	float pos_weight = 0.0f;
	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(buffer, sample);
		float variance = filter_get_pixel_variance(buffer, sample);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;

		filter_get_features(px, py, buffer, sample, features, feature_means);
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
}

CCL_NAMESPACE_END
