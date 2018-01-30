/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __KERNEL_CPU__
#include <fenv.h>
#endif

#ifndef __BSDF_NEWHAIR_H__
#define __BSDF_NEWHAIR_H__

CCL_NAMESPACE_BEGIN

typedef ccl_addr_space struct NewHairBsdf {
	SHADER_CLOSURE_BASE;

	float3 sigma;
	float4 geom;
	float v;
	float s;
	float alpha;
	float eta;
} NewHairBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(NewHairBsdf), "NewHairBsdf is too large!");

ccl_device_inline float cos_from_sin(const float s)
{
	return safe_sqrtf(1.0f - s*s);
}

/* Gives the change in direction in the normal plane for the given angles and p-th-order scattering. */
ccl_device_inline float delta_phi(int p, float gamma_o, float gamma_t) {
	return 2.0f*(p * (gamma_t + M_PI_2_F) - gamma_o);
}

ccl_device_inline float wrap_angle(float a)
{
	while(a > M_PI_F) a -= M_2PI_F;
	while(a < -M_PI_F) a += M_2PI_F;
	return a;
}

ccl_device_inline float logistic(float x, float s)
{
	float v = expf(-fabsf(x)/s);
	return v / (s * sqr(1.0f + v));
}

ccl_device_inline float logistic_cdf(float x, float s)
{
	float arg = -x/s;
	if(arg > 100.0f) return 0.0f;
	return 1.0f / (1.0f + expf(arg));
}

ccl_device_inline float bessel_I0(float x)
{
	x = sqr(x);
	float val = 1.0f + 0.25f*x;
	float pow_x_2i = sqr(x);
	uint64_t i_fac_2 = 1;
	int pow_4_i = 16;
	for(int i = 2; i < 10; i++) {
		i_fac_2 *= i*i;
		float newval = val + pow_x_2i / (pow_4_i * i_fac_2);
		if(val == newval) return val;
		val = newval;
		pow_x_2i *= x;
		pow_4_i *= 4;
	}
	return val;
}

ccl_device_inline float log_bessel_I0(float x)
{
	if (x > 12.0f) {
		return x + 0.5f * (1.f / (8.0f * x) - M_LN_2PI_F - logf(x));
	}
	else {
		return logf(bessel_I0(x));
	}
}

/* Logistic distribution limited to the interval from -pi to pi. */
ccl_device_inline float trimmed_logistic(float x, float s)
{
	/* The logistic distribution is symmetric and centered around zero,
	 * so logistic_cdf(x, s) = 1 - logistic_cdf(-x, s).
	 * Therefore, logistic_cdf(x, s)-logistic_cdf(-x, s) = 1 - 2*logistic_cdf(-x, s) */
	float scaling_fac = 1.0f - 2.0f*logistic_cdf(-M_PI_F, s);
	float val = logistic(x, s);
	return val / scaling_fac;
}

ccl_device_inline float sample_trimmed_logistic(float u, float s)
{
	float cdf_minuspi = logistic_cdf(-M_PI_F, s);
	float x = -s*logf(1.0f / (u*(1.0f - 2.0f*cdf_minuspi) + cdf_minuspi) - 1.0f);
	return clamp(x, -M_PI_F, M_PI_F);
}

ccl_device_inline float azimuthal_scattering(float phi, int p, float s, float gamma_o, float gamma_t)
{
	float phi_o = wrap_angle(phi - delta_phi(p, gamma_o, gamma_t));
	float val = trimmed_logistic(phi_o, s);
	//printf("Azi Phi %f Val %f\n", (double)phi_o, (double)val);
	return val;
}

ccl_device_inline float longitudinal_scattering(float sin_theta_i, float cos_theta_i, float sin_theta_o, float cos_theta_o, float v)
{
	float inv_v = 1.0f/v;
	float cos_arg = cos_theta_i * cos_theta_o * inv_v;
	float sin_arg = sin_theta_i * sin_theta_o * inv_v;
	if(v <= 0.1f) {
		float i0 = log_bessel_I0(cos_arg);
		float val = expf(i0 - sin_arg - inv_v + 0.6931f + logf(0.5f*inv_v));
		//printf("Long LogI0 %f val %f\n", (double)i0, (double)val);
		return val;
	}
	else {
		float i0 = bessel_I0(cos_arg);
		float val = (expf(-sin_arg) * i0) / (sinhf(inv_v) * 2.0f * v);
		//printf("Long I0 %f val %f\n", (double)i0, (double)val);
		return val;
	}
}

ccl_device_inline float4 combine_with_energy(float3 c)
{
	return make_float4(c.x, c.y, c.z, linear_rgb_to_gray(c));
}

