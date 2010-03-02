/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>

#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "BLI_math.h"

#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "material.h"
#include "render_types.h"
#include "shading.h"
#include "texture_stack.h"
#include "render_types.h"

/******************************* Old Shaders *********************************/

/* mix of 'real' fresnel and allowing control. grad defines blending gradient */
float fresnel_fac(float *view, float *vn, float grad, float fac)
{
	float t1, t2;
	
	if(fac==0.0f) return 1.0f;
	
	t1= (view[0]*vn[0] + view[1]*vn[1] + view[2]*vn[2]);
	if(t1>0.0f)  t2= 1.0f+t1;
	else t2= 1.0f-t1;
	
	t2= grad + (1.0f-grad)*pow(t2, fac);
	
	if(t2<0.0f) return 0.0f;
	else if(t2>1.0f) return 1.0f;
	return t2;
}

static float spec(float inp, int hard)	
{
	float b1;
	
	if(inp>=1.0f) return 1.0f;
	else if (inp<=0.0f) return 0.0f;
	
	b1= inp*inp;
	/* avoid FPE */
	if(b1<0.01f) b1= 0.01f;	
	
	if((hard & 1)==0)  inp= 1.0f;
	if(hard & 2)  inp*= b1;
	b1*= b1;
	if(hard & 4)  inp*= b1;
	b1*= b1;
	if(hard & 8)  inp*= b1;
	b1*= b1;
	if(hard & 16) inp*= b1;
	b1*= b1;

	/* avoid FPE */
	if(b1<0.001f) b1= 0.0f;	

	if(hard & 32) inp*= b1;
	b1*= b1;
	if(hard & 64) inp*=b1;
	b1*= b1;
	if(hard & 128) inp*=b1;

	if(b1<0.001f) b1= 0.0f;	

	if(hard & 256) {
		b1*= b1;
		inp*=b1;
	}

	return inp;
}

static float brdf_specular_phong( float *n, float *l, float *v, int hard, int tangent )
{
	float h[3];
	float rslt;
	
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	normalize_v3(h);
	
	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];
	if(tangent) rslt= sasqrt(1.0f - rslt*rslt);
		
	if( rslt > 0.0f ) rslt= spec(rslt, hard);
	else rslt = 0.0f;
	
	return rslt;
}


/* reduced cook torrance spec (for off-specular peak) */
static float brdf_specular_cook_torrance(float *n, float *l, float *v, int hard, int tangent)
{
	float i, nh, nv, h[3];

	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	normalize_v3(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2];
	if(tangent) nh= sasqrt(1.0f - nh*nh);
	else if(nh<0.0f) return 0.0f;
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
	if(tangent) nv= sasqrt(1.0f - nv*nv);
	else if(nv<0.0f) nv= 0.0f;

	i= spec(nh, hard);

	i= i/(0.1+nv);
	return i;
}

