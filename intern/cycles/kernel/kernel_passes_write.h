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

CCL_NAMESPACE_BEGIN

ccl_device_inline void kernel_write_pass_float(ccl_global float *buffer, int sample, float value)
{
	ccl_global float *buf = buffer;
#if defined(__SPLIT_KERNEL__)
	atomic_add_and_fetch_float(buf, value);
#else
	*buf = (sample == 0)? value: *buf + value;
#endif  /* __SPLIT_KERNEL__ */
}

ccl_device_inline void kernel_write_pass_float3(ccl_global float *buffer, int sample, float3 value)
{
#if defined(__SPLIT_KERNEL__)
	ccl_global float *buf_x = buffer + 0;
	ccl_global float *buf_y = buffer + 1;
	ccl_global float *buf_z = buffer + 2;

	atomic_add_and_fetch_float(buf_x, value.x);
	atomic_add_and_fetch_float(buf_y, value.y);
	atomic_add_and_fetch_float(buf_z, value.z);
#else
	ccl_global float3 *buf = (ccl_global float3*)buffer;
	*buf = (sample == 0)? value: *buf + value;
#endif  /* __SPLIT_KERNEL__ */
}

ccl_device_inline void kernel_write_pass_float4(ccl_global float *buffer, int sample, float4 value)
{
#if defined(__SPLIT_KERNEL__)
	ccl_global float *buf_x = buffer + 0;
	ccl_global float *buf_y = buffer + 1;
	ccl_global float *buf_z = buffer + 2;
	ccl_global float *buf_w = buffer + 3;

	atomic_add_and_fetch_float(buf_x, value.x);
	atomic_add_and_fetch_float(buf_y, value.y);
	atomic_add_and_fetch_float(buf_z, value.z);
	atomic_add_and_fetch_float(buf_w, value.w);
#else
	ccl_global float4 *buf = (ccl_global float4*)buffer;
	*buf = (sample == 0)? value: *buf + value;
#endif  /* __SPLIT_KERNEL__ */
}

#ifdef __DENOISING_FEATURES__
ccl_device_inline void kernel_write_pass_float_variance(ccl_global float *buffer, int sample, float value)
{
	kernel_write_pass_float(buffer, sample, value);

	/* The online one-pass variance update that's used for the megakernel can't easily be implemented
	 * with atomics, so for the split kernel the E[x^2] - 1/N * (E[x])^2 fallback is used. */
#  ifdef __SPLIT_KERNEL__
	kernel_write_pass_float(buffer+1, sample, value*value);
#  else
	if(sample == 0) {
		kernel_write_pass_float(buffer+1, sample, 0.0f);
	}
	else {
		float new_mean = buffer[0] * (1.0f / (sample + 1));
		float old_mean = (buffer[0] - value) * (1.0f / sample);
		kernel_write_pass_float(buffer+1, sample, (value - new_mean) * (value - old_mean));
	}
#  endif
}

#  if defined(__SPLIT_KERNEL__)
#    define kernel_write_pass_float3_unaligned kernel_write_pass_float3
#  else
ccl_device_inline void kernel_write_pass_float3_unaligned(ccl_global float *buffer, int sample, float3 value)
{
	buffer[0] = (sample == 0)? value.x: buffer[0] + value.x;
	buffer[1] = (sample == 0)? value.y: buffer[1] + value.y;
	buffer[2] = (sample == 0)? value.z: buffer[2] + value.z;
}
#  endif

ccl_device_inline void kernel_write_pass_float3_variance(ccl_global float *buffer, int sample, float3 value)
{
	kernel_write_pass_float3_unaligned(buffer, sample, value);
#  ifdef __SPLIT_KERNEL__
	kernel_write_pass_float3_unaligned(buffer+3, sample, value*value);
#  else
	if(sample == 0) {
		kernel_write_pass_float3_unaligned(buffer+3, sample, make_float3(0.0f, 0.0f, 0.0f));
	}
	else {
		float3 sum = make_float3(buffer[0], buffer[1], buffer[2]);
		float3 new_mean = sum * (1.0f / (sample + 1));
		float3 old_mean = (sum - value) * (1.0f / sample);
		kernel_write_pass_float3_unaligned(buffer+3, sample, (value - new_mean) * (value - old_mean));
	}
#  endif
}
#endif /* __DENOISING_FEATURES__ */

CCL_NAMESPACE_END