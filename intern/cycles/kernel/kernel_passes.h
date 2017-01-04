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

#ifdef __DENOISING_FEATURES__
ccl_device_inline void kernel_write_denoising_shadow(KernelGlobals *kg, ccl_global float *buffer,
	int sample, float path_total, float path_total_shaded)
{
	if(kernel_data.film.pass_denoising_data == 0)
		return;

	if(sample & 1) buffer += 3;
	buffer += kernel_data.film.pass_denoising_data + 14;

	path_total = ensure_finite(path_total);
	path_total_shaded = ensure_finite(path_total_shaded);

	kernel_write_pass_float(buffer, sample/2, path_total);
	kernel_write_pass_float(buffer+1, sample/2, path_total_shaded);

	float value = path_total_shaded / max(path_total, 1e-7f);
#  ifdef __SPLIT_KERNEL__
	kernel_write_pass_float(buffer+2, sample/2, value*value);
#  else
	if(sample < 2) {
		kernel_write_pass_float(buffer+2, sample/2, 0.0f);
	}
	else {
		float old_value = (buffer[1] - path_total_shaded) / max(buffer[0] - path_total, 1e-7f);
		float new_value = buffer[1] / max(buffer[0], 1e-7f);
		kernel_write_pass_float(buffer+2, sample, (value - new_value) * (value - old_value));
	}
#  endif
}
#endif /* __DENOISING_FEATURES__ */