/* Blinn spec */
static float brdf_specular_blinn(float *n, float *l, float *v, float refrac, float spec_power, int tangent)
{
	float i, nh, nv, nl, vh, h[3];
	float a, b, c, g=0.0f, p, f, ang;

	if(refrac < 1.0f) return 0.0f;
	if(spec_power == 0.0f) return 0.0f;
	
	/* conversion from 'hardness' (1-255) to 'spec_power' (50 maps at 0.1) */
	if(spec_power<100.0f)
		spec_power= sqrt(1.0f/spec_power);
	else spec_power= 10.0f/spec_power;
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	normalize_v3(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(tangent) nh= sasqrt(1.0f - nh*nh);
	else if(nh<0.0f) return 0.0f;

	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(tangent) nv= sasqrt(1.0f - nv*nv);
	if(nv<=0.01f) nv= 0.01f;				/* hrms... */

	nl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(tangent) nl= sasqrt(1.0f - nl*nl);
	if(nl<=0.01f) {
		return 0.0f;
	}

	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and half-way vector */
	if(vh<=0.0f) vh= 0.01f;

	a = 1.0f;
	b = (2.0f*nh*nv)/vh;
	c = (2.0f*nh*nl)/vh;

	if( a < b && a < c ) g = a;
	else if( b < a && b < c ) g = b;
	else if( c < a && c < b ) g = c;

	p = sqrt( (double)((refrac * refrac)+(vh*vh)-1.0f) );
	f = (((p-vh)*(p-vh))/((p+vh)*(p+vh)))*(1+((((vh*(p+vh))-1.0f)*((vh*(p+vh))-1.0f))/(((vh*(p-vh))+1.0f)*((vh*(p-vh))+1.0f))));
	ang = saacos(nh);

	i= f * g * exp((double)(-(ang*ang) / (2.0f*spec_power*spec_power)));
	if(i<0.0f) i= 0.0f;
	
	return i;
}

/* cartoon render spec */
static float brdf_specular_toon( float *n, float *l, float *v, float size, float smooth, int tangent)
{
	float h[3];
	float ang;
	float rslt;
	
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	normalize_v3(h);
	
	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];
	if(tangent) rslt = sasqrt(1.0f - rslt*rslt);
	
	ang = saacos( rslt ); 
	
	if( ang < size ) rslt = 1.0f;
	else if( ang >= (size + smooth) || smooth == 0.0f ) rslt = 0.0f;
	else rslt = 1.0f - ((ang - size) / smooth);
	
	return rslt;
}

/* Ward isotropic gaussian spec */
static float brdf_specular_ward_iso( float *n, float *l, float *v, float rms, int tangent)
{
	float i, nh, nv, nl, h[3], angle, alpha;


	/* half-way vector */
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	normalize_v3(h);

	nh = n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(tangent) nh = sasqrt(1.0f - nh*nh);
	if(nh<=0.0f) nh = 0.001f;
	
	nv = n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(tangent) nv = sasqrt(1.0f - nv*nv);
	if(nv<=0.0f) nv = 0.001f;

	nl = n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(tangent) nl = sasqrt(1.0f - nl*nl);
	if(nl<=0.0f) nl = 0.001f;

	angle = tan(saacos(nh));
	alpha = MAX2(rms, 0.001f);

	i= nl * (1.0f/(4.0f*M_PI*alpha*alpha)) * (exp( -(angle*angle)/(alpha*alpha))/(sqrt(nv*nl)));

	return i;
}

/* cartoon render diffuse */
static float brdf_diffuse_toon( float *n, float *l, float size, float smooth )
{
	float rslt, ang;

	rslt = n[0]*l[0] + n[1]*l[1] + n[2]*l[2];

	ang = saacos( (double)(rslt) );

	if( ang < size ) rslt = 1.0f;
	else if( ang >= (size + smooth) || smooth == 0.0f ) rslt = 0.0f;
	else rslt = 1.0f - ((ang - size) / smooth);

	return rslt;
}

/* Oren Nayar diffuse */

/* 'nl' is either dot product, or return value of area light */
/* in latter case, only last multiplication uses 'nl' */
static float brdf_diffuse_oren_nayar(float nl, float *n, float *l, float *v, float rough )
{
	float i, nh, nv, vh, realnl, h[3];
	float a, b, t, A, B;
	float Lit_A, View_A, Lit_B[3], View_B[3];
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	normalize_v3(h);
	
	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(nh<0.0f) nh = 0.0f;
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(nv<=0.0f) nv= 0.0f;
	
	realnl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(realnl<=0.0f) return 0.0f;
	if(nl<0.0f) return 0.0f;		/* value from area light */
	
	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and halfway vector */
	if(vh<=0.0f) vh= 0.0f;
	
	Lit_A = saacos(realnl);
	View_A = saacos( nv );
	
	Lit_B[0] = l[0] - (realnl * n[0]);
	Lit_B[1] = l[1] - (realnl * n[1]);
	Lit_B[2] = l[2] - (realnl * n[2]);
	normalize_v3( Lit_B );
	
	View_B[0] = v[0] - (nv * n[0]);
	View_B[1] = v[1] - (nv * n[1]);
	View_B[2] = v[2] - (nv * n[2]);
	normalize_v3( View_B );
	
	t = Lit_B[0]*View_B[0] + Lit_B[1]*View_B[1] + Lit_B[2]*View_B[2];
	if( t < 0 ) t = 0;
	
	if( Lit_A > View_A ) {
		a = Lit_A;
		b = View_A;
	}
	else {
		a = View_A;
		b = Lit_A;
	}
	
	A = 1.0f - (0.5f * ((rough * rough) / ((rough * rough) + 0.33f)));
	B = 0.45f * ((rough * rough) / ((rough * rough) + 0.09f));
	
	b*= 0.95f;	/* prevent tangens from shooting to inf, 'nl' can be not a dot product here. */
				/* overflow only happens with extreme size area light, and higher roughness */
	i = nl * ( A + ( B * t * sin(a) * tan(b) ) );
	
	return i;
}

