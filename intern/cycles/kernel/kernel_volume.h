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

CCL_NAMESPACE_BEGIN

#ifdef __VOLUME__

#define my_rand_congruential() (lcg_step_float(rng_congruential))

#define VOLUME_PATH_TERMINATED		0
#define VOLUME_PATH_CONTINUE		1
#define VOLUME_PATH_PARTICLE_MISS	2

// probability to hit volume if far intersection exist, 50% by default
// help to speedup noise clear when tiny object or low density.
//#define __VOLUME_USE_GUARANTEE_HIT_PROB 1
#define VOLUME_GUARANTEE_HIT_PROB 0.5f

__device float sigma_from_value(float value, float geom_factor)
{
	/* return "sigma" that required to get "value" attenuation at "geom_factor" distance of media.
	to make input value resemble "alpha color" in 2d grapics , "value"=0 mean ransparent, 1 = opaque, so there is another a=1-v step.*/
#if 0
//	const float att_magic_eps = 1e-7f;
	const float att_magic_eps = 1e-15f;
	float attenuation = 1-value;
	// protect infinity nan from too big density materials
	if( attenuation < att_magic_eps) attenuation = att_magic_eps;
	return (-logf( attenuation )/geom_factor);
#else
	return value * geom_factor;
#endif
}

__device float get_sigma_sample(KernelGlobals *kg, ShaderData *sd, float randv, int path_flag, float3 p)
{
       sd->P = p;

#ifdef __MULTI_CLOSURE__
       int sampled = 0;

       // if(sd->num_closure > 1)

       const ShaderClosure *sc = &sd->closure[sampled];

       shader_eval_volume(kg, sd, randv, path_flag, SHADER_CONTEXT_MAIN);
       float v = sc->data0;
#else
       shader_eval_volume(kg, sd, randv, path_flag, SHADER_CONTEXT_MAIN);
       float v = sd->closure.data0;
#endif
       return sigma_from_value(v, kernel_data.integrator.volume_density_factor);
}


__device  float3 kernel_volume_get_final_homogeneous_extinction_tsd(KernelGlobals *kg, ShaderData *sd, float trandp, Ray ray, int path_flag)
{
	// return 3 transition extinction coefficients based on particle BRDF, base density and color
	// make sense only for homogeneous volume for now
	// the idea is to measure integral flux, not only individual free fly particles
	// it help to get color gradients even with number of min/max bounces far from infinity
	// NOTE: color must be constant as well, because we use it to modify extinction.

	float3 res_sigma = make_float3(1.0f, 1.0f, 1.0f);
	if((sd->flag & SD_HAS_VOLUME) != 0) { // check for empty volume shader
		// base sigma
		float base_sigma = get_sigma_sample(kg, sd, trandp, path_flag, ray.P/* + ray.D * start*/);

		// get transition probability
		// or flux that pass forward even if catched by particle.
		BsdfEval eval;
		float3 omega_in = -sd->I;
		float transition_pdf;
		// FIXME: true only for g=0, need some weird integral for g != 0 cases. Sure not pdf directly,
		// maybe lim(CPDF) when area -> omega_in ? Damn rocket science ...
		shader_bsdf_eval(kg, sd, omega_in, &eval, &transition_pdf);
#if 0
		transition_pdf = single_peaked_henyey_greenstein(1.0f, 0.6);
#endif
		// colors
#ifdef __MULTI_CLOSURE__
		int sampled = 0;        

		// if(sd->num_closure > 1)

		const ShaderClosure *sc = &sd->closure[sampled];

		shader_eval_volume(kg, sd, trandp, path_flag, SHADER_CONTEXT_MAIN);
		float3 color = sc->weight;
#else
		shader_eval_volume(kg, sd, trandp, path_flag, SHADER_CONTEXT_MAIN);
		float3 color = sd->closure.weight;
#endif

#if 1
//		res_sigma = make_float3(base_sigma, base_sigma, base_sigma) / ( make_float3(transition_pdf, transition_pdf, transition_pdf) * color);
//		res_sigma = make_float3(base_sigma, base_sigma, base_sigma) * ( make_float3(transition_pdf, transition_pdf, transition_pdf) * color);
//		float3 k = make_float3(1.0f, 1.0f, 1.0f) - (make_float3(transition_pdf, transition_pdf, transition_pdf) * color);
//		res_sigma = make_float3(base_sigma, base_sigma, base_sigma) * k;
//		float3 k = make_float3(1.0f, 1.0f, 1.0f) - (make_float3(transition_pdf, transition_pdf, transition_pdf) * color);
//		float3 k = (make_float3(transition_pdf, transition_pdf, transition_pdf) * color);
//		res_sigma = make_float3(base_sigma, base_sigma, base_sigma) - k;
//		res_sigma.x = base_sigma * (1.0f + logf(1.0f - transition_pdf * color.x));
//		res_sigma.y = base_sigma * (1.0f + logf(1.0f - transition_pdf * color.y));
//		res_sigma.z = base_sigma * (1.0f + logf(1.0f - transition_pdf * color.z));
		res_sigma.x = base_sigma * (1.0f + logf(1.0f + (1.0f / M_E - 1.0f) * transition_pdf * color.x));
		res_sigma.y = base_sigma * (1.0f + logf(1.0f + (1.0f / M_E - 1.0f) * transition_pdf * color.y));
		res_sigma.z = base_sigma * (1.0f + logf(1.0f + (1.0f / M_E - 1.0f) * transition_pdf * color.z));
//		printf("pdf=%g\n", transition_pdf);
#else
		res_sigma = ( make_float3(transition_pdf, transition_pdf, transition_pdf) * color);
		if( res_sigma.x > 1.0f) res_sigma.x = 1.0f;
		if( res_sigma.y > 1.0f) res_sigma.y = 1.0f;
		if( res_sigma.z > 1.0f) res_sigma.z = 1.0f;
#endif
	}
	return res_sigma;
}

