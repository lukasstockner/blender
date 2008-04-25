
varying vec3 varco;
varying vec3 varcamco;
varying vec3 varnormal;

/*********** SHADER NODES ***************/

void geom(vec3 attorco, vec2 attuv, vec4 attvcol, out vec3 global, out vec3 local, out vec3 view, out vec3 orco, out vec3 uv, out vec3 normal, out vec4 vcol, out float frontback)
{
	local = varcamco;
	view = normalize(local);
	orco = attorco;
	uv = vec3(attuv*2.0 - vec2(1.0, 1.0), 0.0);
	normal = -normalize(varnormal);
	vcol = vec4(attvcol.w/255.0, attvcol.z/255.0, attvcol.y/255.0, 1.0);
	frontback = 1.0;
}

void mapping(vec3 vec, mat4 mat, vec3 minvec, vec3 maxvec, float domin, float domax, out vec3 outvec)
{
	outvec = (mat * vec4(vec, 1.0)).xyz;
	if(domin == 1.0)
		outvec = max(outvec, minvec);
	if(domax == 1.0)
		outvec = min(outvec, maxvec);
}

void camera(out vec3 outview, out float outdepth, out float outdist)
{
	outview = varcamco;
	outdepth = abs(outview.z);
	outdist = length(outview);
	outview = normalize(outview);
}

void math_add(float val1, float val2, out float outval)
{
	outval = val1 + val2;
}

void math_subtract(float val1, float val2, out float outval)
{
	outval = val1 - val2;
}

void math_multiply(float val1, float val2, out float outval)
{
	outval = val1 * val2;
}

void math_divide(float val1, float val2, out float outval)
{
	if (val2 == 0.0)
		outval = 0.0;
	else
		outval = val1 / val2;
}

void math_sine(float val, out float outval)
{
	outval = sin(val);
}

void math_cosine(float val, out float outval)
{
	outval = cos(val);
}

void math_tangent(float val, out float outval)
{
	outval = tan(val);
}

void math_asin(float val, out float outval)
{
	if (val <= 1.0 && val >= -1.0)
		outval = asin(val);
	else
		outval = 0.0;
}

void math_acos(float val, out float outval)
{
	if (val <= 1.0 && val >= -1.0)
		outval = acos(val);
	else
		outval = 0.0;
}

void math_atan(float val, out float outval)
{
	outval = atan(val);
}

void math_pow(float val1, float val2, out float outval)
{
	if (val1 >= 0.0)
		outval = pow(val1, val2);
	else
		outval = 0.0;
}

void math_log(float val1, float val2, out float outval)
{
	if(val1 > 0.0  && val2 > 0.0)
		outval= log(val1) / log(val2);
	else
		outval= 0.0;
}

void math_max(float val1, float val2, out float outval)
{
	outval = max(val1, val2);
}

void math_min(float val1, float val2, out float outval)
{
	outval = min(val1, val2);
}

void math_round(float val, out float outval)
{
	outval= floor(val + 0.5);
}

void squeeze(float val, float width, float center, out float outval)
{
	outval = 1.0/(1.0 + pow(2.71828183, -((val-center)*width)));
}

void vec_math_add(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = v1 + v2;
	outval = (abs(outvec[0]) + abs(outvec[1]) + abs(outvec[2]))/3.0;
}

void vec_math_sub(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = v1 - v2;
	outval = (abs(outvec[0]) + abs(outvec[1]) + abs(outvec[2]))/3.0;
}

void vec_math_average(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = v1 + v2;
	outval = length(outvec);
	outvec = normalize(outvec);
}

void vec_math_dot(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = vec3(0, 0, 0);
	outval = dot(v1, v2);
}

void vec_math_cross(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = cross(v1, v2);
	outval = length(outvec);
}

void vec_math_normalize(vec3 v, out vec3 outvec, out float outval)
{
	outval = length(v);
	outvec = normalize(v);
	outval = length(outvec);
}

void normal(vec3 dir, vec3 nor, out vec3 outnor, out float outdot)
{
	outnor = dir;
	outdot = -dot(dir, nor);
}

