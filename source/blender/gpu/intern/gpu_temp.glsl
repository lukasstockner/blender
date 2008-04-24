
#if 0
const char *blend_shaders[16] = {
/* mix */
"	outcol = vec4(mix(col1.rgb, col2.rgb, fac), col1.a);\n",
/* add */
"	outcol = vec4(col1.rgb + fac*col2.rgb, col1.a);\n",
/* mult */
"	outcol = vec4(col1.rgb*(vec3(1.0-fac) + fac*col2.rgb), col1.a);\n",
/* sub */
"	outcol = vec4(col1.rgb - fac*col2.rgb, col1.a);\n",
/* screen */
"	vec3 s = vec3(1.0) - fac*col2.rgb;\n"
"	outcol = vec4(vec3(1.0) - s*(vec3(1.0) - col1.rgb), col1.a);\n",
/* div */
"	vec3 b = vec3(equal(col2.rgb, vec3(0.0)));\n"
"	outcol = (vec3(1.0)-b)*col1.rgb + b*((1.0-fac)*col1.rgb + fac*col1.rgb/col2.rgb);\n",
/* diff */
"	outcol = (1.0-fac)*col1.rgb + fac*abs(col1.rgb-col2.rgb);\n",

0, 0,

/* overlay */
"	bvec3 b = lessThan(col1.rgb, vec3(0.5));\n"
"	vec3 s  (1.0) - ((1.0-fac) + 2.0*fac*(1.0 - col2.rgb))*(vec3(1.0) - col1.rgb);\n"
"	outcol = vec4(vec3(1.0) - s*(vec3(1.0) - col1.rgb, col1.a));\n",

0, 0, 0, 0, 0, 0};

const char *filter_shader =
"	vec3 a, b, c, d, e, f, g, h, i;\n"
"	vec2 st = gl_TexCoord[0].st;\n"
"\n"
"	vec4 center = texture2D(tex, st);\n"
"\n"
"	a = texture2D(tex, st + vec2(-dx, -dy)).rgb;\n"
"	b = texture2D(tex, st + vec2(  0, -dy)).rgb;\n"
"	c = texture2D(tex, st + vec2( dx, -dy)).rgb;\n"
"	d = texture2D(tex, st + vec2(-dx,   0)).rgb;\n"
"	e = center.rgb;\n"
"	f = texture2D(tex, st + vec2( dx,   0)).rgb;\n"
"	g = texture2D(tex, st + vec2(-dx,  dy)).rgb;\n"
"	h = texture2D(tex, st + vec2(  0,  dy)).rgb;\n"
"	i = texture2D(tex, st + vec2( dx,  dy)).rgb;\n"
"\n"
"	vec3 conv = a*fmat[0][0] + b*fmat[0][1] + c*fmat[0][2]\n"
"		 + d*fmat[1][0] + e*fmat[1][1] + f*fmat[1][2]\n"
"		 + g*fmat[2][0] + h*fmat[2][1] + i*fmat[2][2];\n"
"\n"
"	outcol = vec4(conv, center.a);\n";

const char *filter_edge_shader =
"	vec3 a, b, c, d, e, f, g, h, i;\n"
"	vec4 center = texture2D(tex, gl_TexCoord[0].st);\n"
"\n"
"	a = texture2D(tex, gl_TexCoord[0].st + vec2(-dx, -dy)).rgb;\n"
"	b = texture2D(tex, gl_TexCoord[0].st + vec2(  0, -dy)).rgb;\n"
"	c = texture2D(tex, gl_TexCoord[0].st + vec2( dx, -dy)).rgb;\n"
"	d = texture2D(tex, gl_TexCoord[0].st + vec2(-dx,   0)).rgb;\n"
"	e = center.rgb;\n"
"	f = texture2D(tex, gl_TexCoord[0].st + vec2( dx,   0)).rgb;\n"
"	g = texture2D(tex, gl_TexCoord[0].st + vec2(-dx,  dy)).rgb;\n"
"	h = texture2D(tex, gl_TexCoord[0].st + vec2(  0,  dy)).rgb;\n"
"	i = texture2D(tex, gl_TexCoord[0].st + vec2( dx,  dy)).rgb;\n"
"\n"
"	vec3 diag = a*fmat[0][0] + e*fmat[1][1] + i*fmat[2][2];\n"
"\n"
"	vec3 conv = diag +                b*fmat[1][0] + c*fmat[2][0]\n"
"		 			 + d*fmat[0][1] +                f*fmat[2][1]\n"
"		 			 + g*fmat[0][2] + h*fmat[1][2]              ;\n"
"\n"
"	vec3 conv2 = diag +               d*fmat[1][0] + g*fmat[2][0]\n"
"					  + b*fmat[0][1] +               h*fmat[2][1]\n"
"		 			  + c*fmat[0][2] + f*fmat[1][2]             ;\n"
"\n"
"	outcol = vec4(sqrt(conv*conv + conv2*conv2), center.a);\n";