/* unused */
__device float kernel_volume_homogeneous_pdf( KernelGlobals *kg, ShaderData *sd, float distance)
{
	float sigma = get_sigma_sample(kg, sd, 0, 0, make_float3(0.0f, 0.0f, 0.0f));
#ifdef __VOLUME_USE_GUARANTEE_HIT_PROB
	return sigma * exp(-distance * sigma) * VOLUME_GUARANTEE_HIT_PROB;
#else
	return sigma * exp(-distance * sigma);
#endif
}

__device float3 kernel_volume_get_final_homogeneous_extinction(KernelGlobals *kg, float trandp, int media_volume_shader)
{
	ShaderData tsd;
	Ray ray;
	ray.P = make_float3(0.0f, 0.0f, 0.0f);
	ray.D = make_float3(0.0f, 0.0f, 1.0f);
	ray.t = 0.0f;
	shader_setup_from_volume(kg, &tsd, &ray, media_volume_shader);
	int path_flag = PATH_RAY_SHADOW; // why ?

	float3 res_sigma = make_float3(1.0f, 1.0f, 1.0f);
	if((tsd.flag & SD_HAS_VOLUME) != 0) { // check for empty volume shader
		// base sigma
		float base_sigma = get_sigma_sample(kg, &tsd, trandp, path_flag, ray.P/* + ray.D * start*/);

		// get transition probability
		BsdfEval eval;
		float3 omega_in = -tsd.I;
		float transition_pdf;
		shader_bsdf_eval(kg, &tsd, omega_in, &eval, &transition_pdf);

		// colors
#ifdef __MULTI_CLOSURE__
		int sampled = 0;        

		// if(tsd.num_closure > 1)

		const ShaderClosure *sc = &tsd.closure[sampled];

		shader_eval_volume(kg, &tsd, trandp, path_flag, SHADER_CONTEXT_MAIN);
		float3 color = sc->weight;
#else
		shader_eval_volume(kg, &tsd, trandp, path_flag, SHADER_CONTEXT_MAIN);
		float3 color = tsd.closure.weight;
#endif
		res_sigma = make_float3(base_sigma, base_sigma, base_sigma) / ( make_float3(transition_pdf, transition_pdf, transition_pdf) * color);
	}
	return res_sigma;
}

__device int get_media_volume_shader(KernelGlobals *kg, float3 P, int bounce)
{
	/* check all objects that intersect random ray from given point, assume we have perfect geometry (all meshes closed, correct faces direct
	 we can calculate current volume material, assuming background as start, and reassign when we cross face */
	if (!kernel_data.integrator.use_volumetric)
		return kernel_data.background.shader;

	Ray ray;

	ray.P = P;
//	ray.D = make_float3(0.0f, 0.0f, 1.0f);
	ray.D = normalize(make_float3(0.1f, 0.5f, 0.9f)); //a bit more wild. Use random dir maybe ?
	ray.t = FLT_MAX;
	
	Intersection isect;
	int stack = 0;
//	while (scene_intersect(kg, &ray, PATH_RAY_SHADOW, &isect))
#ifdef __HAIR__ 
	while (scene_intersect(kg, &ray, 0, &isect, NULL, 0.0f, 0.0f))
#else
	while (scene_intersect(kg, &ray, 0, &isect))
#endif
	{
		ShaderData sd;
        shader_setup_from_ray(kg, &sd, &isect, &ray, bounce);
        shader_eval_surface(kg, &sd, 0.0f, 0, SHADER_CONTEXT_MAIN); // not needed ?

		if (sd.flag & SD_BACKFACING) {
			stack--;
			if (stack <= 0 && (sd.flag & SD_HAS_VOLUME))
				return sd.shader; // we are inside of object, as first triangle hit is from inside
		}
		else
			stack++; //we are outside, push stack to skip closed objects

//		ray.P = ray_offset(sd.P, -sd.Ng);
		ray.P = ray_offset(sd.P, (sd.flag & SD_BACKFACING)? sd.Ng : -sd.Ng);
		ray.t = FLT_MAX;
	}

	return kernel_data.background.shader;
}

/*used */

