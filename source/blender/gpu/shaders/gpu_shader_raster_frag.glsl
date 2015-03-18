varying vec4 varying_color;

#ifdef USE_LINE_STIPPLE
varying float varying_stipple_parameter;
#endif

void main()
{
	gl_FragColor = varying_color;
}