/* Minnaert diffuse */
static float brdf_diffuse_minnaert(float nl, float *n, float *v, float darkness)
{

	float i, nv;

	/* nl = dot product between surface normal and light vector */
	if (nl <= 0.0f)
		return 0.0f;

	/* nv = dot product between surface normal and view vector */
	nv = n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
	if (nv < 0.0f)
		nv = 0.0f;

	if (darkness <= 1.0f)
		i = nl * pow(MAX2(nv*nl, 0.1f), (darkness - 1.0f) ); /*The Real model*/
	else
		i = nl * pow( (1.001f - nv), (darkness  - 1.0f) ); /*Nvidia model*/

	return i;
}

static float brdf_diffuse_fresnel(float *vn, float *lv, float *view, float fac_i, float fac)
{
	return fresnel_fac(lv, vn, fac_i, fac);
}

static float unclamped_smoothstep(float x)
{
	if(x>0.0f && x<1.0f)
		x= x*x*(3.0 - 2.0*x);
	
	return x;
}

static float *diffuse_tangent(ShadeMaterial *mat, ShadeGeometry *geom, float tang[3], float lv[3])
{
	/* tangent case; calculate fake face normal, aligned with lampvector */	
	/* note, tang==vn is used as tangent trigger for buffer shadow */
	Material *ma= mat->mat;
	float *vn= geom->vn;

	if(geom->tangentvn) {
		float cross[3], nstrand[3], blend;

		if(ma->mode & MA_STR_SURFDIFF) {
			cross_v3_v3v3(cross, geom->surfnor, vn);
			cross_v3_v3v3(nstrand, vn, cross);

			blend= dot_v3v3(nstrand, geom->surfnor);
			blend= 1.0f - blend;
			CLAMP(blend, 0.0f, 1.0f);

			interp_v3_v3v3(tang, nstrand, geom->surfnor, blend);
			normalize_v3(tang);
		}
		else {
			cross_v3_v3v3(cross, lv, vn);
			cross_v3_v3v3(tang, cross, vn);
			normalize_v3(tang);
		}

		if(ma->strand_surfnor > 0.0f) {
			if(ma->strand_surfnor > geom->surfdist) {
				blend= (ma->strand_surfnor - geom->surfdist)/ma->strand_surfnor;
				interp_v3_v3v3(tang, tang, geom->surfnor, blend);
				normalize_v3(tang);
			}
		}

		tang[0]= -tang[0];tang[1]= -tang[1];tang[2]= -tang[2];
		vn= tang;
	}
	else if (ma->mode & MA_TANGENT_V) {
		float cross[3];
		cross_v3_v3v3(cross, lv, geom->tang);
		cross_v3_v3v3(tang, cross, geom->tang);
		normalize_v3(tang);
		tang[0]= -tang[0];tang[1]= -tang[1];tang[2]= -tang[2];
		vn= tang;
	}

	return vn;
}