/* Volumetric sampling */
__device int kernel_volumetric_woodcock_sampler(KernelGlobals *kg, RNG *rng_congruential, ShaderData *sd,
	Ray ray, int path_flag, float end, float *new_t, float *pdf)
{
	/* google "woodcock delta tracker" algorithm, must be preprocessed to guess max density in volume, better keep it as close to density as possible or we got lot of tiny steps and spend milenniums marching single volume ray segment. 0.95 is good default. */
	float magic_eps = 1e-4f;

	int max_iter = kernel_data.integrator.volume_max_iterations;
	//float max_prob = kernel_data.integrator.volume_woodcock_max_density;
	//float max_sigma_t = sigma_from_value(max_prob, kernel_data.integrator.volume_density_factor);
	float max_sigma_t = 0.0f;
	
	float step = end / 10.0f; // uses 10 segments for maximum - needs parameter
	for(float s = 0.0f ; s < end ; s+= step)
		max_sigma_t = max( max_sigma_t , get_sigma_sample(kg, sd, my_rand_congruential(), path_flag, ray.P + ray.D * s));
	
	int i = 0;
	float t = 0;
	float sigma_factor = 1.0f;
	*pdf = 1.0f;

	if((end < magic_eps) || (max_sigma_t == 0))
		return 0;

	do {
		float r = my_rand_congruential();
		t += -logf( r) / max_sigma_t;
		// *pdf *= sigma_factor; // pdf that previous position was transparent pseudo-particle, obviously 1.0 for first loop step
		// *pdf *= max_sigma_t * r; // pdf of particle collision, based on conventional freefly homogeneous distance equation
	}
	while( ( sigma_factor = (get_sigma_sample(kg, sd, my_rand_congruential(), path_flag, ray.P + ray.D * t)/ max_sigma_t)) < my_rand_congruential() && 
		t < (end - magic_eps) &&
		i++ < max_iter);

	if (t < (end - magic_eps) && i <= max_iter) {
		*new_t = t;
	    sd->P = ray.P + ray.D * t;
		// *pdf *= sigma_factor; // fixme: is it necessary ?
		return 1;
	}

	// Assume rest of media up to end is homogeneous, it helps when using woodcock in outdoor scenes that tend to have continuous density.
	if ((i > max_iter) && (t < (end - magic_eps))) {
		float sigma = get_sigma_sample(kg, sd, my_rand_congruential(), path_flag, ray.P + ray.D * t);
		if( sigma < magic_eps)
			return 0;

		float r = my_rand_congruential();
		t += -logf( r) / sigma;
		*pdf *= sigma * r;
		if (t < (end - magic_eps)) {
			// double check current sigma, just to be sure we do not register event for null media.
			if(get_sigma_sample(kg, sd, my_rand_congruential(), path_flag, ray.P + ray.D * t) > magic_eps) {
				*new_t = t;
				sd->P = ray.P + ray.D * t;
				return 1;
			}
		}
	}

	return 0;
}
__device int kernel_volumetric_woodcock_sampler2(KernelGlobals *kg, RNG *rng_congruential, ShaderData *sd,
	Ray ray, int path_flag, float end, float *new_t, float *pdf)
{
	/* google "woodcock delta tracker" algorithm, must be preprocessed to guess max density in volume, better keep it as close to density as possible or we got lot of tiny steps and spend milenniums marching single volume ray segment. 0.95 is good default. */
	float magic_eps = 1e-4f;

	int max_iter = kernel_data.integrator.volume_max_iterations;
	float max_prob = kernel_data.integrator.volume_woodcock_max_density;
	float max_sigma_t = sigma_from_value(max_prob, kernel_data.integrator.volume_density_factor);
	
	int i = 0;
	float t = 0;
	float start = 0.0f;
	float sigma_factor = 1.0f;
	*pdf = 1.0f;

	if((end - start) < magic_eps)
		return 0;
	
	if(max_sigma_t == 0)
		return 0;

	do {
		float r = my_rand_congruential();
		t += -logf( r) / max_sigma_t;
		// *pdf *= sigma_factor; // pdf that previous position was transparent pseudo-particle, obviously 1.0 for first loop step
		// *pdf *= max_sigma_t * r; // pdf of particle collision, based on conventional freefly homogeneous distance equation
	}
	while( ( sigma_factor = (get_sigma_sample(kg, sd, my_rand_congruential(), path_flag, ray.P + ray.D * (t + start))/ max_sigma_t)) < my_rand_congruential() && 
		(t +start )< (end - magic_eps) &&
		i++ < max_iter);

	if ((t + start) < (end - magic_eps) && i <= max_iter) {
		*new_t = t + start;
	        sd->P = ray.P + ray.D * (t + start);
		// *pdf *= sigma_factor; // fixme: is it necessary ?
		return 1;
	}

	// *new_t = end;
#if 1
	// last chance trick, we cannot iterate infinity, but we can force to homogeneous last step after max_iter,
	// assume rest of media up to end is homogeneous, it help to use woodcock even in outdoor scenes that tend to have continuous density
	// even if vary a bit in close distance. of course it make sampling biased (not respect actual density).
	if ((i > max_iter) && ((t +start ) < (end - magic_eps))) {
		float sigma = get_sigma_sample(kg, sd, my_rand_congruential(), path_flag, ray.P + ray.D * (t + start));
		if( sigma < magic_eps) return 0;
		// t += -logf( my_rand_congruential()) / sigma;
		float r = my_rand_congruential();
		t += -logf( r) / sigma;
		*pdf *= sigma * r;
		if ((t + start) < (end - magic_eps)) {
			// double check current sigma, just to be sure we do not register event for null media.
			if( get_sigma_sample(kg, sd, my_rand_congruential(), path_flag, ray.P + ray.D * (t + start)) > magic_eps) {
				*new_t = t + start;
				sd->P = ray.P + ray.D * (t + start);
				return 1;
			}
		}
	}
#endif

	return 0;
}
__device int kernel_volumetric_marching_sampler(KernelGlobals *kg, RNG *rng_congruential, ShaderData *sd,
	Ray ray, int path_flag, float end, float *new_t, float *pdf)
{	
	int max_steps = kernel_data.integrator.volume_max_iterations;
	//float step = end != FLT_MAX ? end / max_steps : kernel_data.integrator.volume_cell_step;
	float step = kernel_data.integrator.volume_cell_step;
	
	int cell_count = 0;
	float current_cell_near_boundary_distance = 0.0f;
	float random_jitter_offset = my_rand_congruential() * step;

	*pdf = 1.0f;

	float t = 0.0f;
	float integral = 0.0f;
	float randsamp = my_rand_congruential();
	float previous_cell_average_sigma = 0.0f;
	float current_cell_average_sigma = 0.0f;

	float root = -logf(randsamp);
	float intstep = 0.0f;
	do {
		current_cell_near_boundary_distance += step;
		t = current_cell_near_boundary_distance + random_jitter_offset;
		previous_cell_average_sigma = current_cell_average_sigma;
		current_cell_average_sigma = get_sigma_sample(kg, sd, my_rand_congruential(), path_flag, ray.P + ray.D * t);
		intstep = (previous_cell_average_sigma + current_cell_average_sigma) * step * 0.5f;
		integral += intstep;
		cell_count++;
	}
	while( (integral < root) && (cell_count < max_steps) && (t < end));

	if ((cell_count >= max_steps) || (t > end)) {
		return 0;
	}

	t = current_cell_near_boundary_distance - ((integral - root) / intstep) * step;
	//*pdf = randsamp * current_cell_average_sigma;
	*new_t = t;
	sd->P = ray.P + ray.D * *new_t;
	return 1;
}

