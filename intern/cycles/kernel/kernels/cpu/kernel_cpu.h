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

/* Templated common declaration part of all CPU kernels. */

void KERNEL_FUNCTION_FULL_NAME(path_trace)(KernelGlobals *kg,
                                           float *buffer,
                                           unsigned int *rng_state,
                                           int sample,
                                           int x, int y,
                                           int offset,
                                           int stride);

void KERNEL_FUNCTION_FULL_NAME(convert_to_byte)(KernelGlobals *kg,
                                                uchar4 *rgba,
                                                float *buffer,
                                                float sample_scale,
                                                int x, int y,
                                                int offset, int stride);

void KERNEL_FUNCTION_FULL_NAME(convert_to_half_float)(KernelGlobals *kg,
                                                      uchar4 *rgba,
                                                      float *buffer,
                                                      float sample_scale,
                                                      int x, int y,
                                                      int offset,
                                                      int stride);

void KERNEL_FUNCTION_FULL_NAME(shader)(KernelGlobals *kg,
                                       uint4 *input,
                                       float4 *output,
                                       float *output_luma,
                                       int type,
                                       int filter,
                                       int i,
                                       int offset,
                                       int sample);

void KERNEL_FUNCTION_FULL_NAME(filter_divide_shadow)(KernelGlobals *kg,
                                                     int sample,
                                                     float** buffers,
                                                     int x,
                                                     int y,
                                                     int *tile_x,
                                                     int *tile_y,
                                                     int *offset,
                                                     int *stride,
                                                     float *unfiltered,
                                                     float *sampleV,
                                                     float *sampleVV,
                                                     float *bufferV,
                                                     int* prefilter_rect);

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
                                                   float *mean,
                                                   float *variance,
                                                   int* prefilter_rect);

void KERNEL_FUNCTION_FULL_NAME(filter_non_local_means)(int x, int y,
                                                       float *noisyImage,
                                                       float *weightImage,
                                                       float *variance,
                                                       float *filteredImage,
                                                       int* rect,
                                                       int r, int f,
                                                       float a, float k_2);

void KERNEL_FUNCTION_FULL_NAME(filter_non_local_means_3)(int x, int y,
                                                         float *noisyImage[3],
                                                         float *weightImage[3],
                                                         float *variance[3],
                                                         float *filteredImage[3],
                                                         int* rect,
                                                         int r, int f,
                                                         float a, float k_2);

void KERNEL_FUNCTION_FULL_NAME(filter_combine_halves)(int x, int y,
                                                      float *mean,
                                                      float *variance,
                                                      float *a,
                                                      float *b,
                                                      int* prefilter_rect,
                                                      int r);

void KERNEL_FUNCTION_FULL_NAME(filter_construct_transform)(KernelGlobals *kg,
                                                           int sample,
                                                           float* buffer,
                                                           int x,
                                                           int y,
                                                           void *storage,
                                                           int* rect);

void KERNEL_FUNCTION_FULL_NAME(filter_estimate_wlr_params)(KernelGlobals *kg,
                                                           int sample,
                                                           float* buffer,
                                                           int x,
                                                           int y,
                                                           void *storage,
                                                           int* rect);

void KERNEL_FUNCTION_FULL_NAME(filter_final_pass_wlr)(KernelGlobals *kg,
                                                  int sample,
                                                  float* buffer,
                                                  int x,
                                                  int y,
                                                  int offset,
                                                  int stride,
                                                  float* buffers,
                                                  void *storage,
                                                  int* filter_area,
                                                  int* rect);

void KERNEL_FUNCTION_FULL_NAME(filter_final_pass_nlm)(KernelGlobals *kg,
                                                  int sample,
                                                  float* buffer,
                                                  int x,
                                                  int y,
                                                  int offset,
                                                  int stride,
                                                  float* buffers,
                                                  void *storage,
                                                  int* filter_area,
                                                  int* rect);

void KERNEL_FUNCTION_FULL_NAME(filter_old_1)(KernelGlobals *kg,
                                             float *denoise_data,
                                             int x, int y,
                                             int samples,
                                             int halfWindow,
                                             float bandwidthFactor,
                                             float *storage,
                                             int* rect);

void KERNEL_FUNCTION_FULL_NAME(filter_old_2)(KernelGlobals *kg,
                                             float* buffer,
                                             float *denoise_data,
                                             int x, int y,
                                             int offset, int stride,
                                             int samples,
                                             int halfWindow,
                                             float bandwidthFactor,
                                             float *storage,
                                             int* rect,
                                             int* tile);

#undef KERNEL_ARCH
