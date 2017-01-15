/*
 * Copyright 2011-2013 Blender Foundation
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

/* Templated common implementation part of all CPU kernels.
 *
 * The idea is that particular .cpp files sets needed optimization flags and
 * simply includes this file without worry of copying actual implementation over.
 */

#include "kernel_compat_cpu.h"
#include "kernel_math.h"
#include "kernel_types.h"
#include "kernel_globals.h"
#include "kernel_cpu_image.h"
#include "kernel_film.h"
#include "kernel_path.h"
#include "kernel_path_branched.h"
#include "kernel_bake.h"

#include "filter/filter.h"

#ifdef KERNEL_STUB
#  include "util_debug.h"
#  define STUB_ASSERT(arch, name) assert(!(#name " kernel stub for architecture " #arch " was called!"))
#endif

CCL_NAMESPACE_BEGIN


/* Path Tracing */

void KERNEL_FUNCTION_FULL_NAME(path_trace)(KernelGlobals *kg,
                                           float *buffer,
                                           unsigned int *rng_state,
                                           int sample,
                                           int x, int y,
                                           int offset,
                                           int stride)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, path_trace);
#else
#  ifdef __BRANCHED_PATH__
	if(kernel_data.integrator.branched) {
		kernel_branched_path_trace(kg,
		                           buffer,
		                           rng_state,
		                           sample,
		                           x, y,
		                           offset,
		                           stride);
	}
	else
#  endif
	{
		kernel_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
	}
#endif /* KERNEL_STUB */
}

/* Film */

void KERNEL_FUNCTION_FULL_NAME(convert_to_byte)(KernelGlobals *kg,
                                                uchar4 *rgba,
                                                float *buffer,
                                                float sample_scale,
                                                int x, int y,
                                                int offset,
                                                int stride)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, convert_to_byte);
#else
	kernel_film_convert_to_byte(kg,
	                            rgba,
	                            buffer,
	                            sample_scale,
	                            x, y,
	                            offset,
	                            stride);
#endif /* KERNEL_STUB */
}

void KERNEL_FUNCTION_FULL_NAME(convert_to_half_float)(KernelGlobals *kg,
                                                      uchar4 *rgba,
                                                      float *buffer,
                                                      float sample_scale,
                                                      int x, int y,
                                                      int offset,
                                                      int stride)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, convert_to_half_float);
#else
	kernel_film_convert_to_half_float(kg,
	                                  rgba,
	                                  buffer,
	                                  sample_scale,
	                                  x, y,
	                                  offset,
	                                  stride);
#endif /* KERNEL_STUB */
}

/* Shader Evaluate */

void KERNEL_FUNCTION_FULL_NAME(shader)(KernelGlobals *kg,
                                       uint4 *input,
                                       float4 *output,
                                       float *output_luma,
                                       int type,
                                       int filter,
                                       int i,
                                       int offset,
                                       int sample)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, shader);
#else
	if(type >= SHADER_EVAL_BAKE) {
		kernel_assert(output_luma == NULL);
#  ifdef __BAKING__
		kernel_bake_evaluate(kg,
		                     input,
		                     output,
		                     (ShaderEvalType)type,
		                     filter,
		                     i,
		                     offset,
		                     sample);
#  endif
	}
	else {
		kernel_shader_evaluate(kg,
		                       input,
		                       output,
		                       output_luma,
		                       (ShaderEvalType)type,
		                       i,
		                       sample);
	}
#endif /* KERNEL_STUB */
}

/* Denoise filter */

void KERNEL_FUNCTION_FULL_NAME(filter_divide_shadow)(KernelGlobals *kg,
                                                     int sample,
                                                     float** buffers,
                                                     int x,
                                                     int y,
                                                     int *tile_x,
                                                     int *tile_y,
                                                     int *offset,
                                                     int *stride,
                                                     float *unfiltered, float *sampleVariance, float *sampleVarianceV, float *bufferVariance,
                                                     int* prefilter_rect)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_divide_shadow);