__device int kernel_volumetric_marching_sampler2(KernelGlobals *kg, RNG *rng_congruential, ShaderData *sd,
	Ray ray, int path_flag, float end, float *new_t, float *pdf)
{
	float sigma_magic_eps = 1e-15f;
	
	float step = kernel_data.integrator.volume_cell_step;
	int  max_steps = min(kernel_data.integrator.volume_max_iterations, (int)ceil(end / step));
	
	int cell_count = 0;
	float current_cell_near_boundary_distance;
	float random_jitter_offset = my_rand_congruential() * step;

	float t = 0.0f;
	do {
		current_cell_near_boundary_distance = step * (float)cell_count;
		float current_cell_average_sigma = get_sigma_sample(kg, sd, my_rand_congruential(), path_flag, ray.P + ray.D * (current_cell_near_boundary_distance + random_jitter_offset));
		if (current_cell_average_sigma < sigma_magic_eps)
			t = end + step;
		else
			t = -logf( my_rand_congruential()) / current_cell_average_sigma;
		cell_count++;
	}
	while( (t > step) && (cell_count < max_steps));

	*pdf = 1.0f;

	if ((cell_count >= max_steps) && ((current_cell_near_boundary_distance + t) > end))
		return 0;

	*new_t = current_cell_near_boundary_distance + t;
	sd->P = ray.P + ray.D * *new_t;
	return 1;
}

__device int kernel_volumetric_homogeneous_sampler(KernelGlobals *kg, float randv, float randp, ShaderData *sd,
	Ray ray, int path_flag, float end, float *new_t, float *pdf, float *eval, float *omega_cache)
{
	/* return pdf of perfect importance volume sampling at given distance
	only for homogeneous case, of course.
	TODO: cache sigma to avoid complex shader call (very CPU/GPU expensive) */
	float distance_magic_eps = 1e-4f;
	float rand_magic_eps = 0.00001f;
	float sigma_magic_eps = 1e-15f;
	
	float start = 0.0f;
	float distance = end - start;
	float sigma;

	*pdf = 1.0f; /* pdf used for importance sampling of homogeneous data, it just sigma if x=log(1-rand())/sigma used as sampling distance */
	*eval = 1.0f;
	if((distance < distance_magic_eps) || (randv  < rand_magic_eps)) {
		/* tiny volume and preventing log (0), *new_t = end */
		 return 0;
	}

#if 1
	if (*omega_cache != NULL) {
		if(*omega_cache == 0.0f) {
			*omega_cache =  get_sigma_sample(kg, sd, randp, path_flag, ray.P + ray.D * start);
		}
		sigma = *omega_cache;
	}
	else
		sigma = get_sigma_sample(kg, sd, randp, path_flag, ray.P + ray.D * start);
#else
	float3 sigma3 = kernel_volume_get_final_homogeneous_extinction_tsd(kg, sd, randp, ray, path_flag);
	sigma = min( sigma3.x, sigma3.y);
	sigma = min( sigma, sigma3.z);
#endif

	if(sigma < sigma_magic_eps) {
		/* Very transparent volume - Protect div by 0, *new_t = end; */
		return 0;
	}

#ifdef __VOLUME_USE_GUARANTEE_HIT_PROB
	/* split randv by VOLUME_GUARANTEE_HIT_PROB */
	if (randv > VOLUME_GUARANTEE_HIT_PROB) {
		// miss
		*pdf = VOLUME_GUARANTEE_HIT_PROB;
		return 0;
	}
	else {
		// assume we hit media particle, need adjust randv
		randv = 1.0f - randv / VOLUME_GUARANTEE_HIT_PROB;
		
		//*pdf = sigma * randv * VOLUME_GUARANTEE_HIT_PROB;
	}
#endif

	float sample_distance = -logf(randv) / sigma + start;
	if (sample_distance > end) { // nothing hit in between [start, end]
		//*eval = sigma * exp(-distance * sigma);
		*eval = sigma * randv;
		*pdf = sigma * randv;
		return 0;
	}
	
	// we hit particle!
	*new_t = sample_distance;
	*pdf = sigma * randv;
	*eval = sigma * randv;
	sd->P = ray.P + ray.D * sample_distance;
	return 1;
}

