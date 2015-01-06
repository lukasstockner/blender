uniform vec2 invrendertargetdim;

//texture coordinates for framebuffer read
varying vec4 uvcoordsvar;
varying vec2 uv1;
varying vec2 uv2;
varying vec2 uv3;
varying vec2 uv4;


void vert_half_downsample(void)
{
	uvcoordsvar = gl_MultiTexCoord0;
	gl_Position = gl_Vertex;
	
	uv1 = gl_MultiTexCoord0.xy + vec2(-0.5, -0.5) * invrendertargetdim;
	uv2 = gl_MultiTexCoord0.xy + vec2(-0.5, 0.5) * invrendertargetdim;
	uv3 = gl_MultiTexCoord0.xy + vec2(0.5, -0.5) * invrendertargetdim;
	uv4 = gl_MultiTexCoord0.xy + vec2(0.5, 0.5) * invrendertargetdim;
}

void vert_final_combine(void)
{
	uvcoordsvar = gl_MultiTexCoord0;
	gl_Position = gl_Vertex;
}

void main(void)
{
#ifdef HALF_DOWNSAMPLE_PASS
	vert_half_downsample();
#elif defined(HALF_DOWNSAMPLE_COC_PASS)
	vert_final_combine();
#endif
}
