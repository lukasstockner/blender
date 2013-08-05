/* Options:
 *
 * USE_TEXTURE_2D: use texture for diffuse colors
 * USE_SCENE_LIGHTING: use lights (up to 8)
 * USE_SOLID_LIGHTING: assume 3 directional lights for solid draw mode
 * USE_TWO_SIDE: flip normal towards viewer
 * NO_SPECULAR: use specular component
 */



#if defined(USE_SOLID_LIGHTING) || defined(USE_SCENE_LIGHTING)

varying vec3 varying_normal;

#ifndef USE_SOLID_LIGHTING
varying vec3 varying_position;
#endif

#endif



varying vec4 varying_vertex_color;



#ifdef USE_TEXTURE
varying vec2 varying_texture_coord;
#endif



void main()
{
#if defined(USE_LIGHTING)
	/* compute normal */
	vec3 N = normalize(varying_normal);

#if defined(USE_TWO_SIDE)
	if (!gl_FrontFacing)
		N = -N;
#endif

	/* compute diffuse and specular lighting */
	vec3 L_diffuse = vec3(0.0);

#ifndef NO_SPECULAR
	vec3 L_specular = vec3(0.0);
#endif

#if defined(USE_SOLID_LIGHTING)
	/* assume 3 directional lights */
	for (int i = 0; i < b_LightCount; i++) {
		vec3 light_direction = b_LightSource[i].position.xyz;

		/* diffuse light */
		vec3 light_diffuse = b_LightSource[i].diffuse.rgb;
		float diffuse_bsdf = max(dot(N, light_direction), 0.0);
		L_diffuse += light_diffuse*diffuse_bsdf;

#if !defined(NO_SPECULAR)
		/* specular light */
		vec3 light_specular = b_LightSource[i].specular.rgb;
		vec3 H = b_LightSource[i].halfVector.xyz;

		float specular_bsdf = pow(max(dot(N, H), 0.0), b_FrontMaterial.shininess);
		L_specular += light_specular*specular_bsdf;
#endif
	}

#else

	/* all 8 lights, makes no assumptions, potentially slow */

#if !defined(NO_SPECULAR)
	/* view vector computation, depends on orthographics or perspective */
	vec3 V = (b_ProjectionMatrix[3][3] == 0.0) ? normalize(varying_position): vec3(0.0, 0.0, -1.0);
#endif

	for (int i = 0; i < b_LightCount; i++) {
		float intensity = 1.0;
		vec3 light_direction;

		if (b_LightSource[i].position.w == 0.0) {
			/* directional light */
			light_direction = b_LightSource[i].position.xyz;
		}
		else {
			/* point light */
			vec3 d = b_LightSource[i].position.xyz - varying_position;
			light_direction = normalize(d);

			/* spot light cone */
			if (b_LightSource[i].spotCutoff < 90.0) {
				float cosine = max(dot(light_direction, -b_LightSource[i].spotDirection), 0.0);
				intensity = pow(cosine, b_LightSource[i].spotExponent);
				intensity *= step(b_LightSource[i].spotCosCutoff, cosine);
			}

			/* falloff */
			float distance = length(d);

			intensity /= b_LightSource[i].constantAttenuation +
				b_LightSource[i].linearAttenuation * distance +
				b_LightSource[i].quadraticAttenuation * distance * distance;
		}

		/* diffuse light */
		vec3 light_diffuse = b_LightSource[i].diffuse.rgb;
		float diffuse_bsdf = max(dot(N, light_direction), 0.0);
		L_diffuse += light_diffuse*diffuse_bsdf*intensity;

#if !defined(NO_SPECULAR)
		/* specular light */
		vec3 light_specular = b_LightSource[i].specular.rgb;
		vec3 H = normalize(light_direction - V);

		float specular_bsdf = pow(max(dot(N, H), 0.0), b_FrontMaterial.shininess);
		L_specular += light_specular*specular_bsdf*intensity;
#endif
	}

#endif

	/* compute diffuse color, possibly from texture or vertex colors */
	float alpha;

#if defined(USE_TEXTURE)
	vec4 texture_color = texture2D(b_Sampler[0], varying_texture_coord);

	L_diffuse *= texture_color.rgb * varying_vertex_color.rgb;
	alpha = texture_color.a * varying_vertex_color.a;
#else
	L_diffuse *= varying_vertex_color.rgb;
	alpha = varying_vertex_color.a;
#endif

	/* sum lighting */
	vec3 L = L_diffuse;

#ifndef NO_SPECULAR
	L += L_specular * b_FrontMaterial.specular.rgb;
#endif

	/* write out fragment color */
	gl_FragColor = vec4(L, alpha);
#else

	/* no lighting */
#if defined(USE_TEXTURE)
	gl_FragColor = texture2D(b_Sampler[0], varying_texture_coord) * varying_vertex_color;
#else
	gl_FragColor = varying_vertex_color;
#endif

#endif
}
