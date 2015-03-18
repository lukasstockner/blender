varying vec4 varying_color;
varying vec2 varying_texcoord;

void main()
{
#ifdef GPU_PROFILE_CORE
	gl_FragColor = vec4(varying_color.rgb, texture2D(b_Sampler2D[0], varying_texcoord).r * varying_color.a);
#else
	gl_FragColor = vec4(varying_color.rgb, texture2D(b_Sampler2D[0], varying_texcoord).a * varying_color.a);
#endif
}