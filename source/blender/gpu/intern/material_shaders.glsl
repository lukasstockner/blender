
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

#define M_PI 3.14159265358979323846

/*********** SHADER NODES ***************/

void vcol_attribute(vec4 attvcol, out vec4 vcol)
{
	vcol = vec4(attvcol.x/255.0, attvcol.y/255.0, attvcol.z/255.0, 1.0);
}

void uv_attribute(vec2 attuv, out vec3 uv)
{
	uv = vec3(attuv*2.0 - vec2(1.0, 1.0), 0.0);
}

void geom(vec3 co, vec3 nor, mat4 viewinvmat, vec3 attorco, vec2 attuv, vec4 attvcol, out vec3 global, out vec3 local, out vec3 view, out vec3 orco, out vec3 uv, out vec3 normal, out vec4 vcol, out float frontback)
{
	local = co;
	view = normalize(local);
	global = (viewinvmat*vec4(local, 1.0)).xyz;
	orco = attorco;
	uv_attribute(attuv, uv);
	normal = -normalize(nor);	/* blender render normal is negated */
	vcol_attribute(attvcol, vcol);
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

void camera(vec3 co, out vec3 outview, out float outdepth, out float outdist)
{
	outview = co;
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

void math_less_than(float val1, float val2, out float outval)
{
	if(val1 < val2)
		outval = 1.0;
	else
		outval = 0.0;
}

void math_greater_than(float val1, float val2, out float outval)
{
	if(val1 > val2)
		outval = 1.0;
	else
		outval = 0.0;
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
}

void vec_math_negate(vec3 v, out vec3 outv)
{
	outv = -v;
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

void set_value(float val, out float outval)
{
	outval = val;
}

void set_rgb(vec3 col, out vec3 outcol)
{
	outcol = col;
}

void set_rgba(vec4 col, out vec4 outcol)
{
	outcol = col;
}

void set_value_zero(out float outval)
{
	outval = 0.0;
}

void set_rgb_zero(out vec3 outval)
{
	outval = vec3(0.0);
}

void set_rgba_zero(out vec4 outval)
{
	outval = vec4(0.0);
}

void mix_blend(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col2, fac);
	outcol.a = col1.a;
}

void mix_add(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 + col2, fac);
	outcol.a = col1.a;
}

void mix_mult(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 * col2, fac);
	outcol.a = col1.a;
}

void mix_screen(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = vec4(1.0) - (vec4(facm) + fac*(vec4(1.0) - col2))*(vec4(1.0) - col1);
	outcol.a = col1.a;
}

void mix_overlay(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	if(outcol.r < 0.5)
		outcol.r *= facm + 2.0*fac*col2.r;
	else
		outcol.r = 1.0 - (facm + 2.0*fac*(1.0 - col2.r))*(1.0 - outcol.r);

	if(outcol.g < 0.5)
		outcol.g *= facm + 2.0*fac*col2.g;
	else
		outcol.g = 1.0 - (facm + 2.0*fac*(1.0 - col2.g))*(1.0 - outcol.g);

	if(outcol.b < 0.5)
		outcol.b *= facm + 2.0*fac*col2.b;
	else
		outcol.b = 1.0 - (facm + 2.0*fac*(1.0 - col2.b))*(1.0 - outcol.b);
}

void mix_sub(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 - col2, fac);
	outcol.a = col1.a;
}

void mix_div(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	if(col2.r != 0.0) outcol.r = facm*outcol.r + fac*outcol.r/col2.r;
	if(col2.g != 0.0) outcol.g = facm*outcol.g + fac*outcol.g/col2.g;
	if(col2.b != 0.0) outcol.b = facm*outcol.b + fac*outcol.b/col2.b;
}

void mix_diff(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, abs(col1 - col2), fac);
	outcol.a = col1.a;
}

void mix_dark(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol.rgb = min(col1.rgb, col2.rgb*fac);
	outcol.a = col1.a;
}

void mix_light(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol.rgb = max(col1.rgb, col2.rgb*fac);
	outcol.a = col1.a;
}

