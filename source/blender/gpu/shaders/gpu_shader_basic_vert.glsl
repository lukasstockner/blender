/* Options:

   USE_LIGHTING
   USE_FAST_LIGHTING
   USE_TWO_SIDE
   USE_SPECULAR
   USE_LOCAL_VIEWER
   USE_TEXTURE_2D

*/



#ifdef USE_LIGHTING

varying vec3 varying_normal;

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
	vec4 co = b_ModelViewMatrix * b_Vertex;

#ifdef USE_LIGHTING
	varying_normal = normalize(b_NormalMatrix * b_Normal);

#ifndef USE_FAST_LIGHTING
	varying_position = co.xyz;
#endif

#endif

	gl_Position = b_ProjectionMatrix * co;

#if defined(GPU_PROFILE_COMPAT) && defined(GPU_NVIDIA)
	// Setting gl_ClipVertex is necessary to get glClipPlane working on NVIDIA
	// graphic cards, while on ATI it can cause a software fallback.
	gl_ClipVertex = co;
#endif

	varying_color = b_Color;

#ifdef USE_TEXTURE_2D
	varying_texcoord = (b_TextureMatrix[0] * b_MultiTexCoord0).st;
#endif
}

