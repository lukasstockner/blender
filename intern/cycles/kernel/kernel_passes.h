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
#if defined(__SPLIT_KERNEL__) && defined(__WORK_STEALING__)
	atomic_add_float(buf, value);
#else
	*buf = (sample == 0)? value: *buf + value;
#endif // __SPLIT_KERNEL__ && __WORK_STEALING__
}

ccl_device_inline void kernel_write_pass_float_var(ccl_global float *buffer, int sample, float value)
{
	if(sample == 0) {
		*buffer = value;
		*(buffer + 1) = 0.0f;
	}
	else {
		float old = *buffer;
		buffer[0] += value;
		/* Online single-pass variance estimation - difference to old mean multiplied by difference to new mean. */
		buffer[1] += (value - (old / sample)) * (value - ((old + value) / (sample + 1)));
	}
}

ccl_device_inline void kernel_write_pass_float3(ccl_global float *buffer, int sample, float3 value)
{
#if defined(__SPLIT_KERNEL__) && defined(__WORK_STEALING__)
	ccl_global float *buf_x = buffer + 0;
	ccl_global float *buf_y = buffer + 1;
	ccl_global float *buf_z = buffer + 2;

	atomic_add_float(buf_x, value.x);
	atomic_add_float(buf_y, value.y);
	atomic_add_float(buf_z, value.z);
#else
	ccl_global float3 *buf = (ccl_global float3*)buffer;
	*buf = (sample == 0)? value: *buf + value;
#endif // __SPLIT_KERNEL__ && __WORK_STEALING__
}

ccl_device_inline void kernel_write_pass_float3_nopad(ccl_global float *buffer, int sample, float3 value)
{
	/* TODO somehow avoid this duplicated function */
	buffer[0] = (sample == 0)? value.x: buffer[0] + value.x;
	buffer[1] = (sample == 0)? value.y: buffer[1] + value.y;
	buffer[2] = (sample == 0)? value.z: buffer[2] + value.z;
}

ccl_device_inline void kernel_write_pass_float3_var(ccl_global float *buffer, int sample, float3 value)
{
	if(sample == 0) {
		buffer[0] = value.x;
		buffer[1] = value.y;
		buffer[2] = value.z;
		buffer[3] = 0.0f;
		buffer[4] = 0.0f;
		buffer[5] = 0.0f;
	}
	else {
		float old;
		old = buffer[0];
		buffer[0] += value.x;
		buffer[3] += (value.x - (old / sample)) * (value.x - ((old + value.x) / (sample + 1)));
		old = buffer[1];
		buffer[1] += value.y;
		buffer[4] += (value.y - (old / sample)) * (value.y - ((old + value.y) / (sample + 1)));
		old = buffer[2];
		buffer[2] += value.z;
		buffer[5] += (value.z - (old / sample)) * (value.z - ((old + value.z) / (sample + 1)));
	}
}

ccl_device_inline void kernel_write_pass_float4(ccl_global float *buffer, int sample, float4 value)
{
#if defined(__SPLIT_KERNEL__) && defined(__WORK_STEALING__)
	ccl_global float *buf_x = buffer + 0;
	ccl_global float *buf_y = buffer + 1;
	ccl_global float *buf_z = buffer + 2;
	ccl_global float *buf_w = buffer + 3;

	atomic_add_float(buf_x, value.x);
	atomic_add_float(buf_y, value.y);
	atomic_add_float(buf_z, value.z);
	atomic_add_float(buf_w, value.w);
#else
	ccl_global float4 *buf = (ccl_global float4*)buffer;
	*buf = (sample == 0)? value: *buf + value;
#endif // __SPLIT_KERNEL__ && __WORK_STEALING__
}

