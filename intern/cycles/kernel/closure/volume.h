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
 * limitations under the License
 */

#ifndef __VOLUME_H__
#define __VOLUME_H__

CCL_NAMESPACE_BEGIN

/* note: the interfaces here are just as an example, need to figure
 * out the right functions and parameters to use */

/* ISOTROPIC VOLUME CLOSURE */

__device int volume_isotropic_setup(ShaderClosure *sc, float density)
{
	sc->type = CLOSURE_VOLUME_ISOTROPIC_ID;
	sc->data0 = density;

	return SD_VOLUME;
}

__device float3 volume_isotropic_eval_phase(const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
	return make_float3(1.0f, 1.0f, 1.0f);
}

/* TRANSPARENT VOLUME CLOSURE */

__device int volume_transparent_setup(ShaderClosure *sc, float density)
{
	sc->type = CLOSURE_VOLUME_TRANSPARENT_ID;
	sc->data0 = density;

	return SD_VOLUME;
}

__device float3 volume_transparent_eval_phase(const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
	return make_float3(1.0f, 1.0f, 1.0f);
}

/* VOLUME CLOSURE */

__device float3 volume_eval_phase(KernelGlobals *kg, const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
#ifdef __OSL__
	if(kg->osl && sc->prim)
		return OSLShader::volume_eval_phase(sc, omega_in, omega_out);
#endif

	float3 eval;

	switch(sc->type) {
		case CLOSURE_VOLUME_ISOTROPIC_ID:
			eval = volume_isotropic_eval_phase(sc, omega_in, omega_out);
			break;
		case CLOSURE_VOLUME_TRANSPARENT_ID:
			eval = volume_transparent_eval_phase(sc, omega_in, omega_out);
			break;
		default:
			eval = make_float3(0.0f, 0.0f, 0.0f);
			break;
	}

	return eval;
}

/* HENYEY_GREENSTEIN CLOSURE */

__device int volume_double_peaked_henyey_greeenstein_setup(ShaderClosure *sc, float density, float g)
{
	sc->type = CLOSURE_BSDF_DOUBLE_PEAKED_HENYEY_GREENSTEIN_ID;
	sc->data0 = density;
	sc->data1 = g;
	return SD_BSDF|SD_BSDF_HAS_EVAL;
}

// given cosinus between rays, return probability density that photon bounce to that direction
// F and g parameters controlling how far it difference from uniform sphere. g=0 uniform diffusion-like, g = 1 - very close to sharp single ray,
// F = 0.5 - uniform, F = 0 - most backward reflect, F = 1 most transit
// (F=0.5, g=0) - sphere, (F=0.5, g=1) -very polished half transparent mirror, (F=0 g=1) perfect mirror, (F=1 g=1) perfect transparent glass.

__device float single_peaked_henyey_greenstein(float cos_theta, float m_g)
{
	float p = (1.0f-m_g*m_g)/pow(1.0f+m_g*m_g-2.0f*m_g*cos_theta,1.5f)/4.0f/M_PI_F;
	return p;
};


__device float single_peaked_henyey_greenstein_accumulated_prob(float cos_theta, float m_g)
{
	float p = (1.0f-m_g*m_g) / (2.0f * m_g) * 
		(pow(1.0f+m_g*m_g-2.0f*m_g*cos_theta,1.5f) - 1.0f / (1.0f + m_g));
	return p;
};
/*
__device float single_peaked_henyey_greenstein_forward_prob(float m_g)
{
	float p = (1.0f-m_g) * (1.0f + m_g) / 
		pow(1.0f+m_g*m_g-2.0f*m_g*cos_theta,1.5f) / 4.0f/M_PI_F;
	return p;
};
*/
__device float double_peaked_henyey_greenstein(float cos_theta, float m_F, float m_g)
{
#if 0
	float q1 = (1.0f-sqrt(m_g))/pow(1.0f+sqrt(m_g)-2.0f*m_g*cos_theta,1.5f)/4.0f/M_PI_F;
	float q2 = (1.0f-sqrt(m_g))/pow(1.0f+sqrt(m_g)+2.0f*m_g*cos_theta,1.5f)/4.0f/M_PI_F;
	float p = (1.0f+m_F)/2.0f*q1+(1.0f-m_F)/2.0f*q2;
#else
	float q1 = (1.0f-m_g*m_g)/pow(1.0f+m_g*m_g-2.0f*m_g*cos_theta,1.5f)/4.0f/M_PI_F;
	float q2 = (1.0f-m_g*m_g)/pow(1.0f+m_g*m_g+2.0f*m_g*cos_theta,1.5f)/4.0f/M_PI_F;
	float p = (1.0f+m_F)/2.0f*q1+(1.0f-m_F)/2.0f*q2;
#endif
	return p;
};