/* r,g,b denote energy, ramp is used with different values to make new material color */
static void diffuse_color_ramp(float out[3], Material *ma, float rgb[3], float is, float view[3], float vn[3])
{
	float col[4], fac=0;

	copy_v3_v3(out, rgb);
	
	/* MA_RAMP_IN_RESULT is exceptional */
	if(ma->ramp_col && (ma->mode & MA_RAMP_COL)) {
		/* input */
		switch(ma->rampin_col) {
		case MA_RAMP_IN_SHADER:
			fac= is;
			break;
		case MA_RAMP_IN_NOR:
			fac= dot_v3v3(view, vn);
			break;
		default:
			return;
		}

		do_colorband(ma->ramp_col, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_col;

		ramp_blend(ma->rampblend_col, out, out+1, out+2, fac, col);
	}
}

/* is = dot product shade, t = spec energy */
static void specular_color_ramp(float out[3], Material *ma, float rgb[3], float is, float view[3], float vn[3])
{
	float col[4], fac=0.0f;
	
	copy_v3_v3(out, rgb);

	/* MA_RAMP_IN_RESULT is exception */
	if(ma->ramp_spec && (ma->mode & MA_RAMP_SPEC)) {
		/* input */
		switch(ma->rampin_spec) {
		case MA_RAMP_IN_SHADER:
			fac= is;
			break;
		case MA_RAMP_IN_NOR:
			fac= dot_v3v3(view, vn);
			break;
		default:
			return;
		}
		
		do_colorband(ma->ramp_spec, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_spec;
		
		ramp_blend(ma->rampblend_spec, out, out+1, out+2, fac, col);
	}
}

static void diffuse_shader(float diff[3], ShadeMaterial *mat, ShadeGeometry *geom, float *lv)
{
	Material *ma= mat->mat;
	float *view= geom->view;
	float fac, ramp[3], *vn, inp, vnor[3];

	if(mat->refl == 0.0) {
		zero_v3(diff);
		return;
	}

	vn= diffuse_tangent(mat, geom, vnor, lv);
	inp= dot_v3v3(vn, lv);
	inp= MAX2(inp, 0.0f);

	/* diffuse shaders (oren nayer gets inp from area light) */
	if(ma->diff_shader==MA_DIFF_ORENNAYAR)
		fac= brdf_diffuse_oren_nayar(inp, vn, lv, view, ma->roughness);
	else if(ma->diff_shader==MA_DIFF_TOON)
		fac= brdf_diffuse_toon(vn, lv, ma->param[0], ma->param[1]);
	else if(ma->diff_shader==MA_DIFF_MINNAERT)
		fac= brdf_diffuse_minnaert(inp, vn, view, ma->darkness);
	else if(ma->diff_shader==MA_DIFF_FRESNEL)
		fac= brdf_diffuse_fresnel(vn, lv, view, ma->param[0], ma->param[1]);
	else
		fac= inp; /* Lambert */

	/* nicer termination of shades */
	if(ma->shade_flag & MA_CUBIC)
		fac= unclamped_smoothstep(fac);
	
	diffuse_color_ramp(ramp, ma, &mat->r, fac, view, vn);
	mul_v3_v3fl(diff, ramp, fac*mat->refl);
}

static void specular_shader(float spec[3], ShadeMaterial *mat, ShadeGeometry *geom, float *lv)
{
	Material *ma= mat->mat;
	float *vn, *view= geom->view;
	float fac, ramp[3];
	int hard= mat->har;
	int tangent= (geom->tangentvn) || (ma->mode & MA_TANGENT_V);

	if(mat->spec == 0.0) {
		zero_v3(spec);
		return;
	}

	vn= geom->vn;	// bring back original vector, we use special specular shaders for tangent
	if(ma->mode & MA_TANGENT_V)
		vn= geom->tang;

	if(ma->spec_shader==MA_SPEC_PHONG) 
		fac= brdf_specular_phong(vn, lv, view, hard, tangent);
	else if(ma->spec_shader==MA_SPEC_COOKTORR) 
		fac= brdf_specular_cook_torrance(vn, lv, view, hard, tangent);
	else if(ma->spec_shader==MA_SPEC_BLINN) 
		fac= brdf_specular_blinn(vn, lv, view, ma->refrac, (float)hard, tangent);
	else if(ma->spec_shader==MA_SPEC_WARDISO)
		fac= brdf_specular_ward_iso(vn, lv, view, ma->rms, tangent);
	else 
		fac= brdf_specular_toon(vn, lv, view, ma->param[2], ma->param[3], tangent);
	
	specular_color_ramp(ramp, ma, &mat->specr, fac, view, vn);
	mul_v3_v3fl(spec, ramp, fac*mat->spec);
}

/****************************** Material API *********************************/

void mat_displacement(Render *re, ShadeInput *shi, float displacement[3])
{
	zero_v3(shi->texture.displace);
	do_material_tex(re, shi);
	copy_v3_v3(displacement, shi->texture.displace);
}

void mat_shading_begin(Render *re, ShadeInput *shi, ShadeMaterial *smat, int do_textures)
{
	Material *ma= smat->mat;
  
	/* envmap hack, always reset */
	smat->refcol[0]= smat->refcol[1]= smat->refcol[2]= smat->refcol[3]= 0.0f;

	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		smat->r= smat->vcol[0];
		smat->g= smat->vcol[1];
		smat->b= smat->vcol[2];
		if(ma->mode & (MA_FACETEXTURE_ALPHA))
			smat->alpha= smat->vcol[3];
	}

	if(do_textures && ma->texco)
		do_material_tex(re, shi);

	if(!(ma->mode & MA_SHLESS)) {
		if(ma->fresnel_tra!=0.0f) 
			smat->alpha*= fresnel_fac(shi->geometry.view, shi->geometry.vn, ma->fresnel_tra_i, ma->fresnel_tra);
			
		smat->refl *= smat->alpha;
		/* TODO NSHAD
		if(smat->mode & MA_TRANSP)
			smat->spec *= smat->spectra;*/
		smat->ray_mirror *= smat->alpha;
	}
}

void mat_shading_end(Render *re, ShadeMaterial *smat)
{
}

void mat_color(float color[3], ShadeMaterial *mat)
{
	// use to be copy_v3_v3(color, &mat->r);
	mul_v3_v3fl(color, &mat->r, mat->refl);
}

float mat_alpha(ShadeMaterial *mat)
{
	return mat->alpha;
}

void mat_bsdf_f(float bsdf[3], ShadeMaterial *mat, ShadeGeometry *geom, int thread, float lv[3], int flag)
{
	float tmp[3];

	zero_v3(bsdf);

	if(flag & BSDF_DIFFUSE) {
		diffuse_shader(tmp, mat, geom, lv);
		add_v3_v3(bsdf, tmp);
	}

	if(flag & BSDF_SPECULAR) {
		specular_shader(tmp, mat, geom, lv);
		add_v3_v3(bsdf, tmp);
	}

	mul_v3_fl(bsdf, M_1_PI);
}

void mat_bsdf_sample(float lv[3], float pdf[3], ShadeMaterial *mat, ShadeGeometry *geom, int flag, float r[2])
{
	/* TODO not implemented */
	zero_v3(lv);
	zero_v3(pdf);
}

void mat_emit(float emit[3], ShadeMaterial *mat, ShadeGeometry *geom, int thread)
{
	Material *ma= mat->mat;
	float memit= mat->emit*mat->alpha;

	if((ma->mode & (MA_VERTEXCOL|MA_VERTEXCOLP)) == MA_VERTEXCOL) {
		/* vertexcolor light */
		emit[0]= mat->r*(memit + mat->vcol[0]);
		emit[1]= mat->g*(memit + mat->vcol[1]);
		emit[2]= mat->b*(memit + mat->vcol[2]);
	}
	else {
		emit[0]= mat->r*memit;
		emit[1]= mat->g*memit;
		emit[2]= mat->b*memit;
	}
}