__device int kernel_volumetric_equiangular_sampler(KernelGlobals *kg, RNG *rng_congruential, float randv, float randp,
	ShaderData *sd, Ray ray, int path_flag, float end, float *new_t, float *pdf, float *eval, float *omega_cache)
{
	float distance_magic_eps = 1e-4f;
	float rand_magic_eps = 0.00001f;
	float sigma_magic_eps = 1e-15f;
	
	float start = 0.0f;
	float distance = end - start;
	float sigma;

	*pdf = 1.0f; /* pdf used for importance sampling of homogeneous data, it just sigma if x=log(1-rand())/sigma used as sampling distance */
	*eval = 1.0f;

	if((distance < distance_magic_eps) || (randv  < rand_magic_eps)) {
		/* tiny volume and preventing log (0), *new_t = end */
		 return 0;
	}

	/* sample a light and position on int */
	float light_t = my_rand_congruential();
	float light_u = my_rand_congruential();
	float light_v = my_rand_congruential();

	LightSample ls;
	light_sample(kg, light_t, light_u, light_v, ray.time, ray.P, &ls);
	if(ls.pdf == 0.0f)
		return 0;

	if (*omega_cache != NULL) {
		if(*omega_cache == 0.0f) {
			*omega_cache =  get_sigma_sample(kg, sd, randp, path_flag, ray.P + ray.D * start);
		}
		sigma = *omega_cache;
	}
	else
		sigma = get_sigma_sample(kg, sd, randp, path_flag, ray.P + ray.D * start);

	if( sigma < sigma_magic_eps ) {
		/*  Very transparent volume - Protect div by 0, *new_t = end; */ 
		 return 0;
	}

	float sample_distance = dot((ls.P - ray.P) , ray.D);
	float D = sqrtf(len_squared(ls.P - ray.P) - sample_distance * sample_distance);
	float atheta = atan(sample_distance / D);
	//float endtheta = atan((end - sample_distance) / D); 
	float t = D * tan((randv * M_PI_2_F) - (1 - randv) * atheta);
	sample_distance += t;

	*pdf = D / ((M_PI_2_F + atheta) * (D * D + t * t));
	*eval = *pdf;
	if(sample_distance > end)
		return 0;
	else
		/* we hit particle */
		*new_t = sample_distance;
		sd->P = ray.P + ray.D * sample_distance;
		return 1;
}

__device int kernel_volumetric_sample(KernelGlobals *kg, RNG *rng, int rng_offset, RNG *rng_congruential, int pass, float randv, float randp,
	ShaderData *sd, Ray ray, float distance, float *particle_isect_t, int path_flag, float *pdf, float *eval, float3 *throughput, float *omega_cache = NULL)
{
	/* sample point on volumetric ray (return false - no hit, true - hit : fill new hit t value on path [start,end] */
	float distance_magic_eps = 1e-4f;

	if((sd->flag & SD_HAS_VOLUME) == 0 || (distance < distance_magic_eps))
		return 0; /* empty volume shader slot or escape from bottle when scattering in solid */

	*pdf = 1.0f;
	*eval = 1.0f;
	*particle_isect_t = 0.0f;

	if(sd->flag & SD_HOMOGENEOUS_VOLUME) {
		/* homogeneous media */
		if (kernel_data.integrator.volume_homogeneous_sampling == 1 && kernel_data.integrator.num_all_lights) {
			bool ok = kernel_volumetric_equiangular_sampler(kg, rng_congruential, randv, randp, sd, ray, path_flag, distance, particle_isect_t, pdf, eval,  omega_cache);
			return ok;
		}
		else {
			bool ok = kernel_volumetric_homogeneous_sampler(kg, randv, randp, sd, ray, path_flag, distance, particle_isect_t, pdf, eval, omega_cache);
			return ok;
		}
	}
	else {
		if (kernel_data.integrator.volume_sampling_algorithm == 3) {
			/* Woodcock delta tracking */
			bool ok = kernel_volumetric_woodcock_sampler(kg, rng_congruential, sd, ray, path_flag, distance, particle_isect_t, pdf);
			*eval = *pdf;
			return ok;
		}
		else if (kernel_data.integrator.volume_sampling_algorithm == 2){
			/* Volume marching. Move particles through one region at a time, until collision occurs */
			bool ok = kernel_volumetric_marching_sampler(kg, rng_congruential, sd, ray, path_flag, distance, particle_isect_t, pdf);
			*eval = *pdf;
			return ok;
		}
		else if (kernel_data.integrator.volume_sampling_algorithm == 1){
			/* Woodcock delta tracking */
			bool ok = kernel_volumetric_woodcock_sampler2(kg, rng_congruential, sd, ray, path_flag, distance, particle_isect_t, pdf);
			*eval = *pdf;
			return ok;
		}
		else {
			/* Volume marching. Move particles through one region at a time, until collision occurs */
			bool ok = kernel_volumetric_marching_sampler2(kg, rng_congruential, sd, ray, path_flag, distance, particle_isect_t, pdf);
			*eval = *pdf;
			return ok;
		}
	}
}

