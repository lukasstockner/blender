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

#ifdef USE_FAST_LIGHTING
	for (int i = 0; i < b_LightCount; i++) {
		vec3 L;

		/* directional light */

		L = -b_LightSource[i].position.xyz; /* Assume this is a normalized direction vector for a sun lamp. */

		/* diffuse light */

		float NdotL = dot(N, L);

		if (NdotL > 0) {
			L_diffuse += NdotL * b_LightSource[i].diffuse.rgb;

#ifdef USE_SPECULAR
			/* specular light */

			vec3  V     = vec3(0, 0, 1); /* Assume non-local viewer. */
			vec3  H     = normalize(L + V);
			float NdotH = dot(N, H);

			L_specular += pow(max(0, NdotH), b_FrontMaterial.shininess) * b_LightSource[i].specular.rgb;
#endif
		}
	}

#else /* all 8 lights, makes no assumptions, potentially slow */

	for (int i = 0; i < b_LightCount; i++) {
		float I;
		vec3  L;

		if (b_LightSource[i].position.w == 0.0) {
			/* directional light */

			L = -b_LightSource[i].position.xyz; /* Assume this is a normalized direction vector for a sun lamp. */ 

			I = 1;
		}
		else {
			/* point light */

			vec3 VP = b_LightSource[i].position.xyz - varying_position;

			/* falloff */
			float d = length(VP);

			L = VP/d;

			/* spot light cone */
			if (b_LightSource[i].spotCutoff < 90.0) {
				float cosine = max(dot(L, -b_LightSource[i].spotDirection), 0.0);
				I = pow(cosine, b_LightSource[i].spotExponent) * step(b_LightSource[i].spotCosCutoff, cosine);
			}
			else {
				I = 1;
			}

			I /=
				b_LightSource[i].constantAttenuation +
				b_LightSource[i].linearAttenuation * distance +
				b_LightSource[i].quadraticAttenuation * distance * distance;
		}

		/* diffuse light */

		float NdotL = dot(N, L);

		if (NdotL > 0) {
			L_diffuse += I * NdotL * b_LightSource[i].diffuse.rgb;

#ifdef USE_SPECULAR
			/* specular light */

#ifdef USE_LOCAL_VIEWER
			vec3 V      = normalize(-varying_position);
#else
			vec3 V      = vec3(0, 0, 1);
#endif
			vec3 H      = normalize(L + V);
			float NdotH = dot(N, H);

			L_specular += I * pow(max(0, NdotH), b_FrontMaterial.shininess) * b_LightSource[i].specular.rgb;
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
