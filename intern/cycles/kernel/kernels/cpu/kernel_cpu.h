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

void KERNEL_FUNCTION_FULL_NAME(filter1_pixel)(KernelGlobals *kg, float *buffers, int x, int y, int w, int h, int samples, int halfWindow, float biasWeight, float *storage);
void KERNEL_FUNCTION_FULL_NAME(filter2_pixel)(KernelGlobals *kg, float *buffers, int x, int y, int w, int h, int samples, int halfWindow, float biasWeight, float *storage, int4 tile);

#undef KERNEL_ARCH
