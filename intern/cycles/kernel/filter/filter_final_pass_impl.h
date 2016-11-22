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

#ifdef __KERNEL_CUDA__
#define STORAGE_TYPE CUDAFilterStorage
#else
#define STORAGE_TYPE FilterStorage
#endif

ccl_device void FUNCTION_NAME(KernelGlobals *kg, int sample, float ccl_readonly_ptr buffer, int x, int y, int offset, int stride, float *buffers, int filtered_passes, int2 color_passes, STORAGE_TYPE *storage, float *weight_cache, float ccl_readonly_ptr transform, int transform_stride, int4 filter_area, int4 rect)
{
	int buffer_w = align_up(rect.z - rect.x, 4);
	int buffer_h = (rect.w - rect.y);
	int pass_stride = buffer_h * buffer_w * kernel_data.film.num_frames;
	color_passes *= pass_stride;
	int num_frames = kernel_data.film.num_frames;
	int prev_frames = kernel_data.film.prev_frames;

	int2 low  = make_int2(max(rect.x, x - kernel_data.integrator.half_window),
	                      max(rect.y, y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(rect.z, x + kernel_data.integrator.half_window + 1),
	                      min(rect.w, y + kernel_data.integrator.half_window + 1));

	float ccl_readonly_ptr pixel_buffer;
	float ccl_readonly_ptr center_buffer = buffer + (y - rect.y) * buffer_w + (x - rect.x);
	int3 pixel;

	float3 center_color  = filter_get_pixel_color(center_buffer + color_passes.x, pass_stride);
	float sqrt_center_variance = sqrtf(filter_get_pixel_variance(center_buffer + color_passes.x, pass_stride));

	/* NFOR weighting directly writes to the design row, so it doesn't need the feature vector and always uses full rank. */
#ifndef WEIGHTING_NFOR
#  ifdef __KERNEL_CUDA__
	/* On GPUs, store the feature vector in shared memory for faster access. */
	__shared__ float shared_features[DENOISE_FEATURES*CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH];
	float *features = shared_features + DENOISE_FEATURES*(threadIdx.y*blockDim.x + threadIdx.x);
#  else
	float features[DENOISE_FEATURES];
#  endif
	const int rank = storage->rank;
	const int matrix_size = rank+1;
#else
	const int matrix_size = DENOISE_FEATURES;
	float *feature_scales = transform;
#endif

	float feature_means[DENOISE_FEATURES];
	filter_get_features(make_int3(x, y, 0), center_buffer, feature_means, NULL, pass_stride);

#ifdef WEIGHTING_WLR
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
	float inv_global_bandwidth = 1.0f / (global_bandwidths[sort_idx/2] * kernel_data.integrator.weighting_adjust);

	float bandwidth_factor[DENOISE_FEATURES];
	for(int i = 0; i < rank; i++) {
		/* Same as above, divide by the bandwidth since the bandwidth_factor actually is the inverse of the bandwidth. */
		bandwidth_factor[i] = storage->bandwidth[i] * inv_global_bandwidth;
	}
#endif

	/* Essentially, this function is just a first-order regression solver.
	 * We model the pixel color as a linear function of the feature vectors.
	 * So, we search the parameters S that minimize W*(X*S - y), where:
	 * - X is the design matrix containing all the feature vectors
	 * - y is the vector containing all the pixel colors
	 * - W is the diagonal matrix containing all pixel weights
	 * Since this is just regular least-squares, the solution is given by:
	 * S = inv(Xt*W*X)*Xt*W*y */

	float XtWX[(DENOISE_FEATURES+1)*(DENOISE_FEATURES+1)], design_row[DENOISE_FEATURES+1];
	float3 solution[(DENOISE_FEATURES+1)];

	math_matrix_zero(XtWX, matrix_size);
	math_vec3_zero(solution, matrix_size);
	/* Construct Xt*W*X matrix (and fill weight cache, if used). */
	FOR_PIXEL_WINDOW {
		float3 color = filter_get_pixel_color(pixel_buffer + color_passes.x, pass_stride);
		float variance = filter_get_pixel_variance(pixel_buffer + color_passes.x, pass_stride);
		if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) {
#ifdef WEIGHT_CACHING_CUDA
			if(cache_idx < CUDA_WEIGHT_CACHE_SIZE) weight_cache[cache_idx] = 0.0f;
#elif defined(WEIGHT_CACHING_CPU)
			weight_cache[cache_idx] = 0.0f;
#endif
			continue;
		}

#ifdef WEIGHTING_WLR
		float weight = filter_get_design_row_transform_weight(pixel, pixel_buffer, feature_means, pass_stride, features, rank, design_row, transform, transform_stride, bandwidth_factor);
#elif defined(WEIGHTING_NLM)
		filter_get_design_row_transform(pixel, pixel_buffer, feature_means, pass_stride, features, rank, design_row, transform, transform_stride);
		float weight = nlm_weight(x, y, pixel.x, pixel.y, center_buffer + color_passes.y, pixel_buffer + color_passes.y, pass_stride, 1.0f, kernel_data.integrator.weighting_adjust, 4, rect);
#else /* WEIGHTING_NFOR */
		filter_get_design_row(pixel, pixel_buffer, feature_means, feature_scales, pass_stride, design_row);
		float weight = nlm_weight(x, y, pixel.x, pixel.y, center_buffer + color_passes.y, pixel_buffer + color_passes.y, pass_stride, 1.0f, kernel_data.integrator.weighting_adjust, 4, rect);
#endif
		if(weight < 1e-5f) {
#ifdef WEIGHT_CACHING_CUDA
			if(cache_idx < CUDA_WEIGHT_CACHE_SIZE) weight_cache[cache_idx] = 0.0f;
#elif defined(WEIGHT_CACHING_CPU)
			weight_cache[cache_idx] = 0.0f;
#endif
			continue;
		}
		weight /= max(1.0f, variance);
		weight_cache[cache_idx] = weight;

		math_add_gramian(XtWX, matrix_size, design_row, weight);
		math_add_vec3(solution, matrix_size, design_row, weight * color);
	} END_FOR_PIXEL_WINDOW

	/* Solve S = inv(Xt*W*X)*Xt*W*y.
	 * Instead of explicitly inverting Xt*W*X, we rearrange to:
	 * (Xt*W*X)*S = Wt*W*y
	 * Xt*W*X is per definition symmetric positive-semidefinite, so we can apply Cholesky decomposition to find a lower triangular L so that L*Lt = Xt*W*X.
	 * With, that we get (L*Lt)*S = L*(Lt*S) = L*b = Wt*W*y.
	 * Since L is lower triangular, finding b (=Lt*S) is relatively easy.
	 * Then, the remaining problem is Lt*S = b, which also can be solved easily. */
	math_matrix_add_diagonal(XtWX, matrix_size, 1e-4f); /* Improve the numerical stability. */
	math_cholesky(XtWX, matrix_size); /* Find L so that L*Lt = Xt*W*X. */
	math_substitute_forward_vec3(XtWX, matrix_size, solution); /* Solve L*b = X^T*y, replacing X^T*y by b. */
	math_substitute_back_vec3(XtWX, matrix_size, solution); /* Solve L^T*S = b, replacing b by S. */

	if(kernel_data.integrator.use_gradients) {
		FOR_PIXEL_WINDOW {
			float weight;
			float3 color;
#if defined(WEIGHTING_CACHING_CPU) || defined(WEIGHTING_CACHING_CUDA)
#  ifdef WEIGHTING_CACHING_CUDA
			if(cache_idx < CUDA_WEIGHT_CACHE_SIZE)
#  endif
			{
				weight = weight_cache[cache_idx];
				if(weight == 0.0f) continue;
				color = filter_get_pixel_color(pixel_buffer + color_passes.x, pass_stride);
#  ifdef WEIGHTING_NFOR
				filter_get_design_row(pixel, pixel_buffer, feature_means, feature_scales, pass_stride, design_row);
#  else
				filter_get_design_row_transform(pixel, pixel_buffer, feature_means, pass_stride, features, rank, design_row, transform, transform_stride);
#  endif
			}
#  ifdef WEIGHTING_CACHING_CUDA
			else
#  endif
#endif
#ifndef WEIGHTING_CACHING_CPU
			{
				color = filter_get_pixel_color(pixel_buffer + color_passes.x, pass_stride);
				float variance = filter_get_pixel_variance(pixel_buffer + color_passes.x, pass_stride);
				if(filter_firefly_rejection(color, variance, center_color, sqrt_center_variance)) continue;
#  ifdef WEIGHTING_WLR
				weight = filter_get_design_row_transform_weight(pixel, pixel_buffer, feature_means, pass_stride, features, rank, design_row, transform, transform_stride, bandwidth_factor);
#  elif defined(WEIGHTING_NLM)
				filter_get_design_row_transform(pixel, pixel_buffer, feature_means, pass_stride, features, rank, design_row, transform, transform_stride);
				weight = nlm_weight(x, y, pixel.x, pixel.y, center_buffer + color_passes.y, pixel_buffer + color_passes.y, pass_stride, 1.0f, kernel_data.integrator.weighting_adjust, 4, rect);
#  else /* WEIGHTING_NFOR */
				filter_get_design_row(pixel, pixel_buffer, feature_means, feature_scales, pass_stride, design_row);
				weight = nlm_weight(x, y, pixel.x, pixel.y, center_buffer + color_passes.y, pixel_buffer + color_passes.y, pass_stride, 1.0f, kernel_data.integrator.weighting_adjust, 4, rect);
#  endif
				if(weight == 0.0f) continue;
				weight /= max(1.0f, variance);
			}
#endif

			float3 reconstruction = math_dot_vec3(design_row, solution, matrix_size);
#ifdef OUTPUT_RENDERBUFFER
			if(pixel.y >= filter_area.y && pixel.y < filter_area.y+filter_area.w && pixel.x >= filter_area.x && pixel.x < filter_area.x+filter_area.z) {
				float *combined_buffer = buffers + (offset + pixel.y*stride + pixel.x)*kernel_data.film.pass_stride;
				atomic_add_and_fetch_float(combined_buffer + 0, weight*reconstruction.x);
				atomic_add_and_fetch_float(combined_buffer + 1, weight*reconstruction.y);
				atomic_add_and_fetch_float(combined_buffer + 2, weight*reconstruction.z);
				atomic_add_and_fetch_float(combined_buffer + 3, weight);
			}
#elif defined(OUTPUT_DENOISEBUFFER)
			float *filtered_buffer = ((float*) pixel_buffer) + filtered_passes*pass_stride;
			atomic_add_and_fetch_float(filtered_buffer + 0*pass_stride, weight*reconstruction.x);
			atomic_add_and_fetch_float(filtered_buffer + 1*pass_stride, weight*reconstruction.y);
			atomic_add_and_fetch_float(filtered_buffer + 2*pass_stride, weight*reconstruction.z);
			atomic_add_and_fetch_float(filtered_buffer + 3*pass_stride, weight);
#endif
			
		} END_FOR_PIXEL_WINDOW
	}
	else {
		float3 final_color = solution[0];
#ifdef OUTPUT_RENDERBUFFER
		float *combined_buffer = buffers + (offset + y*stride + x)*kernel_data.film.pass_stride;
		final_color *= sample;
		if(kernel_data.film.pass_no_denoising) {
			final_color.x += combined_buffer[kernel_data.film.pass_no_denoising+0];
			final_color.y += combined_buffer[kernel_data.film.pass_no_denoising+1];
			final_color.z += combined_buffer[kernel_data.film.pass_no_denoising+2];
		}
		combined_buffer[0] = final_color.x;
		combined_buffer[1] = final_color.y;
		combined_buffer[2] = final_color.z;
#elif defined(OUTPUT_DENOISEBUFFER)
		float *filtered_buffer = ((float*) center_buffer) + filtered_passes*pass_stride;
		filtered_buffer[0*pass_stride] = final_color.x;
		filtered_buffer[1*pass_stride] = final_color.y;
		filtered_buffer[2*pass_stride] = final_color.z;
		filtered_buffer[3*pass_stride] = 1.0f;
#endif
	}
}

#undef STORAGE_TYPE
#undef FUNCTION_NAME

CCL_NAMESPACE_END