ccl_device int bsdf_newhair_setup(KernelGlobals *kg, ShaderData *sd, NewHairBsdf *bsdf)
{
	if((sd->type & PRIMITIVE_ALL_CURVE) == 0) {
		bsdf->type = CLOSURE_BSDF_DIFFUSE_ID;
		return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_NEEDS_LCG;
	}

	bsdf->type = CLOSURE_BSDF_NEWHAIR_ID;
	bsdf->v = clamp(bsdf->v, 0.001f, 0.999f);
	bsdf->s = clamp(bsdf->s, 0.001f, 0.999f);

	bsdf->v = sqr(0.726f*bsdf->v + 0.812f*sqr(bsdf->v) + 3.700f*pow20(bsdf->v));
	bsdf->s =    (0.265f*bsdf->s + 1.194f*sqr(bsdf->s) + 5.372f*pow22(bsdf->s))*M_SQRT_PI_8_F;

	float3 dPdCD, curve_P;
	float h = curve_core_distance(kg, sd, &curve_P, &dPdCD);
	dPdCD = normalize(dPdCD);
	bsdf->geom = make_float4(dPdCD.x, dPdCD.y, dPdCD.z, h);

	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_NEEDS_LCG;
}

ccl_device_inline void hair_ap(float f, float3 T, float4 *Ap)
{
	Ap[0] = make_float4(f, f, f, f);
	float3 col = sqr(1.0f - f) * T;
	Ap[1] = combine_with_energy(col);
	col *= T*f;
	Ap[2] = combine_with_energy(col);
	col *= T*f;
	Ap[3] = combine_with_energy(col / (make_float3(1.0f, 1.0f, 1.0f) - T*f));

	float fac = 1.0f / (Ap[0].w + Ap[1].w + Ap[2].w + Ap[3].w);
	Ap[0].w *= fac;
	Ap[1].w *= fac;
	Ap[2].w *= fac;
	Ap[3].w *= fac;
}

ccl_device_inline void hair_alpha_angles(float sin_theta_i, float cos_theta_i, float alpha, float *angles)
{
	float sin_1alpha = sinf(alpha);
	float cos_1alpha = cos_from_sin(sin_1alpha);
	float sin_2alpha = 2.0f*sin_1alpha*cos_1alpha;
	float cos_2alpha = sqr(cos_1alpha) - sqr(sin_1alpha);
	float sin_4alpha = 2.0f*sin_2alpha*cos_2alpha;
	float cos_4alpha = sqr(cos_2alpha) - sqr(sin_2alpha);

	angles[0] = sin_theta_i*cos_2alpha + cos_theta_i*sin_2alpha;
	angles[1] = fabsf(cos_theta_i*cos_2alpha - sin_theta_i*sin_2alpha);
	angles[2] = sin_theta_i*cos_1alpha - cos_theta_i*sin_1alpha;
	angles[3] = fabsf(cos_theta_i*cos_1alpha + sin_theta_i*sin_1alpha);
	angles[4] = sin_theta_i*cos_4alpha - cos_theta_i*sin_4alpha;
	angles[5] = fabsf(cos_theta_i*cos_4alpha + sin_theta_i*sin_4alpha);
}

