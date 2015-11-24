CCL_NAMESPACE_BEGIN

ccl_device_inline float fresnel(const float3 wi, const float3 wm, const float eta)
{
	const float cosI = dot(wi, wm);
	const float cosT2 = 1.0f - (1.0f - cosI*cosI) / (eta*eta);
	if(cosT2 <= 0.0f)
		return 1.0f;
	const float cosT = sqrtf(cosT2);
	const float Rs = (cosI - eta * cosT) / (cosI + eta * cosT);
	const float Rp = (eta * cosI - cosT) / (eta * cosT + cosT);
	return 0.5f * (Rs*Rs + Rp*Rp);
}

ccl_device_inline float D_ggx(float3 wm, const float alpha)
{
	float invZ = 1.0f / wm.z;
	wm.x *= invZ;
	wm.y *= invZ;
	wm.z *= wm.z;
	wm.z *= wm.z;
	float val = 1.0f + (wm.x*wm.x + wm.y*wm.y) / (alpha*alpha);
	val = M_PI_F * alpha*alpha * val*val;
	return 1.0f / (val * wm.z);
}

ccl_device_inline float2 mf_sampleP22_11(const float cosI, const float2 randU, const float alpha)
{
	if(cosI > 0.9999f) {
		const float r = sqrtf(randU.x / (1.0f - randU.x));
		const float phi = M_2PI_F * randU.y;
		return make_float2(r*cosf(phi), r*sinf(phi));
	}

	const float sinI = sqrtf(1.0f - cosI*cosI);
	const float tanI = sinI/cosI;
	const float projA = 0.5f * (cosI + 1.0f);
	if(projA < 0.0001f)
		return make_float2(0.0f, 0.0f);
	const float c = 1.0f / projA;
	const float A = 2.0f*randU.x / (cosI*c) - 1.0f;
	const float tmp = 1.0f / (A*A-1.0f);
	const float D = safe_sqrtf(tanI*tanI*tmp*tmp - (A*A-tanI*tanI)*tmp);

	const float slopeX2 = tanI*tmp + D;
	const float slopeX = (A < 0.0f || slopeX2 > 1.0f/tanI)? (tanI*tmp - D) : slopeX2;

	float U2;
	if(randU.y >= 0.5f)
		U2 = 2.0f*(randU.y - 0.5f);
	else
		U2 = 2.0f*(0.5f - randU.y);
	const float z = (U2*(U2*(U2*0.27385f-0.73369f)+0.46341f)) / (U2*(U2*(U2*0.093073f+0.309420f)-1.0f)+0.597999f);
	const float slopeY = z * sqrtf(1.0f + slopeX*slopeX);

	if(randU.y >= 0.5f)
		return make_float2(slopeX, slopeY);
	else
		return make_float2(slopeX, -slopeY);
}

ccl_device_inline float3 mf_sample_phase(const float3 wi, const float alpha, const float eta, float3 &weight, const float2 randU)
{
	const float3 wi_11 = normalize(make_float3(alpha*wi.x, alpha*wi.y, wi.z));
	const float2 slope_11 = mf_sampleP22_11(wi_11.z, randU, alpha);

	//const float phi = atan2f(wi_11.y, wi_11.x);
	const float2 cossin_phi = normalize(make_float2(wi_11.x, wi_11.y));//wi_11 / len(wi_11);
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
		return 0.0f;
	return fresnel(-w, wh, eta) * D_ggx(wh, alpha) * 0.25f / projArea;
}

ccl_device_inline float mf_lambda(const float3 w, const float alpha)
{
	if(w.z > 0.9999f)
		return 0.0f;
	else if(w.z < -0.9999f)
		return -1.0f;

	float invA = alpha * w.z / sqrtf(1.0f - w.z*w.z);
	return 0.5f*(((invA>0)?1.0f:-1.0f) * sqrtf(1.0f + invA*invA));
}

ccl_device_inline float mf_invC1(const float h)
{
	return 2.0f * saturate(h) - 1.0f;
}