void curves_vec(vec3 vec, sampler1D curvemap, out vec3 outvec)
{
	outvec.x = texture1D(curvemap, (vec.x + 1.0)*0.5).x;
	outvec.y = texture1D(curvemap, (vec.y + 1.0)*0.5).y;
	outvec.z = texture1D(curvemap, (vec.z + 1.0)*0.5).z;
}

void curves_rgb(vec4 col, sampler1D curvemap, out vec4 outcol)
{
	outcol.r = texture1D(curvemap, texture1D(curvemap, col.r).a).r;
	outcol.g = texture1D(curvemap, texture1D(curvemap, col.g).a).g;
	outcol.b = texture1D(curvemap, texture1D(curvemap, col.b).a).b;
	outcol.a = col.a;
}

void setvalue(float val, out float outval)
{
	outval = val;
}

void setrgb(vec4 col, out vec4 outcol)
{
	outcol = col;
}

void mix_blend(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col2, fac);
}

void mix_add(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 + col2, fac);
}

void mix_mult(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 * col2, fac);
}

void mix_sub(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 - col2, fac);
}

void mix_screen(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 - col2, fac); // TODO
}

void mix_div(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 / col2, fac); // TODO
}

/* TODO: blend modes */

void valtorgb(float fac, sampler1D colormap, out vec4 outcol, out float outalpha)
{
	outcol = texture1D(colormap, fac);
	outalpha = outcol.a;
}

void rgbtobw(vec4 color, out float outval)
{
	outval = color.r*0.35 + color.g*0.45 + color.b*0.2;
}

void invert(float fac, vec4 col, out vec4 outcol)
{
	outcol.xyz = mix(col.xyz, vec3(1.0, 1.0, 1.0) - col.xyz, fac);
	outcol.w = col.w;
}

void rgb_to_hsv(vec4 rgb, out vec4 outcol)
{
	float cmax, cmin, h, s, v, cdelta;
	vec3 c;

	cmax = max(rgb[0], max(rgb[1], rgb[2]));
	cmin = min(rgb[0], min(rgb[1], rgb[2]));
	cdelta = cmax-cmin;

	v = cmax;
	if (cmax!=0.0)
		s = cdelta/cmax;
	else {
		s = 0.0;
		h = 0.0;
	}

	if (s == 0.0) {
		h = 0.0;
	}
	else {
		c = (vec3(cmax, cmax, cmax) - rgb.xyz)/cdelta;

		if (rgb.x==cmax) h = c[2] - c[1];
		else if (rgb.y==cmax) h = 2.0 + c[0] -  c[2];
		else h = 4.0 + c[1] - c[0];

		h /= 6.0;

		if (h<0.0)
			h += 1.0;
	}

	outcol = vec4(h, s, v, rgb.w);
}

void hsv_to_rgb(vec4 hsv, out vec4 outcol)
{
	float i, f, p, q, t, h, s, v;
	vec3 rgb;

	h = hsv[0];
	s = hsv[1];
	v = hsv[2];

	if(s==0.0) {
		rgb = vec3(v, v, v);
	}
	else {
		if(h==1.0)
			h = 0.0;
		
		h *= 6.0;
		i = floor(h);
		f = h - i;
		rgb = vec3(f, f, f);
		p = v*(1.0-s);
		q = v*(1.0-(s*f));
		t = v*(1.0-(s*(1.0-f)));
		
		if (i == 0.0) rgb = vec3(v, t, p);
		else if (i == 1.0) rgb = vec3(q, v, p);
		else if (i == 2.0) rgb = vec3(p, v, t);
		else if (i == 3.0) rgb = vec3(p, q, v);
		else if (i == 4.0) rgb = vec3(t, p, v);
		else rgb = vec3(v, p, q);
	}

	outcol = vec4(rgb, hsv.w);
}

