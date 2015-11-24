ccl_device_inline float3 mf_sample_phase(const float3 wi, const float alpha, const float eta, float3 &weight, const float2 randU)
{
	const float wi_11 = normalize(make_float3(alpha*wi.x, alpha*wi.y, wi.z));
	const float2 slope_11 = mf_sampleP22_11(wi_11, randU);

	const float phi = atan2f(wi_11.y, wi_11.x);
	const float2 cossin_phi = phi / len(phi);
	const float2 slope = alpha*make_float2(cossin_phi.x * slope_11.x - cossin_phi.y * slope_11.y, cossin_phi.y * slope_11.x + cossin_phi.x * slope_11.y);

	float3 wm;
	if(isnan(slope.x) || isinf(slope.x))
		wm = (wi.z > 1e-5f)? make_float3(0.0f, 0.0f, 1.0f): normalize(make_float3(wi.x, wi.y, 0.0f)); /* TODO(lukas): normalize() necessary? */
	else
		wm = normalize(make_float3(-slope.x, -slope.y, 1.0f));

	weight *= fresnel(wi, wm, eta);

	return -wi + 2.0f * wm * dot(wi, wm);
}

ccl_device_inline float mf_eval_phase(const float3 w, const float lambda, const float3 wo, const float alpha, const float eta)
{
	if(w.z > 0.9999f)
		return 0.0f;

	const float3 wh = normalize(wo - w);
	if(wh.z < 0.0f)
		return 0.0f;

	float projArea;
	if(w.z < -0.9999f)
		projArea = 1.0f;
	else
		projArea = lambda*w.z;

	const float dotW_WH = dot(-w, wh);
	if(dotW_WH < 0.0f)
		return make_float3(0.0f, 0.0f, 0.0f);
	return fresnel(-w, wh, eta) * D_ggx(wh, alpha) * 0.25f / projArea;
}

ccl_device_inline float mf_lambda(const float3 w, const float alpha)
{
	if(w.z > 0.9999f)
		return 0.0f;
	else if(w.z < -0.9999f)
		return -1.0f;

	float invA = alpha * w.z / sqrtf(1.0f - w.z*w.z);
	return 0.5f*(((a>0)?1.0f:-1.0f) * sqrtf(1.0f + a*a));
}

ccl_device_inline float mv_invC1(const float h)
{
	return 2.0f * saturate(h) - 1.0f;
}

ccl_device_inline float mf_G1(const float3 w, const float C1, const float lambda)
{
	if(w.z > 0.9999f)
		return 1.0f;
	if(w.z < 1e-5f)
		return 0.0f;
	return powf(C1, lambda);
}

ccl_device_inline bool mf_sample_height(const float3 w, float &h, float &C1, float &G1, float &lambda, const float U)
{
	if(w.z > 0.9999f)
		return false;
	if(w.z < -0.9999f) {
		C1 *= U;
		h = mf_invC1(C1);
		G1 = mf_G1(w, C1, lambda);
	}
	else if(fabsf(w.z) >= 0.0001f) {
		if(U > 1.0f - G1)
			return false;
		C1 *= powf(1.0f-U, -1.0f / lambda);
		h = mf_invC1(C1);
		G1 = mf_G1(w, C1, lambda); /* TODO(lukas): G1 = oldC1^lambda / (1-U) */
	}
	return true;
}