ccl_device_inline void kernel_update_denoising_features(KernelGlobals *kg,
                                                        ShaderData *sd,
                                                        ccl_global PathState *state,
                                                        PathRadiance *L)
{
#ifdef __DENOISING_FEATURES__
	if(state->denoising_feature_weight == 0.0f) {
		return;
	}

	L->denoising_depth += ensure_finite(state->denoising_feature_weight * sd->ray_length);

	float3 normal = make_float3(0.0f, 0.0f, 0.0f);
	float3 albedo = make_float3(0.0f, 0.0f, 0.0f);
	float sum_weight = 0.0f, sum_nonspecular_weight = 0.0f;

	for(int i = 0; i < sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(!CLOSURE_IS_BSDF_OR_BSSRDF(sc->type))
			continue;

		/* Classify closures into diffuse-like and specular-like closures.
		 * This is pretty arbitrary, but some distinction has to be made. */
		bool is_specular = (sc->type == CLOSURE_BSDF_TRANSPARENT_ID);
		if(CLOSURE_IS_BSDF_MICROFACET(sc->type)) {
			MicrofacetBsdf *bsdf = (MicrofacetBsdf*) sc;
			if(bsdf->alpha_x*bsdf->alpha_y <= 0.075f*0.075f) {
				is_specular = true;
			}
		}

		/* All closures contribute to the normal feature, but only diffuse-like ones to the albedo. */
		normal += sc->N * sc->sample_weight;
		sum_weight += sc->sample_weight;
		if(!is_specular) {
			albedo += sc->weight;
			sum_nonspecular_weight += sc->sample_weight;
		}
	}

	/* Wait for next bounce if 75% or more sample weight belongs to specular-like closures. */
	if((sum_weight == 0.0f) || (sum_nonspecular_weight*4.0f > sum_weight)) {
		if(sum_weight != 0.0f) {
			normal /= sum_weight;
		}
		L->denoising_normal += ensure_finite3(state->denoising_feature_weight * normal);
		L->denoising_albedo += ensure_finite3(state->denoising_feature_weight * albedo);

		state->denoising_feature_weight = 0.0f;
		if(!(state->flag & PATH_RAY_SHADOW_CATCHER)) {
			state->flag &= ~PATH_RAY_STORE_SHADOW_INFO;
		}
	}
#else
	(void) kg;
	(void) sd;
	(void) state;
	(void) L;
#endif  /* __DENOISING_FEATURES__ */
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
		if(!(sd->flag & SD_TRANSPARENT) ||
		   kernel_data.film.pass_alpha_threshold == 0.0f ||
		   average(shader_bsdf_alpha(kg, sd)) >= kernel_data.film.pass_alpha_threshold)
		{

			if(sample == 0) {
				if(flag & PASS_DEPTH) {
					float depth = camera_distance(kg, sd->P);
					kernel_write_pass_float(buffer + kernel_data.film.pass_depth, sample, depth);
				}
				if(flag & PASS_OBJECT_ID) {
					float id = object_pass_id(kg, sd->object);
					kernel_write_pass_float(buffer + kernel_data.film.pass_object_id, sample, id);
				}
				if(flag & PASS_MATERIAL_ID) {
					float id = shader_pass_id(kg, sd);
					kernel_write_pass_float(buffer + kernel_data.film.pass_material_id, sample, id);
				}
			}

			if(flag & PASS_NORMAL) {
				float3 normal = sd->N;
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

			for(int i = 0; kernel_data.film.pass_aov[i]; i++) {
				if((state->written_aovs & (1 << i)) == 0) {
					bool is_color = (kernel_data.film.pass_aov[i] & (1 << 31));
					int pass_offset = (kernel_data.film.pass_aov[i] & ~(1 << 31));
					if(is_color) {
						kernel_write_pass_float3(buffer + pass_offset, sample, make_float3(0.0f, 0.0f, 0.0f));
					}
					else {
						kernel_write_pass_float(buffer + pass_offset, sample, 0.0f);
					}
				}
			}
			state->written_aovs = ~0;

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

		float depth = camera_distance(kg, sd->P);
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
	int sample, PathRadiance *L, float alpha, bool is_shadow_catcher)
{
	int split_passes = (kernel_data.film.denoising_flags & DENOISING_USE_SPLIT_PASSES);

	if(L) {
		float3 L_sum;
#ifdef __SHADOW_TRICKS__
		if(is_shadow_catcher) {
			L_sum = path_radiance_sum_shadowcatcher(kg, L, &alpha);
		}
		else
#endif  /* __SHADOW_TRICKS__ */
		{
			L_sum = path_radiance_clamp_and_sum(kg, L);
		}

		kernel_write_pass_float4(buffer, sample, make_float4(L_sum.x, L_sum.y, L_sum.z, alpha));

		kernel_write_light_passes(kg, buffer, L, sample);

#ifdef __DENOISING_FEATURES__
		if(kernel_data.film.pass_denoising_data) {
#  ifdef __SHADOW_TRICKS__
			kernel_write_denoising_shadow(kg, buffer, sample, average(L->path_total), average(L->path_total_shaded));
#  else
			kernel_write_denoising_shadow(kg, buffer, sample, 0.0f, 0.0f);
#  endif
			if(kernel_data.film.pass_denoising_clean) {
				float3 noisy, clean;
				path_radiance_split_denoising(kg, L, &noisy, &clean);
				kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data + 20, sample, noisy);
				if(split_passes && !(sample & 1)) {
					kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data + 26, sample, noisy);
				}
				kernel_write_pass_float3_unaligned(buffer + kernel_data.film.pass_denoising_clean, sample, clean);
			}
			else {
				kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data + 20, sample, L_sum);
				if(split_passes && !(sample & 1)) {
					kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data + 26, sample, L_sum);
				}
			}

			kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data, sample, L->denoising_normal);
			kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data + 6, sample, L->denoising_albedo);
			kernel_write_pass_float_variance(buffer + kernel_data.film.pass_denoising_data + 12, sample, L->denoising_depth);
		}
#endif  /* __DENOISING_FEATURES__ */
	}
	else {
		kernel_write_pass_float4(buffer, sample, make_float4(0.0f, 0.0f, 0.0f, 0.0f));

#ifdef __DENOISING_FEATURES__
		if(kernel_data.film.pass_denoising_data) {
			kernel_write_denoising_shadow(kg, buffer, sample, 0.0f, 0.0f);

			kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data + 20, sample, make_float3(0.0f, 0.0f, 0.0f));
			if(split_passes && !(sample & 1)) {
				kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data + 26, sample, make_float3(0.0f, 0.0f, 0.0f));
			}

			kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data, sample, make_float3(0.0f, 0.0f, 0.0f));
			kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data + 6, sample, make_float3(0.0f, 0.0f, 0.0f));
			kernel_write_pass_float_variance(buffer + kernel_data.film.pass_denoising_data + 12, sample, 0.0f);

			if(kernel_data.film.pass_denoising_clean) {
				kernel_write_pass_float3_unaligned(buffer + kernel_data.film.pass_denoising_clean, sample, make_float3(0.0f, 0.0f, 0.0f));
			}
		}
#endif  /* __DENOISING_FEATURES__ */
	}
}

CCL_NAMESPACE_END