ccl_device float3 bsdf_newhair_eval(const ShaderData *sd, const ShaderClosure *sc, const float3 omega_in, float *pdf)
{
	//*pdf = 0.0f;
	//return make_float3(0.0f, 0.0f, 0.0f);

	const NewHairBsdf *bsdf = (const NewHairBsdf*) sc;
	float3 dPdCD = float4_to_float3(bsdf->geom);

	assert(fabsf(dot(sd->dPdu, dPdCD)) < 1e-5f);
	float3 Z = normalize(cross(sd->dPdu, dPdCD));
	float3 Y = dPdCD;

	float3 wo = make_float3(dot(sd->I, sd->dPdu), dot(sd->I, Y), dot(sd->I, Z));
	float3 wi = make_float3(dot(omega_in, sd->dPdu), dot(omega_in, dPdCD), dot(omega_in, Z));
	//assert(fabsf(wo.y) < 1e-5f);
	//scanf("%d %d %d %d %d %d %d", &wo.x, &wo.y, &wo.z, &bsdf->geom.w, &wi.x, &wi.y, &wi.z);

	float sin_theta_o = wo.x;
	float cos_theta_o = cos_from_sin(sin_theta_o);
	float phi_o = atan2f(wo.z, wo.y);

	float sin_theta_t = sin_theta_o / bsdf->eta;
	float cos_theta_t = cos_from_sin(sin_theta_t);

	float sin_gamma_o = bsdf->geom.w;
	float cos_gamma_o = cos_from_sin(sin_gamma_o);
	float gamma_o = safe_asinf(sin_gamma_o);

	float sin_gamma_t = sin_gamma_o * cos_theta_o / sqrtf(sqr(bsdf->eta) - sqr(sin_theta_o));
	float cos_gamma_t = cos_from_sin(sin_gamma_t);
	float gamma_t = safe_asinf(sin_gamma_t);

	float3 T = exp3(-bsdf->sigma * (2.0f * cos_gamma_t / cos_theta_t));
	float4 Ap[4];
	hair_ap(fresnel_dielectric_cos(cos_theta_o * cos_gamma_o, bsdf->eta), T, Ap);

	//printf("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n", (double)Ap[0].x, (double)Ap[0].y, (double)Ap[0].z, (double)Ap[0].w, (double)Ap[1].x, (double)Ap[1].y, (double)Ap[1].z, (double)Ap[1].w, (double)Ap[2].x, (double)Ap[2].y, (double)Ap[2].z, (double)Ap[2].w, (double)Ap[3].x, (double)Ap[3].y, (double)Ap[3].z, (double)Ap[3].w);

	float sin_theta_i = wi.x;
	float cos_theta_i = cos_from_sin(sin_theta_i);
	float phi_i = atan2f(wi.z, wi.y);

	float phi = phi_i - phi_o;

	float angles[6];
	hair_alpha_angles(sin_theta_i, cos_theta_i, bsdf->alpha, angles);
	//printf("%f %f %f %f %f %f\n", (double)angles[0], (double)angles[1], (double)angles[2], (double)angles[3], (double)angles[4], (double)angles[5]);

	float4 F;
	F  = Ap[0] * azimuthal_scattering(phi, 0, bsdf->s, gamma_o, gamma_t)
	           * longitudinal_scattering(angles[0], angles[1], sin_theta_o, cos_theta_o, bsdf->v);

	F += Ap[1] * azimuthal_scattering(phi, 1, bsdf->s, gamma_o, gamma_t)
	           * longitudinal_scattering(angles[2], angles[3], sin_theta_o, cos_theta_o, 0.25f*bsdf->v);

	F += Ap[2] * azimuthal_scattering(phi, 2, bsdf->s, gamma_o, gamma_t)
	           * longitudinal_scattering(angles[4], angles[5], sin_theta_o, cos_theta_o, 4.0f*bsdf->v);

	F += Ap[3] * longitudinal_scattering(sin_theta_i, cos_theta_i, sin_theta_o, cos_theta_o, 4.0f*bsdf->v) * M_1_2PI_F;

	//printf("%f %f %f %f\n", (double)F.x, (double)F.y, (double)F.z, (double)F.w);

	*pdf = F.w;
	return float4_to_float3(F);
}