void mix_dodge(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = col1;

	if(outcol.r != 0.0) {
		float tmp = 1.0 - fac*col2.r;
		if(tmp <= 0.0)
			outcol.r = 1.0;
		else if((tmp = outcol.r/tmp) > 1.0)
			outcol.r = 1.0;
		else
			outcol.r = tmp;
	}
	if(outcol.g != 0.0) {
		float tmp = 1.0 - fac*col2.g;
		if(tmp <= 0.0)
			outcol.g = 1.0;
		else if((tmp = outcol.g/tmp) > 1.0)
			outcol.g = 1.0;
		else
			outcol.g = tmp;
	}
	if(outcol.b != 0.0) {
		float tmp = 1.0 - fac*col2.b;
		if(tmp <= 0.0)
			outcol.b = 1.0;
		else if((tmp = outcol.b/tmp) > 1.0)
			outcol.b = 1.0;
		else
			outcol.b = tmp;
	}
}

void mix_burn(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float tmp, facm = 1.0 - fac;

	outcol = col1;

	tmp = facm + fac*col2.r;
	if(tmp <= 0.0)
		outcol.r = 0.0;
	else if((tmp = (1.0 - (1.0 - outcol.r)/tmp)) < 0.0)
		outcol.r = 0.0;
	else if(tmp > 1.0)
		outcol.r = 1.0;
	else
		outcol.r = tmp;

	tmp = facm + fac*col2.g;
	if(tmp <= 0.0)
		outcol.g = 0.0;
	else if((tmp = (1.0 - (1.0 - outcol.g)/tmp)) < 0.0)
		outcol.g = 0.0;
	else if(tmp > 1.0)
		outcol.g = 1.0;
	else
		outcol.g = tmp;

	tmp = facm + fac*col2.b;
	if(tmp <= 0.0)
		outcol.b = 0.0;
	else if((tmp = (1.0 - (1.0 - outcol.b)/tmp)) < 0.0)
		outcol.b = 0.0;
	else if(tmp > 1.0)
		outcol.b = 1.0;
	else
		outcol.b = tmp;
}

void mix_hue(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	vec4 hsv, hsv2, tmp;
	rgb_to_hsv(col2, hsv2);

	if(hsv2.y != 0.0) {
		rgb_to_hsv(outcol, hsv);
		hsv.x = hsv2.x;
		hsv_to_rgb(hsv, tmp); 

		outcol = mix(outcol, tmp, fac);
		outcol.a = col1.a;
	}
}

void mix_sat(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	vec4 hsv, hsv2;
	rgb_to_hsv(outcol, hsv);

	if(hsv.y != 0.0) {
		rgb_to_hsv(col2, hsv2);

		hsv.y = facm*hsv.y + fac*hsv2.y;
		hsv_to_rgb(hsv, outcol);
	}
}

void mix_val(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	vec4 hsv, hsv2;
	rgb_to_hsv(col1, hsv);
	rgb_to_hsv(col2, hsv2);

	hsv.z = facm*hsv.z + fac*hsv2.z;
	hsv_to_rgb(hsv, outcol);
}

void mix_color(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	vec4 hsv, hsv2, tmp;
	rgb_to_hsv(col2, hsv2);

	if(hsv2.y != 0.0) {
		rgb_to_hsv(outcol, hsv);
		hsv.x = hsv2.x;
		hsv.y = hsv2.y;
		hsv_to_rgb(hsv, tmp); 

		outcol = mix(outcol, tmp, fac);
		outcol.a = col1.a;
	}
}

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