#else
	kernel_filter_divide_shadow(kg, sample, buffers, x, y, tile_x, tile_y, offset, stride, unfiltered, sampleVariance, sampleVarianceV, bufferVariance, load_int4(prefilter_rect));
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_get_feature)(KernelGlobals *kg,
                                                   int sample,
                                                   float** buffers,
                                                   int m_offset,
                                                   int v_offset,
                                                   int x,
                                                   int y,
                                                   int *tile_x,
                                                   int *tile_y,
                                                   int *offset,
                                                   int *stride,
                                                   float *mean, float *variance,
                                                   int* prefilter_rect)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_get_feature);
#else
	kernel_filter_get_feature(kg, sample, buffers, m_offset, v_offset, x, y, tile_x, tile_y, offset, stride, mean, variance, load_int4(prefilter_rect));
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_combine_halves)(int x, int y,
                                                      float *mean,
                                                      float *variance,
                                                      float *a,
                                                      float *b,
                                                      int* prefilter_rect,
                                                      int r)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_combine_halves);
#else
	kernel_filter_combine_halves(x, y, mean, variance, a, b, load_int4(prefilter_rect), r);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_construct_transform)(KernelGlobals *kg,
                                                           int sample,
                                                           float* buffer,
                                                           int x,
                                                           int y,
                                                           void *storage,
                                                           int* prefilter_rect)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_construct_transform);
#else
	kernel_filter_construct_transform(kg, sample, buffer, x, y, (FilterStorage*) storage, load_int4(prefilter_rect));
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_divide_combined)(KernelGlobals *kg,
                                                       int x, int y,
                                                       int sample,
                                                       float *buffers,
                                                       int offset,
                                                       int stride)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_divide_combined);
#else
	kernel_filter_divide_combined(kg, x, y, sample, buffers, offset, stride);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_calc_difference)(int dx,
                                                           int dy,
                                                           float *weightImage,
                                                           float *variance,
                                                           float *differenceImage,
                                                           int *rect,
                                                           int w,
                                                           int channel_offset,
                                                           float a,
                                                           float k_2)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_calc_difference);
#else
	kernel_filter_nlm_calc_difference(dx, dy, weightImage, variance, differenceImage, load_int4(rect), w, channel_offset, a, k_2);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_blur)(float *differenceImage,
                                                float *outImage,
                                                int *rect,
                                                int w,
                                                int f)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_blur);
#else
	kernel_filter_nlm_blur(differenceImage, outImage, load_int4(rect), w, f);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_calc_weight)(float *differenceImage,
                                                       float *outImage,
                                                       int *rect,
                                                       int w,
                                                       int f)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_calc_weight);
#else
	kernel_filter_nlm_calc_weight(differenceImage, outImage, load_int4(rect), w, f);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_update_output)(int dx,
                                                         int dy,
                                                         float *differenceImage,
                                                         float *image,
                                                         float *outImage,
                                                         float *accumImage,
                                                         int *rect,
                                                         int w,
                                                         int f)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_update_output);
#else
	kernel_filter_nlm_update_output(dx, dy, differenceImage, image, outImage, accumImage, load_int4(rect), w, f);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_construct_gramian)(int dx,
                                                             int dy,
                                                             float *differenceImage,
                                                             float *buffer,
                                                             int color_pass,
                                                             void *storage,
                                                             float *XtWX,
                                                             float3 *XtWY,
                                                             int *rect,
                                                             int *filter_rect,
                                                             int w,
                                                             int h,
                                                             int f)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_construct_gramian);
#else
    kernel_filter_nlm_construct_gramian(dx, dy, differenceImage, buffer, color_pass, (FilterStorage*) storage, XtWX, XtWY, load_int4(rect), load_int4(filter_rect), w, h, f);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_normalize)(float *outImage,
                                                     float *accumImage,
                                                     int *rect,
                                                     int w)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_normalize);
#else
	kernel_filter_nlm_normalize(outImage, accumImage, load_int4(rect), w);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_finalize)(int x,
                                                int y,
                                                int storage_ofs,
                                                int w,
                                                int h,
                                                float *buffer,
                                                void *storage,
                                                float *XtWX,
                                                float3 *XtWY,
                                                int *buffer_params,
                                                int sample)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_finalize);
#else
    kernel_filter_finalize(x, y, storage_ofs, 0, w, h, buffer, (FilterStorage*) storage, XtWX, XtWY, load_int4(buffer_params), sample);
#endif
}

#undef KERNEL_STUB
#undef STUB_ASSERT
#undef KERNEL_ARCH

CCL_NAMESPACE_END