#if 0

// just return bsdf at input vector
__device float3 bsdf_double_peaked_henyey_greenstein_eval(const ShaderData *sd, const ShaderClosure *sc, const float3 I, float3 omega_in, float *pdf)
{
	float m_F = sc->data0;
	float m_g = sc->data1;

	float cos_theta = dot( sd->I, omega_in);
	*pdf = double_peaked_henyey_greenstein( cos_theta, m_F, m_g);
	return make_float3( *pdf, *pdf, *pdf);
}
// calculate sample vector, return it and bsdf
// brute force version, sphere uniform sample, then calc pdf at random vector. work good only when g very close to 0
__device int bsdf_double_peaked_henyey_greenstein_sample(const ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
//	float m_F = sc->data0;
	float m_g = sc->data1;
	float3 m_N = sd->N;

//	sample_cos_hemisphere(m_N, randu, randv, omega_in, pdf);

	*omega_in = sample_uniform_sphere( randu, randv);
//	*omega_in = normalize(*omega_in);

	float cos_theta = dot( sd->I, *omega_in);
#if 0
	cos_theta = 1;
	m_F = 0;
	m_g = 0;
#endif
//	float q1 = (1.0f-sqrt(m_g))/pow(1.0f+sqrt(m_g)-2.0f*m_g*cos_theta,1.5f)/4.0f/M_PI_F;
//	float q2 = (1.0f-sqrt(m_g))/pow(1.0f+sqrt(m_g)+2.0f*m_g*cos_theta,1.5f)/4.0f/M_PI_F;
//	float p = (1.0f+m_F)/2.0f*q1+(1.0f-m_F)/2.0f*q2;
//	p = M_1_PI_F;
//	*pdf = p;
//	*pdf = double_peaked_henyey_greenstein(cos_theta, m_F, m_g);
//	*pdf = single_peaked_henyey_greenstein(cos_theta, m_g);
	*pdf = 1.0f/4.0f/M_PI_F;

//	*pdf = cos_theta;
	*eval = make_float3(*pdf, *pdf, *pdf);
//	*eval *= sc->weight[1]; // absorb?
//	*eval = make_float3(sc->data1, *pdf, *pdf);
#ifdef __RAY_DIFFERENTIALS__
		// TODO: find a better approximation for the diffuse bounce
		*domega_in_dx = (2 * dot(m_N, sd->dI.dx)) * m_N - sd->dI.dx;
		*domega_in_dy = (2 * dot(m_N, sd->dI.dy)) * m_N - sd->dI.dy;
		*domega_in_dx *= 125.0f;
		*domega_in_dy *= 125.0f;
#endif
	return LABEL_REFLECT|LABEL_DIFFUSE;
}
#else

// new optimized, importance sampled version, no difference when g = 0 but huge gain at other g values (less variance)