ccl_device float3 mf_eval(const float3 wi, const float3 wo, const float3 color, const float alpha, const float eta, float &pdf, bool swapped)
{
	if(wi.z < 1e-5f || wo.z < 1e-5f)
		return make_float3(0.0f, 0.0f, 0.0f);

	float3 wr = -wi;
	float lambda_r = mf_lambda(wr, alpha);
	float hr = 1.0f;
	float C1_r = 1.0f;
	float G1_r = 1.0f;
	float shadowing_lambda = mf_lambda(wo, alpha);

	const float3 wh = normalize(wi+wo);
	const float G2 = 1.0f / (1.0f - (lambda_r + 1.0f) + shadowing_lambda);
	const float singleScatter = color * fresnel(wi, wh, eta) * D_ggx(wh, alpha) * G2 * 0.25f / wi.z;
	float3 multiScatter = make_float3(0.0f, 0.0f, 0.0f);
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	for(int order = 0; order < 3; order++) {
		if(!mf_sample_height(wr, hr, C1_r, G1_r, lambda_r, rands[3*order]))
			break;
		if(order > 0) {
			float3 energy = mf_eval_phase(wr, lambda_r, wo, alpha, eta) * throughput;
			energy *= mf_G1(wo, saturate(0.5f*(hr + 1.0f)), shadowing_lambda);
			multiScatter += energy; /* TODO(lukas): NaN check */
		}
		if(order+1 < 3) {
			wr = mf_sample_phase(-wr, alpha, eta, throughput, make_float2(rands[3*order+1], rands[3*order+2]));
			lambda_r = mf_lambda(wr, alpha);
			throughput *= color;

			C1_r = saturate(0.5f*(hr + 1.0f));
			G1_r = mf_G1(w, C1_r, lambda_r);
			/* TODO(lukas): NaN check */
		}
	}

	pdf = 0.25f * D_ggx(normalize(wi+wo), alpha) / ((1.0f + mf_lambda(swapped? wo: wi, alpha)) * (swapped? wo.z: wi.z)) + (swapped? wi.z: wo.z);
	return singleScatter + multiScatter;
}

ccl_device float3 mf_sample(const float3 wi, float3 &wo, const float3 color, const float alpha, const float eta, float &pdf, bool swapped)
{
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	float3 wr = -wi;
	float lambda_r = mf_lambda(wr, alpha);
	float hr = 1.0f;
	float C1_r = 1.0f;
	float G1_r = 1.0f;

	int order;
	for(order = 0; order < 3; order++) {
		if(!mf_sample_height(wr, hr, C1_r, G1_r, lambda_r, rands[3*order]))
			break;
		wr = mf_sample_phase(-wr, alpha, eta, throughput, make_float2(rands[3*order+1], rands[3*order+2]));
		lambda_r = mf_lambda(wr, alpha);
		throughput *= color;

		C1_r = saturate(0.5f*(hr + 1.0f));
		G1_r = mf_G1(w, C1_r, lambda_r);
		/* TODO(lukas): NaN check */
	}
	if(order == 3 /* TODO(lukas): Or NaN */) {
		pdf = 0.0f;
		wo = make_float3(0.0f, 0.0f, 1.0f);
		return make_float3(0.0f, 0.0f, 0.0f);
	}
	pdf = 0.25f * D_ggx(normalize(wi+wr), alpha) / ((1.0f + mf_lambda(wi, alpha)) * wi.z) + wr.z;
	wo = wr;
	return throughput;
}

ccl_device int bsdf_microfacet_rough_setup(ShaderClosure *sc)
{
	sc->data0 = saturate(sc->data0); /* alpha */
	sc->data1 = max(1.0f, sc->data1); /* eta */
	sc->data2 = saturate(sc->data2); /* color */

	sc->type = CLOSURE_BSDF_MICROFACET_ROUGH_ID;

	return SD_BSDF|SD_BSDF_HAS_EVAL;
}

ccl_device float3 bsdf_microfacet_rough_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf) {
	*pdf = 0.0f;
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_microfacet_rough_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf) {
	float3 X, Y, Z;
	Z = sc->N;
	make_orthonormals(Z, &X, &Y);
	float3 localI = make_float3(dot(omega_in, X), dot(omega_in, Y), dot(omega_in, Z));
	float3 localO = make_float3(dot(I, X), dot(I, Y), dot(I, Z));
	if(localI.z < localO.z) {
		return mf_eval(localI, localO, make_float3(sc->data2, sc->data2, sc->data2), sc->data0, sc->data1, *pdf, false);
	}
	else {
		return mf_eval(localO, localI, make_float3(sc->data2, sc->data2, sc->data2), sc->data0, sc->data1, *pdf, true) * localO.z / localI.z;
	}
}

ccl_device int bsdf_microfacet_rough_sample(KernelGlobals *kg, const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float3 X, Y, Z;
	Z = sc->N;
	make_orthonormals(Z, &X, &Y);
	float3 localI = make_float3(dot(I, X), dot(I, Y), dot(I, Z));
	float3 localO;
	*eval = mf_sample(I, localO, make_float3(sc->data2, sc->data2, sc->data2), sc->data0, sc->data1, *pdf);
	*omega_in = X*localO.x + Y*localO.y + Z*localO.z;
	return LABEL_REFLECT|LABEL_GLOSSY;
}