void output_node(vec4 rgb, float alpha, out vec4 outrgb)
{
	outrgb = vec4(rgb.rgb, alpha);
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

	normal.x = 2.0*(color.r - 0.5);
	normal.y = 2.0*(0.5 - color.g);
	normal.z = 2.0*(color.b - 0.5);
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

void texco_norm(vec3 normal, out vec3 outnormal)
{
	/* corresponds to shi->orn, which is negated so cancels
	   out blender normal negation */
	outnormal = normalize(normal);
}

void texco_tangent(vec3 tangent, out vec3 outtangent)
{
	outtangent = normalize(tangent);
}

void texco_global(mat4 viewinvmat, vec3 co, out vec3 global)
{
	global = (viewinvmat*vec4(co, 1.0)).xyz;
}

void texco_object(mat4 viewinvmat, mat4 obinvmat, vec3 co, out vec3 object)
{
	object = (obinvmat*(viewinvmat*vec4(co, 1.0))).xyz;
}

void shade_norm(vec3 normal, out vec3 outnormal)
{
	/* blender render normal is negated */
	outnormal = -normalize(normal);
}

void mtex_rgb_blend(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	incol = fact*texcol + facm*outcol;
}

void mtex_rgb_mul(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-facg;

	incol = (facm + fact*texcol)*outcol;
}

void mtex_rgb_screen(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-facg;

	incol = vec3(1.0) - (vec3(facm) + fact*(vec3(1.0) - texcol))*(vec3(1.0) - outcol);
}

void mtex_rgb_overlay(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-facg;

	if(outcol.r < 0.5)
		incol.r = outcol.r*(facm + 2.0*fact*texcol.r);
	else
		incol.r = 1.0 - (facm + 2.0*fact*(1.0 - texcol.r))*(1.0 - outcol.r);

	if(outcol.g < 0.5)
		incol.g = outcol.g*(facm + 2.0*fact*texcol.g);
	else
		incol.g = 1.0 - (facm + 2.0*fact*(1.0 - texcol.g))*(1.0 - outcol.g);

	if(outcol.b < 0.5)
		incol.b = outcol.b*(facm + 2.0*fact*texcol.b);
	else
		incol.b = 1.0 - (facm + 2.0*fact*(1.0 - texcol.b))*(1.0 - outcol.b);
}

void mtex_rgb_sub(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	incol = -fact*facg*texcol + outcol;
}

void mtex_rgb_add(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	incol = fact*facg*texcol + outcol;
}

void mtex_rgb_div(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	if(texcol.r != 0.0) incol.r = facm*outcol.r + fact*outcol.r/texcol.r;
	if(texcol.g != 0.0) incol.g = facm*outcol.g + fact*outcol.g/texcol.g;
	if(texcol.b != 0.0) incol.b = facm*outcol.b + fact*outcol.b/texcol.b;
}

void mtex_rgb_diff(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	incol = facm*outcol + fact*abs(texcol - outcol);
}

void mtex_rgb_dark(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm, col;

	fact *= facg;
	facm = 1.0-fact;

	col = fact*texcol.r;
	if(col < outcol.r) incol.r = col; else incol.r = outcol.r;
	col = fact*texcol.g;
	if(col < outcol.g) incol.g = col; else incol.g = outcol.g;
	col = fact*texcol.b;
	if(col < outcol.b) incol.b = col; else incol.b = outcol.b;
}

void mtex_rgb_light(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm, col;

	fact *= facg;
	facm = 1.0-fact;

	col = fact*texcol.r;
	if(col > outcol.r) incol.r = col; else incol.r = outcol.r;
	col = fact*texcol.g;
	if(col > outcol.g) incol.g = col; else incol.g = outcol.g;
	col = fact*texcol.b;
	if(col > outcol.b) incol.b = col; else incol.b = outcol.b;
}

void mtex_rgb_hue(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	vec4 col;

	mix_hue(fact*facg, vec4(outcol, 1.0), vec4(texcol, 1.0), col);
	incol.rgb = col.rgb;
}

void mtex_rgb_sat(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	vec4 col;

	mix_sat(fact*facg, vec4(outcol, 1.0), vec4(texcol, 1.0), col);
	incol.rgb = col.rgb;
}

void mtex_rgb_val(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	vec4 col;

	mix_val(fact*facg, vec4(outcol, 1.0), vec4(texcol, 1.0), col);
	incol.rgb = col.rgb;
}

void mtex_rgb_color(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	vec4 col;

	mix_color(fact*facg, vec4(outcol, 1.0), vec4(texcol, 1.0), col);
	incol.rgb = col.rgb;
}

void mtex_value_vars(inout float fact, float facg, out float facm, float flip)
{
	fact *= facg;
	facm = 1.0-fact;

	if(flip != 0.0) {
		float tmp = fact;
		fact = facm;
		facm = tmp;
	}
}

void mtex_value_blend(float outcol, float texcol, float fact, float facg, float flip, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm, flip);

	incol = fact*texcol + facm*outcol;
}

