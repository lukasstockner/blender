uniform vec2 invrendertargetdim;

//texture coordinates for framebuffer read
varying vec4 uvcoordsvar;
varying vec2 depth_uv1;
varying vec2 depth_uv2;
varying vec2 depth_uv3;
varying vec2 depth_uv4;


void vert_half_downsample(void)
{
	uvcoordsvar = gl_MultiTexCoord0;
	gl_Position = gl_Vertex;
	
	depth_uv1 = gl_MultiTexCoord0.xy + vec2(0.0, 0.0) * invrendertargetdim;
	depth_uv2 = gl_MultiTexCoord0.xy + vec2(0.0, 1.0) * invrendertargetdim;
	depth_uv3 = gl_MultiTexCoord0.xy + vec2(1.0, 0.0) * invrendertargetdim;
	depth_uv4 = gl_MultiTexCoord0.xy + vec2(1.0, 1.0) * invrendertargetdim;
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
