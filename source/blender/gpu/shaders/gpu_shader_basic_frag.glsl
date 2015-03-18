/* Options:

   USE_LIGHTING
   USE_FAST_LIGHTING
   USE_TWO_SIDE
   USE_SPECULAR
   USE_LOCAL_VIEWER
   USE_TEXTURE_2D

*/



#define USE_SPECULAR



#ifdef USE_LIGHTING

varying vec3 varying_normal;
varying vec3 varying_half_vector;

#ifndef USE_FAST_LIGHTING
varying vec3 varying_position;
#endif

#endif



varying vec4 varying_color;



#ifdef USE_TEXTURE_2D
varying vec2 varying_texcoord;
#endif



void main()
{
#ifdef USE_LIGHTING
	/* compute normal */
	vec3 N = normalize(varying_normal);

#ifdef USE_TWO_SIDE
	if (!gl_FrontFacing)
		N = -N;
#endif

	/* compute diffuse and specular lighting */
	vec3 L_diffuse  = vec3(0,0,0);

#ifdef USE_SPECULAR
	vec3 L_specular = vec3(0,0,0);
#endif
	vec3 debug;

#ifdef USE_FAST_LIGHTING
	for (int i = 0; i < b_LightCount; i++) {
		vec3 VP;

		/* directional light */

		VP = b_LightSource[i].position.xyz; /* Assume this is a normalized direction vector for a sun lamp. */

		/* diffuse light */

		float NdotVP = dot(N, VP);

		if (NdotVP > 0.0) {
			L_diffuse += NdotVP * b_LightSource[i].diffuse.rgb;

#ifdef USE_SPECULAR
			/* specular light */

			vec3  VE     = vec3(0, 0, 1); /* Assume non-local viewer. */
			vec3  HV     = normalize(VP + VE);
			float NdotHV = dot(N, HV);

			if (NdotHV > 0.0)
				L_specular += pow(NdotHV, b_FrontMaterial.shininess) * b_LightSource[i].specular.rgb;
#endif
		}
	}

#else /* all 8 lights, makes no assumptions, potentially slow */

	for (int i = 0; i < 1/*b_LightCount*/; i++) {
		float I;
		vec3  VP;

		if (b_LightSource[i].position.w == 0.0) {
			/* directional light */

			VP = b_LightSource[i].position.xyz; /* Assume this is a normalized direction vector for a sun lamp. */

			I = 1;
		}
		else {
			/* point light */

			VP = b_LightSource[i].position.xyz - varying_position;

			float d = length(VP);

			VP /= d;

			/* spot light cone */
			if (b_LightSource[i].spotCutoff < 90.0) {
				float cosine = max(dot(VP, -b_LightSource[i].spotDirection), 0.0);
				I = pow(cosine, b_LightSource[i].spotExponent) * step(b_LightSource[i].spotCosCutoff, cosine);
			}
			else {
				I = 1;
			}

			I /=
				b_LightSource[i].constantAttenuation           +
				b_LightSource[i].linearAttenuation    * d      +
				b_LightSource[i].quadraticAttenuation * d * d;
		}

		float NdotVP = dot(N, VP);

		if (NdotVP > 0) {
			L_diffuse += I * NdotVP * b_LightSource[i].diffuse.rgb;

#ifdef USE_SPECULAR
			/* specular light */

#ifdef USE_LOCAL_VIEWER
			vec3  VE     = normalize(-varying_position);
#else
			vec3  VE     = vec3(0, 0, 1);
#endif
			vec3  HV     = normalize(VP + VE); /* Assumes VP and VE were normalized already. */
			float NdotHV = dot(N, HV);

			if (NdotHV > 0)
				L_specular += I * pow(NdotHV, b_FrontMaterial.shininess) * b_LightSource[i].specular.rgb;
#endif
		}
	}

#endif /* fast or regular lighting */

	/* compute diffuse color, possibly from texture or vertex colors */
	float alpha;

#ifdef USE_TEXTURE_2D
	vec4 texture_color = texture2D(b_Sampler2D[0], varying_texcoord);

	L_diffuse *= texture_color.rgb * varying_color.rgb;
	alpha = texture_color.a * varying_color.a;
#else
	L_diffuse *= varying_color.rgb;
	alpha = varying_color.a;
#endif

	/* sum lighting */
	vec3 L_total = L_diffuse;

#ifdef USE_SPECULAR
	L_total += L_specular * b_FrontMaterial.specular.rgb;
#endif

	/* write out fragment color */
	gl_FragColor = vec4(L_total, alpha);

#else /* no lighting */

#ifdef USE_TEXTURE_2D
	gl_FragColor = texture2D(b_Sampler2D[0], varying_texcoord) * varying_color;
#else
	gl_FragColor = varying_color;
#endif

#endif
}