ccl_device_inline void kernel_write_denoising_passes(KernelGlobals *kg, ccl_global float *buffer,
	ccl_addr_space PathState *state, ShaderData *sd, int sample, float3 world_albedo)
{
	if(kernel_data.film.pass_denoising == 0)
		return;
	buffer += kernel_data.film.pass_denoising;

	if(state->flag & PATH_RAY_DENOISING_PASS_DONE)
		return;

	/* Can also be called if the ray misses the scene, sd is NULL in that case. */
	if(sd) {
		state->path_length += ccl_fetch(sd, ray_length);

		float3 normal = make_float3(0.0f, 0.0f, 0.0f);
		float3 albedo = make_float3(0.0f, 0.0f, 0.0f);
		float sum_weight = 0.0f, max_weight = 0.0f;
		int max_weight_closure = -1;

		/* Average normal and albedo, determine the closure with the highest weight for the roughness decision. */
		for(int i = 0; i < ccl_fetch(sd, num_closure); i++) {
			ShaderClosure *sc = ccl_fetch_array(sd, closure, i);

			if(!CLOSURE_IS_BSDF_OR_BSSRDF(sc->type))
				continue;

			normal += sc->N * sc->sample_weight;
			albedo += sc->weight;
			sum_weight += sc->sample_weight;

			if(sc->sample_weight > max_weight) {
				max_weight = sc->sample_weight;
				max_weight_closure = i;
			}
		}

		if(sum_weight == 0.0f) {
			kernel_write_pass_float3_var(buffer, sample, make_float3(0.0f, 0.0f, 0.0f));
			kernel_write_pass_float3_var(buffer + 6, sample, make_float3(0.0f, 0.0f, 0.0f));
			kernel_write_pass_float_var(buffer + 12, sample, 0.0f);
		}
		else {
			ShaderClosure *max_sc = ccl_fetch_array(sd, closure, max_weight_closure);
			if(max_sc->roughness <= 0.075f) {
				/* This bounce is almost specular, so don't write the data yet. */
				return;
			}
			kernel_write_pass_float3_var(buffer, sample, normal/sum_weight);
			kernel_write_pass_float3_var(buffer + 6, sample, albedo);
			kernel_write_pass_float_var(buffer + 12, sample, state->path_length);
		}
	}
	else {
		kernel_write_pass_float3_var(buffer, sample, make_float3(0.0f, 0.0f, 0.0f));
		kernel_write_pass_float3_var(buffer + 6, sample, world_albedo);
		kernel_write_pass_float_var(buffer + 12, sample, 0.0f);
	}

	state->flag |= PATH_RAY_DENOISING_PASS_DONE;
}

ccl_device_inline void kernel_write_data_passes(KernelGlobals *kg, ccl_global float *buffer, PathRadiance *L,
	ShaderData *sd, int sample, ccl_addr_space PathState *state, float3 throughput)
{
#ifdef __PASSES__
	int path_flag = state->flag;

	if(!(path_flag & PATH_RAY_CAMERA))
		return;

	int flag = kernel_data.film.pass_flag;

	if(!(flag & PASS_ALL))
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
	if(flag & PASS_MIST)
		kernel_write_pass_float(buffer + kernel_data.film.pass_mist, sample, 1.0f - L->mist);
#endif
}

ccl_device_inline void kernel_write_result(KernelGlobals *kg, ccl_global float *buffer,
	int sample, PathRadiance *L, float alpha)
{
	if(L) {
		float3 L_sum = path_radiance_clamp_and_sum(kg, L);
		kernel_write_pass_float4(buffer, sample, make_float4(L_sum.x, L_sum.y, L_sum.z, alpha));

		kernel_write_light_passes(kg, buffer, L, sample);

		if(kernel_data.film.pass_denoising) {
			if(kernel_data.film.pass_no_denoising) {
				float3 noisy, clean;
				path_radiance_split_denoising(kg, L, &noisy, &clean);
				kernel_write_pass_float3_var(buffer + kernel_data.film.pass_denoising + 20, sample, noisy);
				kernel_write_pass_float3_nopad(buffer + kernel_data.film.pass_no_denoising, sample, clean);
			}
			else {
				kernel_write_pass_float3_var(buffer + kernel_data.film.pass_denoising + 20, sample, L_sum);
			}
		}
	}
	else {
		kernel_write_pass_float4(buffer, sample, make_float4(0.0f, 0.0f, 0.0f, 0.0f));

		if(kernel_data.film.pass_denoising) {
			kernel_write_pass_float3_var(buffer + kernel_data.film.pass_denoising + 20, sample, make_float3(0.0f, 0.0f, 0.0f));
		}
		if(kernel_data.film.pass_no_denoising) {
			kernel_write_pass_float3_nopad(buffer + kernel_data.film.pass_no_denoising, sample, make_float3(0.0f, 0.0f, 0.0f));
		}
	}
}

CCL_NAMESPACE_END