/* Volumetric shadows */
__device float3 kernel_volume_get_shadow_attenuation(KernelGlobals *kg, RNG *rng, int rng_offset, RNG *rng_congruential, int sample,
	Ray *light_ray, int media_volume_shader, float *volume_pdf)
{
	// helper for shadow probes, optimised for homogeneous volume, variable density other return 0 or 1.
	// assume there are no objects inside light_ray, so it mus be preceded by shdow_blocked() or scene_intersect().
	float3 attenuation = make_float3(1.0f, 1.0f, 1.0f);
	*volume_pdf = 1.0f;

	if(!kernel_data.integrator.use_volumetric)
		return attenuation;

	ShaderData tsd;
	shader_setup_from_volume(kg, &tsd, light_ray, media_volume_shader);
	if((tsd.flag & SD_HAS_VOLUME) != 0) { // check for empty volume shader
		float trandv = path_rng(kg, rng, sample, rng_offset + PRNG_VOLUME_SHADOW_DISTANCE);
		float trandp = path_rng(kg, rng, sample, rng_offset + PRNG_VOLUME_SHADOW_DENSITY);
		float tparticle_isect_t;
		float tpdf;
		float teval;
		float3 tthroughput = make_float3(1.0f, 1.0f, 1.0f);
		attenuation = make_float3(1.0f, 1.0f, 1.0f);
		if(tsd.flag & SD_HOMOGENEOUS_VOLUME) {
			// special case
			float sigma = get_sigma_sample(kg, &tsd, trandp, PATH_RAY_SHADOW, light_ray->P/* + ray.D * start*/);
			if (sigma < 0.0f) sigma = 0.0f;
//			sigma = 1.0f;
#if 0
			// get transition probability
			BsdfEval eval;
			float3 omega_in = -tsd.I;
			float transition_pdf;
			shader_bsdf_eval(kg, &tsd, omega_in, &eval, &transition_pdf);
//			sigma /= 1.0f / 4 * M_PI; //diffusion?
			sigma /= transition_pdf; //diffusion?
#endif
			float magic_eps = 0.00001f;
//			if ( light_ray->t < magic_eps)
			if ( light_ray->t < magic_eps || (sigma < 0.00001f))
				attenuation = make_float3(1.0f, 1.0f, 1.0f);
			else {
				*volume_pdf =  sigma * exp(-light_ray->t * sigma);
#if 0
				float a = exp(-light_ray->t * sigma);
				attenuation = make_float3(a, a, a);
#else
				// maybe it incorrect, but in homogeneous and constant color media we can analytically
				// calculate transition color as well, based on knowledge of phase function probability
				// in front direction, and integral of contributions of all possible combinations
				// of direct light along straight line.
				// (actually it only my guess, need to search correct integral, i assume it exp(-distance*sigma),
				// remember we need complete unbiased multiscattering solution, not fake with 3 different color densities)
				// In other words, try to calculate integral assuming Max Bounce = infinity along line.
				// warning, require Color = constant as well as Homogeneous
				// TODO: ensure that shader have color input unplugged.
//				float3 color = make_float3(0.2f, 0.8f, 0.8f);
//				float3 color = make_float3(0.5f, 0.99f, 0.99f);
//				float3 tr_color = make_float3(1.0f, 1.0f, 1.0f) - color;
//				float3 tr_color = color;
#if 0
				attenuation.x = exp(-light_ray->t * sigma / (transition_pdf * tr_color.x) );
				attenuation.y = exp(-light_ray->t * sigma / (transition_pdf * tr_color.y) );
				attenuation.z = exp(-light_ray->t * sigma / (transition_pdf * tr_color.z) );
#else
//				float3 sigma3 = kernel_volume_get_final_homogeneous_extinction_tsd(kg, &tsd, trandp, *light_ray, PATH_RAY_SHADOW);
#if 1
//				attenuation.x = exp(-light_ray->t * sigma3.x);
//				attenuation.y = exp(-light_ray->t * sigma3.y);
//				attenuation.z = exp(-light_ray->t * sigma3.z);
				attenuation.x = exp(-light_ray->t * sigma);
				attenuation.y = exp(-light_ray->t * sigma);
				attenuation.z = exp(-light_ray->t * sigma);
#else
//				attenuation.x = exp(-light_ray->t * sigma3.x) + exp(-light_ray->t * sigma);
//				attenuation.y = exp(-light_ray->t * sigma3.y) + exp(-light_ray->t * sigma);
//				attenuation.z = exp(-light_ray->t * sigma3.z) + exp(-light_ray->t * sigma);
				attenuation.x = exp(-light_ray->t * sigma) + (1.0f - exp(-light_ray->t * sigma)) * sigma3.x;
				attenuation.y = exp(-light_ray->t * sigma) + (1.0f - exp(-light_ray->t * sigma)) * sigma3.y;
				attenuation.z = exp(-light_ray->t * sigma) + (1.0f - exp(-light_ray->t * sigma)) * sigma3.z;
#endif
#endif
#endif
			}

		}
		else {
			if(!kernel_volumetric_sample(kg, rng, rng_offset, rng_congruential, sample, trandv, trandp, &tsd, *light_ray, light_ray->t, &tparticle_isect_t, PATH_RAY_SHADOW, &tpdf, &teval, &tthroughput))
				attenuation = make_float3(1.0f, 1.0f, 1.0f);
			else {
				*volume_pdf = 0.0f;
				attenuation = make_float3(0.0f, 0.0f, 0.0f);
			}
		}

	}
	else
		attenuation = make_float3(1.0f, 1.0f, 1.0f);

	return attenuation;
}