void mtex_value_mul(float outcol, float texcol, float fact, float facg, float flip, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm, flip);

	facm = 1.0 - facg;
	incol = (facm + fact*texcol)*outcol;
}

void mtex_value_screen(float outcol, float texcol, float fact, float facg, float flip, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm, flip);

	facm = 1.0 - facg;
	incol = 1.0 - (facm + fact*(1.0 - texcol))*(1.0 - outcol);
}

void mtex_value_sub(float outcol, float texcol, float fact, float facg, float flip, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm, flip);

	fact = -fact;
	incol = fact*texcol + outcol;
}

void mtex_value_add(float outcol, float texcol, float fact, float facg, float flip, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm, flip);

	fact = fact;
	incol = fact*texcol + outcol;
}

void mtex_value_div(float outcol, float texcol, float fact, float facg, float flip, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm, flip);

	if(texcol != 0.0)
		incol = facm*outcol + fact*outcol/texcol;
	else
		incol = 0.0;
}

void mtex_value_diff(float outcol, float texcol, float fact, float facg, float flip, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm, flip);

	incol = facm*outcol + fact*abs(texcol - outcol);
}

void mtex_value_dark(float outcol, float texcol, float fact, float facg, float flip, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm, flip);

	float col = fact*texcol;
	if(col < outcol) incol = col; else incol = outcol;
}

void mtex_value_light(float outcol, float texcol, float fact, float facg, float flip, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm, flip);

	float col = fact*texcol;
	if(col > outcol) incol = col; else incol = outcol;
}

void mtex_value_clamp_positive(float fac, out float outfac)
{
	if(fac < 0.0) outfac = 0.0;
	else outfac = fac;
}

void mtex_value_clamp(float fac, out float outfac)
{
	outfac = clamp(fac, 0.0, 1.0);
}

void mtex_har_divide(float har, out float outhar)
{
	outhar = har/128.0;
}

void mtex_har_multiply_clamp(float har, out float outhar)
{
	har *= 128.0;

	if(har < 1.0) outhar = 1.0;
	else if(har > 511.0) outhar = 511.0;
	else outhar = har;
}

void mtex_alpha_from_col(vec4 col, out float alpha)
{
	alpha = col.a;
}

void mtex_alpha_to_col(vec4 col, float alpha, out vec4 outcol)
{
	outcol = vec4(col.rgb, alpha);
}

void mtex_rgbtoint(vec4 rgb, out float intensity)
{
	intensity = 0.35*rgb.r + 0.45*rgb.g + 0.2*rgb.b;
}

void mtex_value_invert(float invalue, out float outvalue)
{
	outvalue = 1.0 - invalue;
}

void mtex_rgb_invert(vec4 inrgb, out vec4 outrgb)
{
	outrgb = vec4(vec3(1.0) - inrgb.rgb, inrgb.a);
}

void mtex_value_stencil(float stencil, float intensity, out float outstencil, out float outintensity)
{
	float fact = intensity;
	outintensity = intensity*stencil;
	outstencil = stencil*fact;
}

void mtex_rgb_stencil(float stencil, vec4 rgb, out float outstencil, out vec4 outrgb)
{
	float fact = rgb.a;
	outrgb = vec4(rgb.rgb, rgb.a*stencil);
	outstencil = stencil*fact;
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
	
	normal.x = 2.0*(color.r - 0.5);
	normal.y = 2.0*(0.5 - color.g);
	normal.z = 2.0*(color.b - 0.5);
}

void mtex_negate_texnormal(vec3 normal, out vec3 outnormal)
{
	outnormal = vec3(-normal.x, -normal.y, normal.z);
}

void mtex_nspace_tangent(vec3 tangent, vec3 normal, vec3 texnormal, out vec3 outnormal)
{
	tangent = normalize(tangent);
	vec3 B = cross(normal, tangent);

	outnormal = texnormal.x*tangent + texnormal.y*B + texnormal.z*normal;
	outnormal = normalize(outnormal);
}

void mtex_blend_normal(float norfac, vec3 normal, vec3 newnormal, out vec3 outnormal)
{
	norfac = min(norfac, 1.0);
	outnormal = (1.0 - norfac)*normal + norfac*newnormal;
	outnormal = normalize(outnormal);
}

