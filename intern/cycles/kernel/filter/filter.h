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

#include "util_atomic.h"
#include "util_math_matrix.h"

#include "filter_features.h"
#ifdef __KERNEL_SSE3__
#  include "filter_features_sse.h"
#endif // __KERNEL_SSE3__

#ifdef __KERNEL_CPU__
#include "filter_nlm_cpu.h"
#else
#include "filter_nlm_gpu.h"
#endif

#include "filter_prefilter.h"

/* Not all features are included in the matrix norm. */
#define NORM_FEATURE_OFFSET 3
#define NORM_FEATURE_NUM 8

/* Large enough for a half-window of 10. */
#define CUDA_WEIGHT_CACHE_SIZE 441

#ifdef __KERNEL_CUDA__

#  include "filter_wlr_cuda.h"

/* Define the reconstruction function. */
#  undef  WEIGHT_CACHING_CPU
#  define WEIGHT_CACHING_CUDA
#  define OUTPUT_RENDERBUFFER
#  include "filter_final_pass_impl.h"

#else

#  ifdef __KERNEL_SSE3__
#    include "filter_wlr_sse.h"
#  else
#    include "filter_wlr.h"
#  endif // __KERNEL_SSE3__

/* Define the reconstruction function. */
#  define WEIGHT_CACHING_CPU
#  undef  WEIGHT_CACHING_CUDA
#  define OUTPUT_RENDERBUFFER
#  include "filter_final_pass_impl.h"

#endif // __KERNEL_CUDA__

CCL_NAMESPACE_BEGIN

ccl_device void kernel_filter_divide_combined(KernelGlobals *kg, int x, int y, int sample, float *buffers, int offset, int stride)
{
	float *combined_buffer = buffers + (offset + y*stride + x)*kernel_data.film.pass_stride;
	float fac = sample / combined_buffer[3];
	combined_buffer[0] *= fac;
	combined_buffer[1] *= fac;
	combined_buffer[2] *= fac;
	combined_buffer[3] *= fac;
	if(kernel_data.film.pass_no_denoising) {
		combined_buffer[0] += combined_buffer[kernel_data.film.pass_no_denoising+0];
		combined_buffer[1] += combined_buffer[kernel_data.film.pass_no_denoising+1];
		combined_buffer[2] += combined_buffer[kernel_data.film.pass_no_denoising+2];
	}
}

CCL_NAMESPACE_END