const char *alpha_over_premul_shader = 
"	if (over.a < 0.0)\n"
"		outcol = src;\n"
"	else if (fac == 1.0 && over.a > 1.0)\n"
"		outcol = over;\n"
"	else\n"
"		outcol = (1.0 - fac*over.a)*src + fac*over;\n";

const char *alpha_over_key_shader = 
"	if (over.a < 0.0)\n"
"		outcol = src;\n"
"	else if (fac == 1.0 && over.a > 1.0)\n"
"		outcol = over;\n"
"	else {\n"
"		float premul = fac*over.a;\n"
"		outcol = (1.0-premul)*src + vec4(premul*over.rgb, fac*over.a);\n"
"	}\n";

const char *blur_hor_shader = 
"	int i;\n"
"	vec2 st = gl_TexCoord[0].st;\n"
"	vec4 sum = texture2D(tex, st)*texture1D(gausstab, 0.0);\n"
"	float f;\n"
"\n"
"	for (i = 1; i < 4; i++) {\n"
"		vec2 offset = vec2(float(i)*dx, 0.0);\n"
"		float f = texture1D(gausstab, offset.x).r;\n"
"		sum += f*(texture2D(tex, st + offset) + texture2D(tex, st - offset));\n"
"	}\n"
"\n"
"	outcol = sum;\n";

const char *blur_ver_shader = 
"	int i;\n"
"	vec2 st = gl_TexCoord[0].st;\n"
"	vec4 sum = texture2D(tex, st)*texture1D(gausstab, 0.0);\n"
"	float f;\n"
"\n"
"	for (i = 1; i < 4; i++) {\n"
"		vec2 offset = vec2(0.0, float(i)*dy);\n"
"		float f = texture1D(gausstab, offset.y).r;\n"
"		sum += f*(texture2D(tex, st + offset) + texture2D(tex, st - offset));\n"
"	}\n"
"\n"
"	outcol = sum;\n";

const char *dilate_shader =
"	float a, b, c, d, e, f, g, h, i;\n"
"	vec2 st = gl_TexCoord[0].st;\n"
"\n"
"	a = dot(texture2D(tex, st + vec2(-dx, -dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	b = dot(texture2D(tex, st + vec2(  0, -dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	c = dot(texture2D(tex, st + vec2( dx, -dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	d = dot(texture2D(tex, st + vec2(-dx,   0)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	e = dot(texture2D(tex, st).rgb, vec3(0.35, 0.45, 0.2));\n"
"	f = dot(texture2D(tex, st + vec2( dx,   0)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	g = dot(texture2D(tex, st + vec2(-dx,  dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	h = dot(texture2D(tex, st + vec2(  0,  dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	i = dot(texture2D(tex, st + vec2( dx,  dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"\n"
"	float m1 = max(a, max(b, c));\n"
"	float m2 = max(d, max(f, f));\n"
"	float m3 = max(g, max(h, i));\n"
"\n"
"	outval = max(m1, max(m2, m3));\n";

const char *erode_shader =
"	float a, b, c, d, e, f, g, h, i;\n"
"	vec2 st = gl_TexCoord[0].st;\n"
"\n"
"	a = dot(texture2D(tex, st + vec2(-dx, -dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	b = dot(texture2D(tex, st + vec2(  0, -dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	c = dot(texture2D(tex, st + vec2( dx, -dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	d = dot(texture2D(tex, st + vec2(-dx,   0)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	e = dot(texture2D(tex, st).rgb, vec3(0.35, 0.45, 0.2));\n"
"	f = dot(texture2D(tex, st + vec2( dx,   0)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	g = dot(texture2D(tex, st + vec2(-dx,  dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	h = dot(texture2D(tex, st + vec2(  0,  dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"	i = dot(texture2D(tex, st + vec2( dx,  dy)).rgb, vec3(0.35, 0.45, 0.2));\n"
"\n"
"	float m1 = min(a, min(b, c));\n"
"	float m2 = min(d, min(f, f));\n"
"	float m3 = min(g, min(h, i));\n"
"\n"
"	outval = min(m1, min(m2, m3));\n";

const char *glow_shader = 
"	intensity = col.r + col.g + col.r - threshld);\n"
"	if (intensity > 0.0)\n"
"		outcol = min(vec4(clamp), col*boost*intensity;\n"
"	else\n"		
"		outcol = vec4(0.0);\n";

const char *gamma_shader = 
"	vec4 rtex = texture1D(gamma, col.r);\n"
"	vec4 gtex = texture1D(gamma, col.g);\n"
"	vec4 btex = texture1D(gamma, col.b);\n"
"	vec4 atex = texture1D(gamma, col.a);\n"
"\n"
"	float r = rtex.x + (col.r - rtex.y)*rtex.z;\n"
"	float g = gtex.x + (col.g - gtex.y)*gtex.z;\n"
"	float b = btex.x + (col.b - btex.y)*btex.z;\n"
"	float a = atex.x + (col.a - atex.y)*atex.z;\n"
"\n"
"	outcol = vec4(r, g, b, a);\n";
#endif