void hue_sat(float hue, float sat, float value, float fac, vec4 col, out vec4 outcol)
{
	vec4 hsv;

	rgb_to_hsv(col, hsv);

	hsv[0] += (hue - 0.5);
	if(hsv[0]>1.0) hsv[0]-=1.0; else if(hsv[0]<0.0) hsv[0]+= 1.0;
	hsv[1] *= sat;
	if(hsv[1]>1.0) hsv[1]= 1.0; else if(hsv[1]<0.0) hsv[1]= 0.0;
	hsv[2] *= value;
	if(hsv[2]>1.0) hsv[2]= 1.0; else if(hsv[2]<0.0) hsv[2]= 0.0;

	hsv_to_rgb(hsv, outcol);

	outcol = mix(col, outcol, fac);
}

void separate_rgb(vec4 col, out float r, out float g, out float b)
{
	r = col.r;
	g = col.g;
	b = col.b;
}

void combine_rgb(float r, float g, float b, out vec4 col)
{
	col = vec4(r, g, b, 1.0);
}

/*********** TEXTURES ***************/

void texture_flip_blend(vec3 vec, out vec3 outvec)
{
	outvec = vec.yxz;
}

void texture_blend_lin(vec3 vec, out float outval)
{
	outval = (1.0+vec.x)/2.0;
}

void texture_blend_quad(vec3 vec, out float outval)
{
	outval = max((1.0+vec.x)/2.0, 0.0);
	outval *= outval;
}

void texture_wood_sin(vec3 vec, out float value, out vec4 color, out vec3 normal)
{
	float a = sqrt(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z)*20.0;
	float wi = 0.5 + 0.5*sin(a);

	value = wi;
	color = vec4(wi, wi, wi, 1.0);
	normal = vec3(0.0, 0.0, 0.0);
}

void texture_image(vec3 vec, sampler2D ima, out float value, out vec4 color, out vec3 normal)
{
	color = texture2D(ima, (vec.xy + vec2(1.0, 1.0))*0.5);
	value = 1.0;
	normal = vec3(0.0, 0.0, 0.0);
}

/************* MTEX *****************/

void texco_orco(vec3 attorco, out vec3 orco)
{
	orco = attorco;
}

void texco_uv(vec2 attuv, out vec3 uv)
{
	uv = vec3(attuv*2.0 - vec2(1.0, 1.0), 0.0);
}

void texco_norm(out vec3 normal)
{
	normal = -normalize(varnormal);
}

void mtex_blend(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	incol = fact*texcol + facm*outcol;
}

void mtex_rgbtoint(vec4 rgb, out float intensity)
{
	intensity = 0.35*rgb.r + 0.45*rgb.g + 0.2*rgb.b;
}

void mtex_invert(vec4 inrgb, out vec4 outrgb)
{
	outrgb = vec4(1.0) - inrgb;
}

void mtex_mapping(vec3 texco, vec3 size, vec3 ofs, out vec3 outtexco)
{
	outtexco.x = size.x*(texco.x - 0.5) + ofs.x + 0.5;
	outtexco.y = size.y*(texco.y - 0.5) + ofs.y + 0.5;
	outtexco.z = texco.z;
}

void mtex_2d_mapping(vec3 vec, out vec3 outvec)
{
	outvec.xy = (vec.xy + vec2(1.0, 1.0))*0.5;
	outvec.z = vec.z;
}

void mtex_image(vec3 vec, sampler2D ima, out float value, out vec4 color, out vec3 normal)
{
	color = texture2D(ima, vec.xy);
	value = 1.0;
	normal = vec3(0.0, 0.0, 0.0);
}

/******* MATERIAL *********/

void material_simple(vec4 col, float ref, vec4 spec, float specfac, float hard, out vec4 combined)
{
	vec3 l1 = vec3(-0.300, 0.300, 0.900);
	vec3 l2 = vec3(0.500, 0.500, 0.100);
	//vec3 v = normalize(varcamco);
	//vec3 h = normalize(l + v);

	combined = max(dot(varnormal, l1), 0.0)*ref*col*vec4(0.8, 0.8, 0.8, 1.0);
	combined += max(dot(varnormal, l2), 0.0)*ref*col*vec4(0.4, 0.4, 0.8, 1.0);
	//combined += pow(max(dot(varnormal, h), 0.0), hard)*specfac*spec;
}

