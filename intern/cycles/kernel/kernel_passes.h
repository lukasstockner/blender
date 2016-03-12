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

ccl_device_inline float kernel_add_pass_float(ccl_global float *buffer, int sample, float value)
{
#ifdef __KERNEL_GPU__
	ccl_global float *buf = buffer;
	if(sample)
		return atomic_add_float(buf, value);
	else {
		*buf = value;
		return 0.0f;
	}
#else
	if(sample) {
		float v = *buffer;
		*buffer += value;
		return v;
	} else {
		*buffer = value;
		return 0.0f;
	}
#endif
}

ccl_device_inline void kernel_write_lwr_float(ccl_global float *buffer, float sample, float value) {
	float old = kernel_add_pass_float(buffer, sample, value);
	float var = (sample > 0.0f)? (value - (old / sample)) * (value - ((old + value) / (sample + 1))): 0.0f;
	kernel_add_pass_float(buffer + 1, sample, var);
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

ccl_device_inline void kernel_write_lwr_float3(ccl_global float *buffer, float sample, float3 value, int copy = 0) {
	float old = kernel_add_pass_float(buffer, sample, value.x);
	float var = (sample > 0.0f)? (value.x - (old / sample)) * (value.x - ((old + value.x) / (sample + 1))): 0.0f;
	kernel_add_pass_float(buffer + 3, sample, var);
	if(copy) buffer[copy] = (old + value.x) / (sample + 1);

	old = kernel_add_pass_float(buffer+1, sample, value.y);
	var = (sample > 0.0f)? (value.y - (old / sample)) * (value.y - ((old + value.y) / (sample + 1))): 0.0f;
	kernel_add_pass_float(buffer + 4, sample, var);
	if(copy) buffer[copy+1] = (old + value.y) / (sample + 1);

	old = kernel_add_pass_float(buffer+2, sample, value.z);
	var = (sample > 0.0f)? (value.z - (old / sample)) * (value.z - ((old + value.z) / (sample + 1))): 0.0f;
	kernel_add_pass_float(buffer + 5, sample, var);
	if(copy) buffer[copy+2] = (old + value.z) / (sample + 1);
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

	if(kernel_data.film.pass_lwr) {
		if(!(path_flag & PATH_RAY_LWR_PASS_DONE) && (state->roughness > 0.025f)) {
			kernel_write_lwr_float(buffer + kernel_data.film.pass_lwr, sample, L->depth);
			kernel_write_lwr_float3(buffer + kernel_data.film.pass_lwr + 2, sample, L->normal);
			kernel_write_lwr_float3(buffer + kernel_data.film.pass_lwr + 8, sample, L->color);
			state->flag |= PATH_RAY_LWR_PASS_DONE;
		}

		if(sd) {
			L->depth += ccl_fetch(sd, ray_length);
			L->normal = ccl_fetch(sd, N);
			L->color = shader_bsdf_diffuse(kg, sd) + shader_bsdf_glossy(kg, sd) + shader_bsdf_subsurface(kg, sd) + shader_bsdf_transmission(kg, sd);
		}
	}

	if(!(path_flag & PATH_RAY_CAMERA) || !sd)
		return;

	if(!(path_flag & PATH_RAY_SINGLE_PASS_DONE)) {
		if(!(ccl_fetch(sd, flag) & SD_TRANSPARENT) ||
		   kernel_data.film.pass_alpha_threshold == 0.0f ||
		   average(shader_bsdf_alpha(kg, sd)) >= kernel_data.film.pass_alpha_threshold)
		{

			if(sample == 0) {
				if(flag & PASS_DEPTH) {
					float depth = camera_distance(kg, ccl_fetch(sd, P));
					kernel_write_pass_float(buffer + kernel_data.film.pass_depth, sample, depth);
				}
				if(flag & PASS_OBJECT_ID) {
					float id = object_pass_id(kg, ccl_fetch(sd, object));
					kernel_write_pass_float(buffer + kernel_data.film.pass_object_id, sample, id);
				}
				if(flag & PASS_MATERIAL_ID) {
					float id = shader_pass_id(kg, sd);
					kernel_write_pass_float(buffer + kernel_data.film.pass_material_id, sample, id);
				}
			}

			if(flag & PASS_NORMAL) {
				float3 normal = ccl_fetch(sd, N);
				kernel_write_pass_float3(buffer + kernel_data.film.pass_normal, sample, normal);
			}
			if(flag & PASS_UV) {
				float3 uv = primitive_uv(kg, sd);
				kernel_write_pass_float3(buffer + kernel_data.film.pass_uv, sample, uv);
			}
			if(flag & PASS_MOTION) {
				float4 speed = primitive_motion_vector(kg, sd);
				kernel_write_pass_float4(buffer + kernel_data.film.pass_motion, sample, speed);
				kernel_write_pass_float(buffer + kernel_data.film.pass_motion_weight, sample, 1.0f);
			}

			state->flag |= PATH_RAY_SINGLE_PASS_DONE;
		}
	}

	if(flag & (PASS_DIFFUSE_INDIRECT|PASS_DIFFUSE_COLOR|PASS_DIFFUSE_DIRECT))
		L->color_diffuse += shader_bsdf_diffuse(kg, sd)*throughput;
	if(flag & (PASS_GLOSSY_INDIRECT|PASS_GLOSSY_COLOR|PASS_GLOSSY_DIRECT))
		L->color_glossy += shader_bsdf_glossy(kg, sd)*throughput;
	if(flag & (PASS_TRANSMISSION_INDIRECT|PASS_TRANSMISSION_COLOR|PASS_TRANSMISSION_DIRECT))
		L->color_transmission += shader_bsdf_transmission(kg, sd)*throughput;
	if(flag & (PASS_SUBSURFACE_INDIRECT|PASS_SUBSURFACE_COLOR|PASS_SUBSURFACE_DIRECT))
		L->color_subsurface += shader_bsdf_subsurface(kg, sd)*throughput;

	if(flag & PASS_MIST) {
		/* bring depth into 0..1 range */
		float mist_start = kernel_data.film.mist_start;
		float mist_inv_depth = kernel_data.film.mist_inv_depth;

		float depth = camera_distance(kg, ccl_fetch(sd, P));
		float mist = saturate((depth - mist_start)*mist_inv_depth);

		/* falloff */
		float mist_falloff = kernel_data.film.mist_falloff;

		if(mist_falloff == 1.0f)
			;
		else if(mist_falloff == 2.0f)
			mist = mist*mist;
		else if(mist_falloff == 0.5f)
			mist = sqrtf(mist);
		else
			mist = powf(mist, mist_falloff);

		/* modulate by transparency */
		float3 alpha = shader_bsdf_alpha(kg, sd);
		L->mist += (1.0f - mist)*average(throughput*alpha);
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

	if(flag & PASS_EMISSION)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_emission, sample, L->emission);
	if(flag & PASS_BACKGROUND)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_background, sample, L->background);
	if(flag & PASS_AO)
		kernel_write_pass_float3(buffer + kernel_data.film.pass_ao, sample, L->ao);

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
//	if(flag & PASS_MIST)
//		kernel_write_pass_float(buffer + kernel_data.film.pass_mist, sample, 1.0f - L->mist);
#endif
}

CCL_NAMESPACE_END

