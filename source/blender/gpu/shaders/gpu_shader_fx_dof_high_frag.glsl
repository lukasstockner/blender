uniform vec2 invrendertargetdim;

//texture coordinates for framebuffer read
varying vec4 uvcoordsvar;
varying vec2 depth_uv1;
varying vec2 depth_uv2;
varying vec2 depth_uv3;
varying vec2 depth_uv4;

// color buffer
uniform sampler2D colorbuffer;
// depth buffer
uniform sampler2D depthbuffer;

// this includes focal distance in x and aperture size in y
uniform vec4 dof_params;

/* coc calculation, positive is far coc, negative is near */
float calculate_signed_coc(in float zdepth)
{
	float coc = dof_params.x * (1.0 - dof_params.y / zdepth);

	/* multiply by 1.0 / sensor size to get the normalized size */
	return coc * dof_params.z;
}

void half_downsample_frag()
{
	vec4 depthv, final_coc;
	depthv.r = calculate_signed_coc(texture2D(depthbuffer, depth_uv1).r);
	depthv.g = calculate_signed_coc(texture2D(depthbuffer, depth_uv2).r);
	depthv.b = calculate_signed_coc(texture2D(depthbuffer, depth_uv3).r);
	depthv.a = calculate_signed_coc(texture2D(depthbuffer, depth_uv4).r);
	
	/* near coc, keep the min here */
	gl_FragData[1].r = min(min(depthv.r, depthv.g), min(depthv.b, depthv.a));
	/* far coc keep the max */
	gl_FragData[1].g = max(-min(min(depthv.r, depthv.g), min(depthv.b, depthv.a)), 0.0);
	gl_FragData[1].b = gl_FragData[1].a = 0.0;
	
	/* framebuffer output 1 is bound to half size color. linear filtering should take care of averaging here */
	gl_FragData[0] = texture2D(colorbuffer, uvcoordsvar);
}

void main(void)
{
#ifdef HALF_DOWNSAMPLE_PASS
	half_downsample_frag();
#elif defined(HALF_DOWNSAMPLE_COC_PASS)

#endif

}
