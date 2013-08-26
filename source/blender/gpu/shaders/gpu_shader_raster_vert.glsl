varying vec4 varying_color;

#ifdef USE_LINE_STIPPLE
attribute float b_StippleParameter;
#endif

void main()
{
	varying_color = b_Color;

#ifdef USE_LINE_STIPPLE
	varying_stipple_parameter = b_StippleParameter;
#endif

	gl_Position = b_ModelViewProjectionMatrix * b_Vertex;
}