ccl_device int bsdf_newhair_sample(ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
#ifdef __KERNEL_CPU__
	//feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
#endif

	const NewHairBsdf *bsdf = (const NewHairBsdf*) sc;
	float3 dPdCD = float4_to_float3(bsdf->geom);

	assert(fabsf(dot(sd->dPdu, dPdCD)) < 1e-5f);
	float3 Z = normalize(cross(sd->dPdu, dPdCD));
	float3 Y = dPdCD;

	float3 wo = make_float3(dot(sd->I, sd->dPdu), dot(sd->I, Y), dot(sd->I, Z));
	//assert(fabsf(wo.y) < 1e-5f);

	float2 u[2];
	u[0] = make_float2(randu, randv);
	u[1] = make_float2(lcg_step_float_addrspace(&sd->lcg_state), lcg_step_float_addrspace(&sd->lcg_state));
	//printf("Enter sample data: ");
	//scanf("%d %d %d %d %d %d %d %d", &wo.x, &wo.y, &wo.z, &bsdf->geom.w, &u[0].x, &u[0].y, &u[1].x, &u[1].y);

	float sin_theta_o = wo.x;
	float cos_theta_o = cos_from_sin(sin_theta_o);
	float phi_o = atan2f(wo.z, wo.y);

	float sin_theta_t = sin_theta_o / bsdf->eta;
	float cos_theta_t = cos_from_sin(sin_theta_t);

	float sin_gamma_o = bsdf->geom.w;
	float cos_gamma_o = cos_from_sin(sin_gamma_o);
	float gamma_o = safe_asinf(sin_gamma_o);

	float sin_gamma_t = sin_gamma_o * cos_theta_o / sqrtf(sqr(bsdf->eta) - sqr(sin_theta_o));
	float cos_gamma_t = cos_from_sin(sin_gamma_t);
	float gamma_t = safe_asinf(sin_gamma_t);

	float3 T = exp3(-bsdf->sigma * (2.0f * cos_gamma_t / cos_theta_t));
	float4 Ap[4];
	hair_ap(fresnel_dielectric_cos(cos_theta_o * cos_gamma_o, bsdf->eta), T, Ap);

	//printf("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n", (double)Ap[0].x, (double)Ap[0].y, (double)Ap[0].z, (double)Ap[0].w, (double)Ap[1].x, (double)Ap[1].y, (double)Ap[1].z, (double)Ap[1].w, (double)Ap[2].x, (double)Ap[2].y, (double)Ap[2].z, (double)Ap[2].w, (double)Ap[3].x, (double)Ap[3].y, (double)Ap[3].z, (double)Ap[3].w);

	int p = 0;
	for(; p < 3; p++) {
		if(u[0].x < Ap[p].w) break;
		u[0].x -= Ap[p].w;
	}

	float v = bsdf->v;
	if(p == 1) v *= 0.25f;
	if(p >= 2) v *= 4.0f;

	u[1].x = max(u[1].x, 1e-5f);
	float fac = 1.0f + v*logf(u[1].x + (1.0f - u[1].x)*expf(-2.0f/v));
	float sin_theta_i = -fac * sin_theta_o + cos_from_sin(fac) * cosf(M_2PI_F * u[1].y) * cos_theta_o;
	float cos_theta_i = cos_from_sin(sin_theta_i);

	//printf("%d %f %f %f\n", p, (double)v, (double)sin_theta_i, (double)cos_theta_i);

	float angles[6];
	if(p < 3) {
		hair_alpha_angles(sin_theta_i, cos_theta_i, -bsdf->alpha, angles);
		sin_theta_i = angles[2*p];
		cos_theta_i = angles[2*p+1];
	}

	//printf("%f %f %f %f %f %f\n", (double)angles[0], (double)angles[1], (double)angles[2], (double)angles[3], (double)angles[4], (double)angles[5]);

	float phi;
	if(p < 3) {
		phi = delta_phi(p, gamma_o, gamma_t) + sample_trimmed_logistic(u[0].y, bsdf->s);
	}
	else {
		phi = M_2PI_F*u[0].y;
	}
	float phi_i = phi_o + phi;

	//printf("%f %f\n", (double)phi, (double)phi_i);

	hair_alpha_angles(sin_theta_i, cos_theta_i, bsdf->alpha, angles);

	//printf("%f %f %f %f %f %f\n", (double)angles[0], (double)angles[1], (double)angles[2], (double)angles[3], (double)angles[4], (double)angles[5]);

	float4 F;
	F  = Ap[0] * azimuthal_scattering(phi, 0, bsdf->s, gamma_o, gamma_t)
	           * longitudinal_scattering(angles[0], angles[1], sin_theta_o, cos_theta_o, bsdf->v);

	F += Ap[1] * azimuthal_scattering(phi, 1, bsdf->s, gamma_o, gamma_t)
	           * longitudinal_scattering(angles[2], angles[3], sin_theta_o, cos_theta_o, 0.25f*bsdf->v);

	F += Ap[2] * azimuthal_scattering(phi, 2, bsdf->s, gamma_o, gamma_t)
	           * longitudinal_scattering(angles[4], angles[5], sin_theta_o, cos_theta_o, 4.0f*bsdf->v);

	F += Ap[3] * longitudinal_scattering(sin_theta_i, cos_theta_i, sin_theta_o, cos_theta_o, 4.0f*bsdf->v) * M_1_2PI_F;

	*eval = float4_to_float3(F);
	*pdf = F.w;

	//printf("%f %f %f %f %f %f %f\n", (double)eval->x, (double)eval->y, (double)eval->z, (double)*pdf, (double)sin_theta_i, (double)cosf(phi_i), (double)sinf(phi_i));

	*omega_in = sd->dPdu*sin_theta_i + Y*cos_theta_i*cosf(phi_i) + Z*cos_theta_i*sinf(phi_i);

#ifdef __RAY_DIFFERENTIALS__
	float3 N = normalize(sd->I + *omega_in);
	*domega_in_dx = (2 * dot(N, sd->dI.dx)) * N - sd->dI.dx;
	*domega_in_dy = (2 * dot(N, sd->dI.dy)) * N - sd->dI.dy;
#endif

#ifdef __KERNEL_CPU__
	//fedisableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
#endif

	return LABEL_GLOSSY|((p == 0)? LABEL_REFLECT : LABEL_TRANSMIT);
}

CCL_NAMESPACE_END

#endif /* __BSDF_NEWHAIR_H__ */

