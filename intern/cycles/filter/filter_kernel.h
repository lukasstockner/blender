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

#include "util_color.h"
#include "util_math.h"
#include "util_math_fast.h"
#include "util_texture.h"

#include "util_atomic.h"
#include "util_math_matrix.h"

#include "filter_defines.h"

#include "filter_features.h"
#ifdef __KERNEL_SSE3__
#  include "filter_features_sse.h"
#endif

#include "filter_prefilter.h"

#ifdef __KERNEL_GPU__
#  include "filter_transform_gpu.h"
#else
#  ifdef __KERNEL_SSE3__
#    include "filter_transform_sse.h"
#  else
#    include "filter_transform.h"
#  endif
#endif

#include "filter_reconstruction.h"

#ifdef __KERNEL_CPU__
#include "filter_nlm_cpu.h"
#else
#include "filter_nlm_gpu.h"
#endif

CCL_NAMESPACE_BEGIN

ccl_device void kernel_filter_divide_combined(int x, int y, int sample, ccl_global float *buffers, int offset, int stride, int pass_stride, int no_denoising_offset)
{
	ccl_global float *combined_buffer = buffers + (offset + y*stride + x);
	float fac = sample / combined_buffer[3];
	combined_buffer[0] *= fac;
	combined_buffer[1] *= fac;
	combined_buffer[2] *= fac;
	combined_buffer[3] *= fac;
	if(no_denoising_offset) {
		combined_buffer[0] += combined_buffer[no_denoising_offset+0];
		combined_buffer[1] += combined_buffer[no_denoising_offset+1];
		combined_buffer[2] += combined_buffer[no_denoising_offset+2];
	}
}

CCL_NAMESPACE_END