__device bool shadow_blocked_new(KernelGlobals *kg, RNG *rng, int rng_offset, RNG *rng_congruential, int sample, PathState *state,
	Ray *ray, float3 *shadow, int media_volume_shader, float *volume_pdf)
{
	*shadow = make_float3(1.0f, 1.0f, 1.0f);
	*volume_pdf = 1.0f;
	float tmp_volume_pdf;

	if(ray->t == 0.0f)
		return false;

	uint visibility = path_state_ray_visibility(kg, state);
	visibility |= PATH_RAY_SHADOW_OPAQUE;
	Intersection isect;
#ifdef __HAIR__ 
//	bool result = scene_intersect(kg, ray, PATH_RAY_SHADOW_OPAQUE, &isect);
	bool result = scene_intersect(kg, ray, visibility, &isect, NULL, 0.0f, 0.0f);
#else
//	bool result = scene_intersect(kg, ray, PATH_RAY_SHADOW_OPAQUE, &isect);
	bool result = scene_intersect(kg, ray, visibility, &isect);
#endif

#ifdef __TRANSPARENT_SHADOWS__
	if(result && kernel_data.integrator.transparent_shadows) {
		/* transparent shadows work in such a way to try to minimize overhead
		 * in cases where we don't need them. after a regular shadow ray is
		 * cast we check if the hit primitive was potentially transparent, and
		 * only in that case start marching. this gives on extra ray cast for
		 * the cases were we do want transparency.
		 *
		 * also note that for this to work correct, multi close sampling must
		 * be used, since we don't pass a random number to shader_eval_surface */
		if(shader_transparent_shadow(kg, &isect)) {
			float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
			float3 Pend = ray->P + ray->D*ray->t;
			int bounce = state->transparent_bounce;

			for(;;) {
				if(bounce >= kernel_data.integrator.transparent_max_bounce) {
					*shadow = make_float3(1.0f, 1.0f, 1.0f);
					*volume_pdf = 1.0f;
					return true;
				}
				else if(bounce >= kernel_data.integrator.transparent_min_bounce) {
					/* todo: get random number somewhere for probabilistic terminate */
#if 0
					float probability = average(throughput);
					float terminate = 0.0f;

					if(terminate >= probability)
						return true;

					throughput /= probability;
#endif
				}
#ifdef __HAIR__
				if(!scene_intersect(kg, ray, visibility, &isect, NULL, 0.0f, 0.0f)) {
#else
//				if(!scene_intersect(kg, ray, PATH_RAY_SHADOW_TRANSPARENT, &isect)) {
				if(!scene_intersect(kg, ray, visibility, &isect)) {
#endif
					float3 attenuation = kernel_volume_get_shadow_attenuation(kg, rng, rng_offset, rng_congruential, sample, ray, media_volume_shader, &tmp_volume_pdf);
					throughput *= attenuation;

					*shadow *= throughput;
					*volume_pdf *= tmp_volume_pdf;
					return false;
				}

				if(!shader_transparent_shadow(kg, &isect)) {
//					*shadow = make_float3(1.0f, 1.0f, 1.0f);
					*shadow = make_float3(0.0f, 0.0f, 0.0f); // black 
					*volume_pdf = 1.0f;
					return true;
				}

				Ray v_ray = *ray;
				v_ray.t = isect.t;
				float3 attenuation = kernel_volume_get_shadow_attenuation(kg, rng, rng_offset, rng_congruential, sample, &v_ray, media_volume_shader, &tmp_volume_pdf);
				*volume_pdf *= tmp_volume_pdf;

				ShaderData sd;
				shader_setup_from_ray(kg, &sd, &isect, ray, state->bounce);
				shader_eval_surface(kg, &sd, 0.0f, PATH_RAY_SHADOW, SHADER_CONTEXT_MAIN);

				throughput *= shader_bsdf_transparency(kg, &sd) * attenuation;

				ray->P = ray_offset(sd.P, -sd.Ng);
				if(ray->t != FLT_MAX)
					ray->D = normalize_len(Pend - ray->P, &ray->t);

				bounce++;

//				swap_media();
				if (media_volume_shader == kernel_data.background.shader)
					media_volume_shader = sd.shader;
				else
					media_volume_shader = kernel_data.background.shader;

			}
		}
	}
#endif

	if(! result) {
		float3 attenuation = kernel_volume_get_shadow_attenuation(kg, rng, rng_offset, rng_congruential, sample, ray, media_volume_shader, &tmp_volume_pdf);
		*shadow *= attenuation;
		*volume_pdf *= tmp_volume_pdf;
	}

	return result;
}


/* volumetric tracing */
__device int kernel_path_trace_volume(KernelGlobals *kg, RNG *rng, int rng_offset, RNG *rng_congruential, int sample,
	Ray *ray, Intersection *isect, float isect_t, PathState *state, int media_volume_shader, float3 *throughput,
	PathRadiance *L,  __global float *buffer, float *ray_pdf, float3 *volume_eval, float *volume_pdf, float *omega_cache)
{
	// we sampling volume using different algorithms, respect importance sampling
	*volume_pdf = 1.0f;
	*volume_eval = make_float3( *volume_pdf, *volume_pdf, *volume_pdf);

	if (!kernel_data.integrator.use_volumetric)
		return VOLUME_PATH_PARTICLE_MISS;

	ShaderData vsd;
	shader_setup_from_volume(kg, &vsd, ray, media_volume_shader);
	if ((vsd.flag & SD_HAS_VOLUME) == 0)
		return VOLUME_PATH_PARTICLE_MISS; // null volume slot, assume transparent.

	float randv = path_rng(kg, rng, sample, rng_offset + PRNG_VOLUME_DISTANCE);
	float randp = path_rng(kg, rng, sample, rng_offset + PRNG_VOLUME_DENSITY);
	float particle_isect_t;
	float pdf;
	float eval;
	if (kernel_volumetric_sample(kg, rng, rng_offset, rng_congruential, sample, randv, randp, &vsd,
		*ray, isect_t, &particle_isect_t, state->flag, &pdf, &eval, throughput, omega_cache)) {
		
		*volume_pdf = pdf;
		*volume_eval = make_float3( eval, eval, eval);
		//if (vsd.flag & SD_HOMOGENEOUS_VOLUME)
		//	*volume_eval = make_float3( *volume_pdf, *volume_pdf, *volume_pdf);  // perfect importance sampling for homogeneous

		kernel_write_data_passes(kg, buffer, L, &vsd, sample, state->flag, *throughput);

#ifdef __EMISSION__
		/* emission */
		if(vsd.flag & SD_EMISSION) {
//			float3 emission = indirect_emission(kg, &vsd, particle_isect_t, state->flag, *ray_pdf) / pdf;
//			float3 emission = indirect_emission(kg, &vsd, particle_isect_t, state->flag, *ray_pdf, *volume_pdf) / pdf;
//			float3 emission = indirect_emission(kg, &vsd, particle_isect_t, state->flag, *ray_pdf, *volume_pdf);
			float3 emission = indirect_primitive_emission(kg, &vsd, particle_isect_t, state->flag, *ray_pdf, *volume_pdf);
			path_radiance_accum_emission(L, *throughput, emission, state->bounce);
		}
#endif

		/* path termination. this is a strange place to put the termination, it's
		   mainly due to the mixed in MIS that we use. gives too many unneeded
		   shader evaluations, only need emission if we are going to terminate */
		float probability = path_state_terminate_probability(kg, state, *throughput);
		float terminate = path_rng(kg, rng, sample, rng_offset + PRNG_VOLUME_TERMINATE);

		if(terminate >= probability)
			return VOLUME_PATH_TERMINATED;

		*throughput /= probability;

#ifdef __EMISSION__
		if(kernel_data.integrator.use_direct_light) {
			/* sample illumination from lights to find path contribution */
			if(vsd.flag & SD_BSDF_HAS_EVAL) {
				float light_t = path_rng(kg, rng, sample, rng_offset + PRNG_LIGHT);
				float light_o = path_rng(kg, rng, sample, rng_offset + PRNG_LIGHT_F);
				float light_u = path_rng(kg, rng, sample, rng_offset + PRNG_LIGHT_U);
				float light_v = path_rng(kg, rng, sample, rng_offset + PRNG_LIGHT_V);

				Ray light_ray;
				BsdfEval L_light;
//				int lamp;
				bool is_lamp;

#ifdef __OBJECT_MOTION__
                                light_ray.time = vsd.time;
#endif

#ifdef __MULTI_LIGHT__ /* ToDo: Fix, Branched Path trace feature */
				/* index -1 means randomly sample from distribution */
				int i = (kernel_data.integrator.num_distribution)? -1: 0;

				for(; i < kernel_data.integrator.num_all_lights; i++) {
#else
				const int i = -1;
#endif

					if(direct_emission(kg, &vsd, i, light_t, light_o, light_u, light_v, &light_ray, &L_light, &is_lamp, state->bounce)) {
						/* trace shadow ray */
						float3 shadow;
						float tmp_volume_pdf;

						if(!shadow_blocked_new(kg, rng, rng_offset, rng_congruential, sample, state, &light_ray, &shadow, media_volume_shader, &tmp_volume_pdf)) {
							/* accumulate */
//							bool is_lamp = (lamp != ~0);
							path_radiance_accum_light(L, *throughput, &L_light, shadow, 1.0f, state->bounce, is_lamp);
						}
					}
#ifdef __MULTI_LIGHT__
				}
#endif
			}
		}
#endif
		/* sample BRDF */
		float bsdf_pdf;
		BsdfEval bsdf_eval;
		float3 bsdf_omega_in;
		differential3 bsdf_domega_in;
		float bsdf_u = path_rng(kg, rng, sample, rng_offset + PRNG_VOLUME_BRDF_U);
		float bsdf_v = path_rng(kg, rng, sample, rng_offset + PRNG_VOLUME_BRDF_V);
		int label;

		label = shader_volume_bsdf_sample(kg, &vsd, bsdf_u, bsdf_v, &bsdf_eval,
			&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

		if(bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval) || pdf == 0.0f)
			return VOLUME_PATH_TERMINATED;

		/* modify throughput */
		bsdf_eval_mul(&bsdf_eval, (*volume_eval)/(*volume_pdf));

		path_radiance_bsdf_bounce(L, throughput, &bsdf_eval, bsdf_pdf, state->bounce, label);

		/* set labels */
#if defined(__EMISSION__) || defined(__BACKGROUND__)
		*ray_pdf = bsdf_pdf * (*volume_pdf);
#endif

		/* update path state */
		path_state_next(kg, state, label);

		/* setup ray */
//		ray.P = ray_offset(sd.P, (label & LABEL_TRANSMIT)? -vsd.Ng: vsd.Ng);
		ray->P = vsd.P;
		ray->D = bsdf_omega_in;
		ray->t = FLT_MAX;
#ifdef __RAY_DIFFERENTIALS__
		ray->dP = vsd.dP;
		ray->dD = bsdf_domega_in;
#endif
		return VOLUME_PATH_CONTINUE;

	}
	*volume_pdf = pdf;
	//*volume_eval = make_float3( 1.0f, 1.0f, 1.0f);
	*volume_eval = make_float3( eval, eval, eval);
	//*volume_eval = make_float3( *volume_pdf, *volume_pdf, *volume_pdf);  // perfect importance sampling for homogeneous

	// even if we missed any volume particle and hit face after it, we still modify color
	// by transition attenuation, to respect "leaked" light because of scattering in strong forward direction
	if (vsd.flag & SD_HOMOGENEOUS_VOLUME)
	{
		//int path_flag = 0;
	
		//float sigma = get_sigma_sample(kg, &vsd, randp, PATH_RAY_SHADOW, ray->P/* + ray.D * start*/);
		//if (sigma < 0.0f) sigma = 0.0f;
		//float3 sigma3 = kernel_volume_get_final_homogeneous_extinction_tsd(kg, &vsd, randp, *ray, path_flag);
		//float3 attenuation;
//		attenuation.x = exp(-isect_t* sigma3.x);
//		attenuation.y = exp(-isect_t* sigma3.y);
//		attenuation.z = exp(-isect_t* sigma3.z);
//		bsdf_eval_mul(&bsdf_eval, attenuation);
//		*throughput *= attenuation;
//		*throughput += attenuation;
//		attenuation.x = (1.0f - exp(-isect_t* sigma)) * sigma3.x;
//		attenuation.y = (1.0f - exp(-isect_t* sigma)) * sigma3.y;
//		attenuation.z = (1.0f - exp(-isect_t* sigma)) * sigma3.z;
		//attenuation.x = exp(-isect_t* sigma3.x) - exp(-isect_t* sigma);
		//attenuation.y = exp(-isect_t* sigma3.y) - exp(-isect_t* sigma);
		//attenuation.z = exp(-isect_t* sigma3.z) - exp(-isect_t* sigma);
//		*throughput *= (make_float3(1.0f, 1.0f, 1.0f) + attenuation);
		//*volume_eval = make_float3( *volume_pdf, *volume_pdf, *volume_pdf);  // perfect importance sampling for homogeneous
	}

	return VOLUME_PATH_PARTICLE_MISS;
}

#endif

CCL_NAMESPACE_END