ccl_device_inline float mf_C1(const float h)
{
	return saturate(0.5f * (h + 1.0f));
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

ccl_device float3 mf_eval(const float3 wi, const float3 wo, const float3 color, const float alpha, const float eta, float &pdf, const uint seed, bool swapped)
{
	if(wi.z < 1e-5f || wo.z < 1e-5f)
		return make_float3(0.0f, 0.0f, 0.0f);

	uint rng = lcg_init(seed);

	float3 wr = -wi;
	float lambda_r = mf_lambda(wr, alpha);
	float hr = 1.0f;
	float C1_r = 1.0f;
	float G1_r = 1.0f;
	float shadowing_lambda = mf_lambda(wo, alpha);

	const float3 wh = normalize(wi+wo);
	const float G2 = 1.0f / (1.0f - (lambda_r + 1.0f) + shadowing_lambda);
	const float3 singleScatter = color * fresnel(wi, wh, eta) * D_ggx(wh, alpha) * G2 * 0.25f / wi.z;
	float3 multiScatter = make_float3(0.0f, 0.0f, 0.0f);
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	for(int order = 0; order < 3; order++) {
		if(!mf_sample_height(wr, hr, C1_r, G1_r, lambda_r, lcg_step_float(&rng)))
			break;
		if(order > 0) {
			float3 energy = mf_eval_phase(wr, lambda_r, wo, alpha, eta) * throughput;
			energy *= mf_G1(wo, mf_C1(hr), shadowing_lambda);
			multiScatter += energy; /* TODO(lukas): NaN check */
		}
		if(order+1 < 3) {
			wr = mf_sample_phase(-wr, alpha, eta, throughput, make_float2(lcg_step_float(&rng), lcg_step_float(&rng)));
			lambda_r = mf_lambda(wr, alpha);
			throughput *= color;

			C1_r = mf_C1(hr);
			G1_r = mf_G1(wr, C1_r, lambda_r);
			/* TODO(lukas): NaN check */
		}
	}

	pdf = 0.25f * D_ggx(normalize(wi+wo), alpha) / ((1.0f + mf_lambda(swapped? wo: wi, alpha)) * (swapped? wo.z: wi.z)) + (swapped? wi.z: wo.z);
	return singleScatter + multiScatter;
}

ccl_device float3 mf_sample(const float3 wi, float3 &wo, const float3 color, const float alpha, const float eta, float &pdf, const uint seed)
{
	uint rng = lcg_init(seed);

	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	float3 wr = -wi;
	float lambda_r = mf_lambda(wr, alpha);
	float hr = 1.0f;
	float C1_r = 1.0f;
	float G1_r = 1.0f;

	int order;
	for(order = 0; order < 3; order++) {
		if(!mf_sample_height(wr, hr, C1_r, G1_r, lambda_r, lcg_step_float(&rng)))
			break;
		wr = mf_sample_phase(-wr, alpha, eta, throughput, make_float2(lcg_step_float(&rng), lcg_step_float(&rng)));
		lambda_r = mf_lambda(wr, alpha);
		throughput *= color;

		C1_r = mf_C1(hr);
		G1_r = mf_G1(wr, C1_r, lambda_r);
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

ccl_device_inline uint hash_float3(const float3 hash)
{
	return __float_as_uint(hash.x) ^ __float_as_uint(hash.y) ^ __float_as_uint(hash.z);
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
	uint seed = hash_float3(I) ^ hash_float3(omega_in) ^ hash_float3(make_float3(sc->data0, sc->data1, sc->data2));
	if(localI.z < localO.z) {
		return mf_eval(localI, localO, make_float3(sc->data2, sc->data2, sc->data2), sc->data0, sc->data1, *pdf, seed, false);
	}
	else {
		return mf_eval(localO, localI, make_float3(sc->data2, sc->data2, sc->data2), sc->data0, sc->data1, *pdf, seed, true) * localO.z / localI.z;
	}
}

ccl_device int bsdf_microfacet_rough_sample(KernelGlobals *kg, const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float3 X, Y, Z;
	Z = sc->N;
	make_orthonormals(Z, &X, &Y);
	float3 localI = make_float3(dot(I, X), dot(I, Y), dot(I, Z));
	float3 localO;
	*eval = mf_sample(I, localO, make_float3(sc->data2, sc->data2, sc->data2), sc->data0, sc->data1, *pdf, __float_as_uint(randu));
	*omega_in = X*localO.x + Y*localO.y + Z*localO.z;
	return LABEL_REFLECT|LABEL_GLOSSY;
}

CCL_NAMESPACE_END