/******* MATERIAL *********/

void lamp_visibility_sun_hemi(mat4 viewmat, vec3 lampvec, out vec3 lv, out float dist, out float visifac)
{
	lampvec = -normalize(lampvec);
	lampvec = (viewmat*vec4(lampvec, 0.0)).xyz;

	lv = lampvec;
	dist = 1.0;
	visifac = 1.0;
}

void lamp_visibility_other(mat4 viewmat, vec3 co, vec3 lampco, out vec3 lv, out float dist, out float visifac)
{
	lv = co - (viewmat*vec4(lampco, 1.0)).xyz;
	dist = length(lv);
	lv = normalize(lv);
	visifac = 1.0;
}

void lamp_falloff_invlinear(float lampdist, float dist, out float visifac)
{
	visifac = lampdist/(lampdist + dist);
}

void lamp_falloff_invsquare(float lampdist, float dist, out float visifac)
{
	visifac = lampdist/(lampdist + dist*dist);
}

void lamp_falloff_sliders(float lampdist, float ld1, float ld2, float dist, out float visifac)
{
	float lampdistkw = lampdist*lampdist;

	visifac = lampdist/(lampdist + ld1*dist);
	visifac *= lampdistkw/(lampdistkw + ld2*dist*dist);
}

void lamp_falloff_curve(float lampdist, sampler1D curvemap, float dist, out float visifac)
{
	visifac = texture1D(curvemap, dist/lampdist).x;
}

void lamp_visibility_sphere(float lampdist, float dist, float visifac, out float outvisifac)
{
	float t= lampdist - dist;

	outvisifac= visifac*max(t, 0.0)/lampdist;
}

void lamp_visibility_spot_square(mat4 viewmat, mat4 viewinvmat, vec3 lampvec, mat4 lampimat, vec3 lv, out float inpr)
{
	lampvec = -normalize(lampvec);
	lampvec = (viewmat*vec4(lampvec, 0.0)).xyz;

	if(dot(lv, lampvec) > 0.0) {
		vec3 lvrot = (viewinvmat*vec4(lv, 0.0)).xyz;
		lvrot = (lampimat*vec4(lvrot, 0.0)).xyz;
		float x = max(abs(lvrot.x/lvrot.z), abs(lvrot.y/lvrot.z));

		inpr = 1.0/sqrt(1.0 + x*x);
	}
	else
		inpr = 0.0;
}

void lamp_visibility_spot_circle(mat4 viewmat, vec3 lampvec, vec3 lv, out float inpr)
{
	lampvec = -normalize(lampvec);
	lampvec = (viewmat*vec4(lampvec, 0.0)).xyz;

	inpr = dot(lv, lampvec);
}

void lamp_visibility_spot(float spotsi, float spotbl, float inpr, float visifac, out float outvisifac)
{
	float t = spotsi;

	if(inpr <= t) {
		outvisifac = 0.0;
	}
	else {
		t = inpr - t;
		if(t < spotbl && spotbl != 0.0) {
			/* soft area */
			float i = t/spotbl;
			t = i*i;
			inpr *= (3.0*t - 2.0*t*i);
		}
		outvisifac = visifac*inpr;
	}
}

void lamp_visibility_clamp(float visifac, out float outvisifac)
{
	if(visifac <= 0.001)
		outvisifac = 0.0;
	else
		outvisifac = visifac;
}

void shade_view(vec3 co, out vec3 view)
{
	/* handle perspective/orthographic */
	if(gl_ProjectionMatrix[3][3] == 0.0)
		view = normalize(co);
	else
		view = vec3(0.0, 0.0, -1.0);
}

void shade_tangent_v(vec3 lv, vec3 tang, out vec3 vn)
{
	vec3 c = cross(lv, tang);
	vec3 vnor = cross(c, tang);

	vn = -normalize(vnor);
}

void shade_inp(vec3 vn, vec3 lv, out float inp)
{
	inp = dot(vn, lv);
}

void shade_is_no_diffuse(out float is)
{
	is = 0.0;
}

void shade_is_hemi(float inp, out float is)
{
	is = 0.5*inp + 0.5;
}