// just return bsdf at input vector
__device float3 bsdf_double_peaked_henyey_greenstein_eval(const ShaderData *sd, const ShaderClosure *sc, const float3 I, float3 omega_in, float *pdf)
{
//	float m_F = sc->data0;
	float m_g = sc->data1;
	const float magic_eps = 0.001f;

	// WARNING! sd->I point in backward direction!
//	float cos_theta = dot( sd->I, omega_in);
	float cos_theta = dot( -sd->I, omega_in);
//	*pdf = double_peaked_henyey_greenstein( cos_theta, m_F, m_g);
	if ( fabsf(m_g) <  magic_eps)
		*pdf = M_1_PI_F * 0.25f; // ?? double check it
	else
		*pdf = single_peaked_henyey_greenstein( cos_theta, m_g);
//	*pdf = M_1_PI_F / 4;
	return make_float3( *pdf, *pdf, *pdf);
}
__device int bsdf_double_peaked_henyey_greenstein_sample(const ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float m_g = sc->data1;
	float3 m_N = sd->N;
	const float magic_eps = 0.001f;
#if 0
	float4 tt4;
	float3 tt3;
	float4* ptt4 = &tt4;
	float3* ptt3 = &tt3;

	tt4.x = 3.3f;
	tt3.x = 3.3f;
	(*ptt4).x = 3.3f; 
	(*ptt3).x = 3.3f; 
#endif
	// WARNING! sd->I point in backward direction!

	if ( fabsf(m_g) <  magic_eps)
	{
		*omega_in = sample_uniform_sphere( randu, randv);
		*pdf = M_1_PI_F * 0.25f; // ?? double check it
	}
	else
	{
		float cos_phi, sin_phi, cos_theta;

		if( fabsf(m_g) <  magic_eps)
			cos_theta = (1.0f - 2.0f * randu);
		else
		{
			float k = (1.0f - m_g * m_g) / (1.0f - m_g + 2.0f * m_g * randu);
	//		float cos_theta = 1.0f / (2.0f * m_g) * (1.0f + m_g * m_g - k*k);
			cos_theta = (1.0f + m_g * m_g - k*k) / (2.0f * m_g);
	//		float cos_theta = (1.0f - 2.0f * randu);
	//		float cos_theta = randu;
		}
		float sin_theta = sqrt(1 - cos_theta * cos_theta);
		
		float3 T, B;
		make_orthonormals(-sd->I, &T, &B);
		float phi = 2.f * M_PI_F * randv;
		cos_phi = cosf( phi);
		sin_phi = sinf( phi);
		*omega_in = sin_theta * cos_phi * T + sin_theta * sin_phi * B + cos_theta * (-sd->I);
		*pdf = single_peaked_henyey_greenstein( cos_theta, m_g);
	}

	*eval = make_float3(*pdf, *pdf, *pdf); // perfect importance sampling
#ifdef __RAY_DIFFERENTIALS__
		// TODO: find a better approximation for the diffuse bounce
		*domega_in_dx = (2 * dot(m_N, sd->dI.dx)) * m_N - sd->dI.dx;
		*domega_in_dy = (2 * dot(m_N, sd->dI.dy)) * m_N - sd->dI.dy;
		*domega_in_dx *= 125.0f;
		*domega_in_dy *= 125.0f;
#endif
	return LABEL_REFLECT|LABEL_DIFFUSE;
}

#endif

__device int volume_bsdf_sample(KernelGlobals *kg, const ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3 *eval, float3 *omega_in, differential3 *domega_in, float *pdf)
{
	int label;

#ifdef __OSL__
        if(kg->osl && sc->prim)
			return OSLShader::bsdf_sample(sd, sc, randu, randv, *eval, *omega_in, *domega_in, *pdf);
#endif

	switch(sc->type) {
		case CLOSURE_BSDF_DOUBLE_PEAKED_HENYEY_GREENSTEIN_ID:
			label = bsdf_double_peaked_henyey_greenstein_sample(sd, sc, randu, randv, eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		default:
			*eval = make_float3(0.0f, 0.0f, 0.0f);
			label = LABEL_NONE;
			break;
	}

//	*eval *= sd->svm_closure_weight;

	return label;
}
CCL_NAMESPACE_END

#endif
