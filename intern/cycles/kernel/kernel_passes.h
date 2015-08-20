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

#include "util_atomic.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void kernel_write_pass_float(ccl_global float *buffer, int sample, float value)
{
#ifdef __KERNEL_GPU__
	ccl_global float *buf = buffer;
	if(sample)
		atomic_add_float(buf, value);
	else
		*buf = value;
#else
	if(sample)
		*buffer += value;
	else
		*buffer = value;
#endif
}

ccl_device_inline float kernel_increment_pass_float(ccl_global float *buffer, int sample)
{
#ifdef __KERNEL_GPU__
	ccl_global float *buf = buffer;
	if(sample)
		return atomic_add_float(buf, 1.0f);
	else {
		*buf = 1.0f;
		return 0.0f;
	}
#else
	if(sample) {
		float v = *buffer;
		*buffer += 1.0f;
		return v;
	} else {
		*buffer = 1.0f;
		return 0.0f;
	}
#endif
}

ccl_device_inline void kernel_write_pass_float3(ccl_global float *buffer, int sample, float3 value)
{
#ifdef __KERNEL_GPU__
	ccl_global float *buf_x = buffer + 0;
	ccl_global float *buf_y = buffer + 1;
	ccl_global float *buf_z = buffer + 2;

	if(sample) {
		atomic_add_float(buf_x, value.x);
		atomic_add_float(buf_y, value.y);
		atomic_add_float(buf_z, value.z);
	} else {
		*buf_x = value.x;
		*buf_y = value.y;
		*buf_z = value.z;
	}
#else
	float3 *buf = (float3*) buffer;
	if(sample)
		*buf += value;
	else
		*buf = value;
#endif
}

ccl_device_inline void kernel_write_pass_float4(ccl_global float *buffer, int sample, float4 value)
{
#ifdef __KERNEL_GPU__
	ccl_global float *buf_x = buffer + 0;
	ccl_global float *buf_y = buffer + 1;
	ccl_global float *buf_z = buffer + 2;
	ccl_global float *buf_w = buffer + 3;

	if(sample) {
		atomic_add_float(buf_x, value.x);
		atomic_add_float(buf_y, value.y);
		atomic_add_float(buf_z, value.z);
		atomic_add_float(buf_w, value.w);
	} else {
		*buf_x = value.x;
		*buf_y = value.y;
		*buf_z = value.z;
		*buf_w = value.w;
	}
#else
	float4 *buf = (float4*) buffer;
	if(sample)
		*buf += value;
	else
		*buf = value;
#endif
}

ccl_device_inline void kernel_write_data_passes(KernelGlobals *kg, ccl_global float *buffer, PathRadiance *L,
	ShaderData *sd, int sample, ccl_addr_space PathState *state, float3 throughput)
{
#ifdef __PASSES__
	int path_flag = state->flag;

	int flag = kernel_data.film.pass_flag;

	if(!(flag & PASS_ALL))
		return;

	if(!(path_flag & PATH_RAY_SINGLE_PASS_DONE) && !(path_flag & PATH_RAY_SINGULAR)) {
		if(flag & PASS_DEPTH)
			kernel_write_pass_float(buffer + kernel_data.film.pass_depth, sample, L->depth);
		if(flag & PASS_OBJECT_ID)
			kernel_write_pass_float(buffer + kernel_data.film.pass_object_id, sample, L->depth*L->depth);
		if(flag & PASS_NORMAL)
			kernel_write_pass_float3(buffer + kernel_data.film.pass_normal, sample, L->normal);
		if(flag & PASS_UV)
			kernel_write_pass_float3(buffer + kernel_data.film.pass_uv, sample, L->normal*L->normal);
		if(flag & PASS_EMISSION)
			kernel_write_pass_float3(buffer + kernel_data.film.pass_emission, sample, L->color);
		if(flag & PASS_BACKGROUND)
			kernel_write_pass_float3(buffer + kernel_data.film.pass_background, sample, L->color*L->color);
		state->flag |= PATH_RAY_SINGLE_PASS_DONE;
	}

	if(sd) {
		L->depth += ccl_fetch(sd, ray_length);
		L->normal = ccl_fetch(sd, N);
		L->color = shader_bsdf_diffuse(kg, sd) + shader_bsdf_glossy(kg, sd) + shader_bsdf_subsurface(kg, sd) + shader_bsdf_transmission(kg, sd);
	}
#endif
}

ccl_device_inline void kernel_write_light_passes(KernelGlobals *kg, ccl_global float *buffer, PathRadiance *L, int sample)
{
#ifdef __PASSES__
	int flag = kernel_data.film.pass_flag;

	if(!kernel_data.film.use_light_pass)
		return;
	
	if(flag & PASS_DIFFUSE_INDIRECT)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_diffuse_indirect, sample, L->indirect_diffuse);
	if(flag & PASS_GLOSSY_INDIRECT)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_glossy_indirect, sample, L->indirect_glossy);
	if(flag & PASS_TRANSMISSION_INDIRECT)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_transmission_indirect, sample, L->indirect_transmission);
	if(flag & PASS_SUBSURFACE_INDIRECT)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_subsurface_indirect, sample, L->indirect_subsurface);
	if(flag & PASS_DIFFUSE_DIRECT)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_diffuse_direct, sample, L->direct_diffuse);
	if(flag & PASS_GLOSSY_DIRECT)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_glossy_direct, sample, L->direct_glossy);
	if(flag & PASS_TRANSMISSION_DIRECT)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_transmission_direct, sample, L->direct_transmission);
	if(flag & PASS_SUBSURFACE_DIRECT)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_subsurface_direct, sample, L->direct_subsurface);

	if(flag & PASS_DIFFUSE_COLOR)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_diffuse_color, sample, L->color_diffuse);
	if(flag & PASS_GLOSSY_COLOR)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_glossy_color, sample, L->color_glossy);
	if(flag & PASS_TRANSMISSION_COLOR)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_transmission_color, sample, L->color_transmission);
	if(flag & PASS_SUBSURFACE_COLOR)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_subsurface_color, sample, L->color_subsurface);
	if(flag & PASS_SHADOW) {
		float4 shadow = L->shadow;
		shadow.w = kernel_data.film.pass_shadow_scale;
		kernel_write_pass_float4(buffer + kernel_data.film.pass_shadow, sample, shadow);
	}
#endif
}

CCL_NAMESPACE_END