float area_lamp_energy(mat4 area, vec3 co, vec3 vn)
{
	vec3 vec[4], c[4];
	float rad[4], fac;
	
	vec[0] = normalize(co - area[0].xyz);
	vec[1] = normalize(co - area[1].xyz);
	vec[2] = normalize(co - area[2].xyz);
	vec[3] = normalize(co - area[3].xyz);

	c[0] = normalize(cross(vec[0], vec[1]));
	c[1] = normalize(cross(vec[1], vec[2]));
	c[2] = normalize(cross(vec[2], vec[3]));
	c[3] = normalize(cross(vec[3], vec[0]));

	rad[0] = acos(dot(vec[0], vec[1]));
	rad[1] = acos(dot(vec[1], vec[2]));
	rad[2] = acos(dot(vec[2], vec[3]));
	rad[3] = acos(dot(vec[3], vec[0]));

	fac=  rad[0]*dot(vn, c[0]);
	fac+= rad[1]*dot(vn, c[1]);
	fac+= rad[2]*dot(vn, c[2]);
	fac+= rad[3]*dot(vn, c[3]);

	return max(fac, 0.0);
}

void shade_inp_area(vec3 position, vec3 lampco, vec3 lampvec, vec3 vn, mat4 area, float areasize, float k, out float inp)
{
	lampvec = -normalize(lampvec);

	vec3 co = position;
	vec3 vec = co - lampco;

	if(dot(vec, lampvec) < 0.0) {
		inp = 0.0;
	}
	else {
		float intens = area_lamp_energy(area, co, vn);

		inp = pow(intens*areasize, k);
	}
}

void shade_diffuse_oren_nayer(float nl, vec3 n, vec3 l, vec3 v, float rough, out float is)
{
	vec3 h = normalize(v + l);
	float nh = max(dot(n, h), 0.0);
	float nv = max(dot(n, v), 0.0);
	float realnl = dot(n, l);

	if(realnl < 0.0) {
		is = 0.0;
	}
	else if(nl < 0.0) {
		is = 0.0;
	}
	else {
		float vh = max(dot(v, h), 0.0);
		float Lit_A = acos(realnl);
		float View_A = acos(nv);

		vec3 Lit_B = normalize(l - realnl*n);
		vec3 View_B = normalize(v - nv*n);

		float t = max(dot(Lit_B, View_B), 0.0);

		float a, b;

		if(Lit_A > View_A) {
			a = Lit_A;
			b = View_A;
		}
		else {
			a = View_A;
			b = Lit_A;
		}

		float A = 1.0 - (0.5*((rough*rough)/((rough*rough) + 0.33)));
		float B = 0.45*((rough*rough)/((rough*rough) + 0.09));

		b *= 0.95;
		is = nl*(A + (B * t * sin(a) * tan(b)));
	}
}

void shade_diffuse_toon(vec3 n, vec3 l, vec3 v, float size, float tsmooth, out float is)
{
	float rslt = dot(n, l);
	float ang = acos(rslt);

	if(ang < size) is = 1.0;
	else if(ang > (size + tsmooth) || tsmooth == 0.0) is = 0.0;
	else is = 1.0 - ((ang - size)/tsmooth);
}

void shade_diffuse_minnaert(float nl, vec3 n, vec3 v, float darkness, out float is)
{
	if(nl <= 0.0) {
		is = 0.0;
	}
	else {
		float nv = max(dot(n, v), 0.0);

		if(darkness <= 1.0)
			is = nl*pow(max(nv*nl, 0.1), darkness - 1.0);
		else
			is = nl*pow(1.0001 - nv, darkness - 1.0);
	}
}

float fresnel_fac(vec3 view, vec3 vn, float grad, float fac)
{
	float t1, t2;
	float ffac;

	if(fac==0.0) {
		ffac = 1.0;
	}
	else {
		t1= dot(view, vn);
		if(t1>0.0)  t2= 1.0+t1;
		else t2= 1.0-t1;

		t2= grad + (1.0-grad)*pow(t2, fac);

		if(t2<0.0) ffac = 0.0;
		else if(t2>1.0) ffac = 1.0;
		else ffac = t2;
	}

	return ffac;
}

