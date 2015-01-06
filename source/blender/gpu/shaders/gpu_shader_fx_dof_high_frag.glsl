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
// circle of confusion buffer
uniform sampler2D cocbuffer;

// this includes focal distance in x and aperture size in y
uniform vec4 dof_params;
// viewvectors for reconstruction of world space
uniform vec4 viewvecs[3];

/* coc calculation, positive is far coc, negative is near */
vec4 calculate_signed_coc(in vec4 zdepth)
{
	vec4 coc = dof_params.x * (vec4(1.0) - vec4(dof_params.y) / zdepth);

	/* multiply by 1.0 / sensor size to get the normalized size */
	return coc * dof_params.z;
}

void half_downsample_frag(void)
{
	vec4 depthv, coc;
	
	depthv.r = texture2D(depthbuffer, depth_uv1).r;
	depthv.g = texture2D(depthbuffer, depth_uv2).r;
	depthv.b = texture2D(depthbuffer, depth_uv3).r;
	depthv.a = texture2D(depthbuffer, depth_uv4).r;
	
	coc = calculate_signed_coc(get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depthv));
	
	/* near coc, keep the min here */
	gl_FragData[1].r = max(-min(min(coc.r, coc.g), min(coc.b, coc.a)), 0.0);
	/* far coc keep the max */
	gl_FragData[1].g = max(max(max(coc.r, coc.g), max(coc.b, coc.a)), 0.0);
	/* framebuffer output 1 is bound to half size color. linear filtering should take care of averaging here */
	gl_FragData[0] = texture2D(colorbuffer, uvcoordsvar.xy);
}

void final_combine_frag(void)
{
	vec4 coc = texture2D(cocbuffer, uvcoordsvar.xy);
	/* framebuffer output 1 is bound to half size color. linear filtering should take care of averaging here */
	gl_FragColor = texture2D(colorbuffer, uvcoordsvar.xy);
	gl_FragColor.g *= coc.g;
	gl_FragColor.r *= coc.r;
}

void main(void)
{
#ifdef HALF_DOWNSAMPLE_PASS
	half_downsample_frag();
#elif defined(HALF_DOWNSAMPLE_COC_PASS)
	final_combine_frag();
#endif
}