void shade_diffuse_fresnel(vec3 vn, vec3 lv, vec3 view, float fac_i, float fac, out float is)
{
	is = fresnel_fac(lv, vn, fac_i, fac);
}

void shade_cubic(float is, out float outis)
{
	if(is>0.0 && is<1.0)
		outis= 3.0*is*is - 2.0*is*is*is;
	else
		outis= is;
}

void shade_visifac(float i, float visifac, float refl, out float outi)
{
	if(i > 0.0)
		outi = i*visifac*refl;
	else
		outi = i;
}

void shade_tangent_v_spec(vec3 tang, out vec3 vn)
{
	vn = tang;
}

void shade_add_to_diffuse(float i, vec3 lampcol, vec3 col, out vec3 outcol)
{
	if(i > 0.0)
		outcol = i*lampcol*col;
	else
		outcol = vec3(0.0, 0.0, 0.0);
}

void shade_hemi_spec(vec3 vn, vec3 lv, vec3 view, float spec, float hard, float visifac, out float t)
{
	lv += view;
	lv = normalize(lv);

	t = dot(vn, lv);
	t = 0.5*t + 0.5;

	t = visifac*spec*pow(t, hard);
}

void shade_phong_spec(vec3 n, vec3 l, vec3 v, float hard, out float specfac)
{
	vec3 h = normalize(l + v);
	float rslt = dot(h, n);

	if(rslt > 0.0) rslt = pow(rslt, hard);
	else rslt = 0.0;

	specfac = rslt;
}

void shade_cooktorr_spec(vec3 n, vec3 l, vec3 v, float hard, out float specfac)
{
	vec3 h = normalize(v + l);
	float nh = dot(n, h);

	if(nh < 0.0) {
		specfac = 0.0;
	}
	else {
		float nv = max(dot(n, v), 0.0);
		float i = pow(nh, hard);

		i = i/(0.1+nv);
		specfac = i;
	}
}

void shade_blinn_spec(vec3 n, vec3 l, vec3 v, float refrac, float spec_power, out float specfac)
{
	if(refrac < 1.0) {
		specfac = 0.0;
	}
	else if(spec_power == 0.0) {
		specfac = 0.0;
	}
	else {
		if(spec_power<100.0)
			spec_power= sqrt(1.0/spec_power);
		else
			spec_power= 10.0/spec_power;

		vec3 h = normalize(v + l);
		float nh = dot(n, h);
		if(nh < 0.0) {
			specfac = 0.0;
		}
		else {
			float nv = max(dot(n, v), 0.01);
			float nl = dot(n, l);
			if(nl <= 0.01) {
				specfac = 0.0;
			}
			else {
				float vh = max(dot(v, h), 0.01);

				float a = 1.0;
				float b = (2.0*nh*nv)/vh;
				float c = (2.0*nh*nl)/vh;

				float g;

				if(a < b && a < c) g = a;
				else if(b < a && b < c) g = b;
				else if(c < a && c < b) g = c;

				float p = sqrt(((refrac * refrac)+(vh*vh)-1.0));
				float f = (((p-vh)*(p-vh))/((p+vh)*(p+vh)))*(1.0+((((vh*(p+vh))-1.0)*((vh*(p+vh))-1.0))/(((vh*(p-vh))+1.0)*((vh*(p-vh))+1.0))));
				float ang = acos(nh);

				specfac = max(f*g*exp((-(ang*ang)/(2.0*spec_power*spec_power))), 0.0);
			}
		}
	}
}

void shade_wardiso_spec(vec3 n, vec3 l, vec3 v, float rms, out float specfac)
{
	vec3 h = normalize(l + v);
	float nh = max(dot(n, h), 0.001);
	float nv = max(dot(n, v), 0.001);
	float nl = max(dot(n, l), 0.001);
	float angle = tan(acos(nh));
	float alpha = max(rms, 0.001);

	specfac= nl * (1.0/(4.0*M_PI*alpha*alpha))*(exp(-(angle*angle)/(alpha*alpha))/(sqrt(nv*nl)));
}

void shade_toon_spec(vec3 n, vec3 l, vec3 v, float size, float tsmooth, out float specfac)
{
	vec3 h = normalize(l + v);
	float rslt = dot(h, n);
	float ang = acos(rslt);

	if(ang < size) rslt = 1.0;
	else if(ang >= (size + tsmooth) || tsmooth == 0.0) rslt = 0.0;
	else rslt = 1.0 - ((ang - size)/tsmooth);

	specfac = rslt;
}

void shade_spec_area_inp(float specfac, float inp, out float outspecfac)
{
	outspecfac = specfac*inp;
}

void shade_spec_t(float shadfac, float spec, float visifac, float specfac, out float t)
{
	t = shadfac*spec*visifac*specfac;
}

void shade_add_spec(float t, vec3 lampcol, vec3 speccol, out vec3 outcol)
{
	outcol = t*lampcol*speccol;
}

void shade_add(vec4 col1, vec4 col2, out vec4 outcol)
{
	outcol = col1 + col2;
}

void shade_madd(vec4 col, vec4 col1, vec4 col2, out vec4 outcol)
{
	outcol = col + col1*col2;
}

void shade_mul(vec4 col1, vec4 col2, out vec4 outcol)
{
	outcol = col1*col2;
}

void shade_mul_value(float fac, vec4 col, out vec4 outcol)
{
	outcol = col*fac;
}

void ramp_rgbtobw(vec3 color, out float outval)
{
	outval = color.r*0.3 + color.g*0.58 + color.b*0.12;
}

void shade_only_shadow(float i, float shadfac, float energy, vec3 rgb, vec3 specrgb, vec4 diff, vec4 spec, out vec4 outdiff, out vec4 outspec)
{
	shadfac = i*energy*(1.0 - shadfac);

	outdiff = diff - vec4(rgb*shadfac, 0.0);
	outspec = spec - vec4(specrgb*shadfac, 0.0);
}

#if 0
void shade_one_light(vec4 col, float ref, vec4 spec, float specfac, float hard, vec3 normal, vec3 lv, float visifac, out vec4 outcol)
{
	vec3 v = -normalize(varposition);
	float inp;

	inp = max(dot(-normal, lv), 0.0);

	outcol = visifac*material_lambert_diff(inp)*ref*col;
	outcol += visifac*material_cooktorr_spec(-normal, lv, v, hard)*spec*specfac;
}

void material_simple(vec4 col, float ref, vec4 spec, float specfac, float hard, vec3 normal, out vec4 combined)
{
	vec4 outcol;

	shade_one_light(col, ref, spec, specfac, hard, normal, vec3(-0.300, 0.300, 0.900), 1.0, outcol);
	combined = outcol*vec4(0.9, 0.9, 0.9, 1.0);
	shade_one_light(col, ref, spec, specfac, hard, normal, vec3(0.500, 0.500, 0.100), 1.0, outcol);
	combined += outcol*vec4(0.2, 0.2, 0.5, 1.0);
}
#endif

void readshadowbuf(sampler2D shadowmap, float xs, float ys, float zs, float bias, out float result)
{
	float zsamp = texture2D(shadowmap, vec2(xs, ys)).x;

	if(zsamp > zs)
		result = 1.0;
	else if(zsamp < zs-bias)
		result = 0.0;
	else {
		float temp = (zs-zsamp)/bias;
		result = 1.0 - temp*temp;
	}
}

void test_shadowbuf(mat4 viewinvmat, vec3 rco, sampler2D shadowmap, mat4 shadowpersmat, float shadowbias, float inp, out float result)
{
	if(inp <= 0.0) {
		result = 0.0;
	}
	else {
		vec4 co = shadowpersmat*(viewinvmat*vec4(rco, 1.0));

		/* note that as opposed to the blender renderer, the range of
		   these coordinates is 0.0..1.0 instead of -1.0..1.0, since
		   that seems to correspond to the opengl depth buffer */
		float xs = co.x/co.w;
		float ys = co.y/co.w;
		float zs = co.z/co.w;

		if(zs >= 1.0)
			result = 0.0;
		else if(zs <= 0.0)
			result = 1.0;
		else {
			float bias = (1.5 - inp*inp)*shadowbias;

			readshadowbuf(shadowmap, xs, ys, zs, bias, result);
		}
	}
}

