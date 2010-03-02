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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <float.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_listbase.h"

#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#include "BKE_colortools.h"
#include "BKE_global.h"

#include "environment.h"
#include "lamp.h"
#include "raytrace.h"
#include "render_types.h"
#include "rendercore.h"
#include "sampler.h"
#include "shading.h"
#include "shadowbuf.h"
#include "texture_stack.h"

/******************************* Visibility ********************************/

#define LAMP_FALLOFF_MIN_DIST 1e-10f /* to division by zero */

static float lamp_falloff(LampRen *lar, float dist)
{
	float factor;

	switch(lar->falloff_type)
	{
		case LA_FALLOFF_CONSTANT:
			factor = 1.0f;
			break;
		case LA_FALLOFF_INVLINEAR:
			factor = 1.0f/(dist + lar->falloff_smooth*lar->power + LAMP_FALLOFF_MIN_DIST);
			break;
		case LA_FALLOFF_INVSQUARE:
			factor = 1.0f/(dist*dist + lar->falloff_smooth*lar->power + LAMP_FALLOFF_MIN_DIST);
			break;
		case LA_FALLOFF_CURVE:
			factor = curvemapping_evaluateF(lar->curfalloff, 0, dist/lar->dist);
			break;
		default:
			factor = 1.0f;
			break;
	}

	return factor;
}

static float lamp_sphere_factor(LampRen *lar, float dist, float fac)
{
	float t= lar->dist - dist;

	if(t <= 0.0f)
		return 0.0f;
	
	return fac*(t/lar->dist);
}

static float lamp_spot_falloff(LampRen *lar, float vec[3], float fac)
{
	float inpr, t;
	
	if(lar->mode & LA_SQUARE) {
		if(vec[0]*lar->vec[0]+vec[1]*lar->vec[1]+vec[2]*lar->vec[2]>0.0f) {
			float vecrot[3], x;
			
			/* rotate view to lampspace */
			copy_v3_v3(vecrot, vec);
			mul_m3_v3(lar->imat, vecrot);
			
			x= MAX2(fabs(vecrot[0]/vecrot[2]) , fabs(vecrot[1]/vecrot[2]));
			/* 1.0f/(sqrt(1+x*x)) is equivalent to cos(atan(x)) */
			
			inpr= 1.0f/(sqrtf(1.0f+x*x));
		}
		else inpr= 0.0f;
	}
	else {
		inpr= vec[0]*lar->vec[0]+vec[1]*lar->vec[1]+vec[2]*lar->vec[2];
	}
	
	t= lar->spotsi;
	if(inpr<=t) {
		fac= 0.0f;
	}
	else {
		t= inpr-t;

		if(t<lar->spotbl && lar->spotbl!=0.0f) {
			/* soft area */
			float i= t/lar->spotbl;
			t= i*i;
			inpr*= (3.0f*t-2.0f*t*i);
		}
		fac *= inpr;
	}

	return fac;
}

/* lampdistance and spot angle, writes in lv and dist */
float lamp_visibility(LampRen *lar, float co[3], float vn[3], float lco[3], float r_vec[3], float *r_dist)
{
	float vec[3], dist, fac;

	switch(lar->type) {
		case LA_SUN:
			dist= 1.0f;
			copy_v3_v3(vec, lar->vec);
			fac= 1.0f;
			break;

		case LA_HEMI:
			dist= 1.0f;
			if(vn) {
				/* stupid: unnormalized gives us exactly 0.5*(vn.lv) + 0.5 for lambert */
				mid_v3_v3v3(vec, vn, lar->vec);
				//normalize_v3(vec);
			}
			else
				copy_v3_v3(vec, lar->vec);
			fac= 1.0f;
			break;

		case LA_AREA:
			sub_v3_v3v3(vec, co, lco);
			dist= normalize_v3(vec);
			fac= lamp_falloff(lar, dist);

			if(fac <= 1e-6f) fac = 0.0f;
			break;

		case LA_SPOT:
		case LA_LOCAL:
		default:
			sub_v3_v3v3(vec, co, lco);
			dist= normalize_v3(vec);
			fac= lamp_falloff(lar, dist);

			if(lar->mode & LA_SPHERE)
				fac= lamp_sphere_factor(lar, dist, fac);

			if(lar->type == LA_SPOT)
				fac= lamp_spot_falloff(lar, vec, fac);
			
			if(fac <= 1e-6f) fac = 0.0f;

			break;
	}

	/* return values */
	if(r_vec) copy_v3_v3(r_vec, vec);
	if(r_dist) *r_dist= dist;

	return fac;
}

static void lamp_sample_location(float lco[3], LampRen *lar, float co[3], float r[2])
{
	switch(lar->type) {
		case LA_LOCAL:
		case LA_SPOT: {
			if(r) {
				/* sphere shadow source */
				float ru[3], rv[3], v[3], s[3], disc[3];
				
				/* calc tangent plane vectors */
				v[0] = co[0] - lar->co[0];
				v[1] = co[1] - lar->co[1];
				v[2] = co[2] - lar->co[2];
				normalize_v3(v);
				ortho_basis_v3v3_v3(ru, rv, v);
				
				/* sampling, returns quasi-random vector in area_size disc */
				sample_project_disc(disc, lar->area_size, r);

				/* distribute disc samples across the tangent plane */
				s[0] = disc[0]*ru[0] + disc[1]*rv[0];
				s[1] = disc[0]*ru[1] + disc[1]*rv[1];
				s[2] = disc[0]*ru[2] + disc[1]*rv[2];
				
				add_v3_v3v3(lco, lar->co, s);
			}
			else
				copy_v3_v3(lco, lar->co);
			break;
		}
		case LA_AREA: {
			if(r) {
				float rect[3], s[3];

				/* sampling, returns quasi-random vector in [sizex,sizey]^2 plane */
				sample_project_rect(rect, lar->area_size, lar->area_sizey, r);

				/* align samples to lamp vector */
				mul_v3_m3v3(s, lar->mat, rect);
				add_v3_v3v3(lco, lar->co, s);
			}
			else
				copy_v3_v3(lco, lar->co);
			break;
		}
		default: {
			copy_v3_v3(lco, lar->co);
			break;
		}
	}
}

int lamp_skip(Render *re, LampRen *lar, ShadeInput *shi)
{
	Material *ma= shi->material.mat;

	if(lar->type==LA_YF_PHOTON) return 1;
	if(lar->mode & LA_LAYER) if(shi->primitive.obi && (lar->lay & shi->primitive.obi->lay)==0) return 1;
	if((lar->lay & shi->shading.lay)==0) return 1;

	if(lar->power == 0.0)
		return 1;

	/* only shadow lamps shouldn't affect shadow-less materials at all */
	if((lar->mode & LA_ONLYSHADOW) && (!(ma->mode & MA_SHADOW) || !(re->params.r.mode & R_SHADOW)))
		return 1;

	/* optimisation, don't render fully black lamps */
	if(!(lar->mode & LA_TEXTURE) && is_zero_v3(&lar->r))
		return 1;

	return 0;
}

int lamp_sample(float lv[3], float lainf[3], float lashdw[3],
	Render *re, LampRen *lar, ShadeInput *shi,
	float co[3], float r[2])
{
	float lco[3], dist, fac;

	/* compute sample location, vector and distance */
	lamp_sample_location(lco, lar, co, r);
	fac= lamp_visibility(lar, co, shi->geometry.vn, lco, lv, &dist);
	if(fac == 0.0f)
		return 0;

	/* compute influence */
	copy_v3_v3(lainf, &lar->r);

	if(lar->mode & LA_TEXTURE) {
		if(lar->type==LA_SPOT && (lar->mode & LA_OSATEX)) {
			shi->geometry.osatex= 1;	/* signal for multitex() */
			
			shi->texture.dxlv[0]= lv[0] - (shi->geometry.co[0]-lar->co[0]+shi->geometry.dxco[0])/dist;
			shi->texture.dxlv[1]= lv[1] - (shi->geometry.co[1]-lar->co[1]+shi->geometry.dxco[1])/dist;
			shi->texture.dxlv[2]= lv[2] - (shi->geometry.co[2]-lar->co[2]+shi->geometry.dxco[2])/dist;
			
			shi->texture.dylv[0]= lv[0] - (shi->geometry.co[0]-lar->co[0]+shi->geometry.dyco[0])/dist;
			shi->texture.dylv[1]= lv[1] - (shi->geometry.co[1]-lar->co[1]+shi->geometry.dyco[1])/dist;
			shi->texture.dylv[2]= lv[2] - (shi->geometry.co[2]-lar->co[2]+shi->geometry.dyco[2])/dist;
		}

		do_lamp_tex(re, lar, lv, shi, lainf, LA_TEXTURE);
	}

	mul_v3_fl(lainf, fac);

	/* compute shadow */
	if((re->params.r.mode & R_SHADOW) && (shi->material.mat->mode & MA_SHADOW))
		lamp_shadow(lashdw, re, lar, shi, co, lco, lv);
	else
		lashdw[0]= lashdw[1]= lashdw[2]= 1.0f;

	return 1;
}

/***************************** spot halo ***********************************/

static void spothalo(Render *re, struct LampRen *lar, ShadeInput *shi, float *intens)
{
	double a, b, c, disc, nray[3], npos[3];
	float t0, t1 = 0.0f, t2= 0.0f, t3, haint;
	float p1[3], p2[3], ladist, maxz = 0.0f, maxy = 0.0f;
	int snijp, doclip=1, use_yco=0;
	int ok1=0, ok2=0;
	
	*intens= 0.0f;
	haint= lar->haint;
	
	if(re->cam.type == R_CAM_ORTHO) {
		/* camera pos (view vector) cannot be used... */
		/* camera position (cox,coy,0) rotate around lamp */
		p1[0]= shi->geometry.co[0]-lar->co[0];
		p1[1]= shi->geometry.co[1]-lar->co[1];
		p1[2]= -lar->co[2];
		mul_m3_v3(lar->imat, p1);
		VECCOPY(npos, p1);	// npos is double!
		
		/* pre-scale */
		npos[2]*= lar->sh_zfac;
	}
	else {
		VECCOPY(npos, lar->sh_invcampos);	/* in initlamp calculated */
	}
	
	/* rotate view */
	VECCOPY(nray, shi->geometry.view);
	mul_m3_v3_double(lar->imat, nray);
	
	if(re->db.wrld.mode & WO_MIST) {
		/* patchy... */
		haint *= environment_mist_factor(re, -lar->co[2], lar->co);
		if(haint==0.0f) {
			return;
		}
	}


	/* rotate maxz */
	if(shi->geometry.co[2]==0.0f) doclip= 0;	/* for when halo at sky */
	else {
		p1[0]= shi->geometry.co[0]-lar->co[0];
		p1[1]= shi->geometry.co[1]-lar->co[1];
		p1[2]= shi->geometry.co[2]-lar->co[2];
	
		maxz= lar->imat[0][2]*p1[0]+lar->imat[1][2]*p1[1]+lar->imat[2][2]*p1[2];
		maxz*= lar->sh_zfac;
		maxy= lar->imat[0][1]*p1[0]+lar->imat[1][1]*p1[1]+lar->imat[2][1]*p1[2];

		if( fabs(nray[2]) < DBL_EPSILON ) use_yco= 1;
	}
	
	/* scale z to make sure volume is normalized */	
	nray[2]*= lar->sh_zfac;
	/* nray does not need normalization */
	
	ladist= lar->sh_zfac*lar->dist;
	
	/* solve */
	a = nray[0] * nray[0] + nray[1] * nray[1] - nray[2]*nray[2];
	b = nray[0] * npos[0] + nray[1] * npos[1] - nray[2]*npos[2];
	c = npos[0] * npos[0] + npos[1] * npos[1] - npos[2]*npos[2];

	snijp= 0;
	if (fabs(a) < DBL_EPSILON) {
		/*
		 * Only one intersection point...
		 */
		return;
	}
	else {
		disc = b*b - a*c;
		
		if(disc==0.0) {
			t1=t2= (-b)/ a;
			snijp= 2;
		}
		else if (disc > 0.0) {
			disc = sqrt(disc);
			t1 = (-b + disc) / a;
			t2 = (-b - disc) / a;
			snijp= 2;
		}
	}
	if(snijp==2) {
		/* sort */
		if(t1>t2) {
			a= t1; t1= t2; t2= a;
		}

		/* z of intersection points with diabolo */
		p1[2]= npos[2] + t1*nray[2];
		p2[2]= npos[2] + t2*nray[2];

		/* evaluate both points */
		if(p1[2]<=0.0f) ok1= 1;
		if(p2[2]<=0.0f && t1!=t2) ok2= 1;
		
		/* at least 1 point with negative z */
		if(ok1==0 && ok2==0) return;
		
		/* intersction point with -ladist, the bottom of the cone */
		if(use_yco==0) {
			t3= (-ladist-npos[2])/nray[2];
				
			/* de we have to replace one of the intersection points? */
			if(ok1) {
				if(p1[2]<-ladist) t1= t3;
			}
			else {
				ok1= 1;
				t1= t3;
			}
			if(ok2) {
				if(p2[2]<-ladist) t2= t3;
			}
			else {
				ok2= 1;
				t2= t3;
			}
		}
		else if(ok1==0 || ok2==0) return;
		
		/* at least 1 visible interesction point */
		if(t1<0.0f && t2<0.0f) return;
		
		if(t1<0.0f) t1= 0.0f;
		if(t2<0.0f) t2= 0.0f;
		
		if(t1==t2) return;
		
		/* sort again to be sure */
		if(t1>t2) {
			a= t1; t1= t2; t2= a;
		}
		
		/* calculate t0: is the maximum visible z (when halo is intersected by face) */ 
		if(doclip) {
			if(use_yco==0) t0= (maxz-npos[2])/nray[2];
			else t0= (maxy-npos[1])/nray[1];

			if(t0<t1) return;
			if(t0<t2) t2= t0;
		}

		/* calc points */
		p1[0]= npos[0] + t1*nray[0];
		p1[1]= npos[1] + t1*nray[1];
		p1[2]= npos[2] + t1*nray[2];
		p2[0]= npos[0] + t2*nray[0];
		p2[1]= npos[1] + t2*nray[1];
		p2[2]= npos[2] + t2*nray[2];
		
			
		/* now we have 2 points, make three lengths with it */
		
		a= sqrt(p1[0]*p1[0]+p1[1]*p1[1]+p1[2]*p1[2]);
		b= sqrt(p2[0]*p2[0]+p2[1]*p2[1]+p2[2]*p2[2]);
		c= len_v3v3(p1, p2);
		
		a/= ladist;
		a= sqrt(a);
		b/= ladist; 
		b= sqrt(b);
		c/= ladist;
		
		*intens= c*( (1.0-a)+(1.0-b) );

		/* WATCH IT: do not clip a,b en c at 1.0, this gives nasty little overflows
			at the edges (especially with narrow halos) */
		if(*intens<=0.0f) return;

		/* soft area */
		/* not needed because t0 has been used for p1/p2 as well */
		/* if(doclip && t0<t2) { */
		/* 	*intens *= (t0-t1)/(t2-t1); */
		/* } */
		
		*intens *= haint;
		*intens *= shadow_halo(lar, p1, p2);
	}
}

void lamp_spothalo_render(Render *re, ShadeInput *shi, float *col, float alpha)
{
	ListBase *lights;
	GroupObject *go;
	LampRen *lar;
	float i;
	
	if(alpha==0.0f) return;
	
	lights= lamps_get(re, shi);
	for(go=lights->first; go; go= go->next) {
		lar= go->lampren;
		if(lar==NULL) continue;
		
		if(lar->type==LA_SPOT && (lar->mode & LA_HALO) && (lar->buftype != LA_SHADBUF_DEEP) && lar->haint>0) {
			
			if(lar->mode & LA_LAYER) 
				if(shi->primitive.vlr && (lar->lay & shi->primitive.obi->lay)==0) 
					continue;
			if((lar->lay & shi->shading.lay)==0) 
				continue;
			
			spothalo(re, lar, shi, &i);
			if(i>0.0f) {
				col[3]+= i*alpha;			// all premul
				col[0]+= i*lar->r*alpha;
				col[1]+= i*lar->g*alpha;
				col[2]+= i*lar->b*alpha;	
			}
		}
	}
	/* clip alpha, is needed for unified 'alpha threshold' (vanillaRenderPipe.c) */
	if(col[3]>1.0f) col[3]= 1.0f;
}

/*************** lights list *******************/

ListBase *lamps_get(Render *re, ShadeInput *shi)
{
	if(re->params.r.scemode & R_PREVIEWBUTS)
		return &re->db.lights;
	if(shi->material.light_override)
		return &shi->material.light_override->gobject;
	if(shi->material.mat && shi->material.mat->group)
		return &shi->material.mat->group->gobject;
	
	return &re->db.lights;
}

/****************** shadow ***********************/

void lamp_shadow(float lashdw[3], Render *re, LampRen *lar, ShadeInput *shi,
	float co[3], float lco[3], float lv[3])
{
	LampShadowSubSample *lss= &(lar->shadsamp[shi->shading.thread].s[shi->shading.sample]);
	/* TODO strand render bias is not same as it was before, but also was
	   mixed together with the shading code which doesn't work anymore now */
	float inp = (shi->geometry.tangentvn)? 1.0f: dot_v3v3(shi->geometry.vn, lv);
	int do_real = (lar->ray_totsamp > 1)? 1: shi->shading.depth; // TODO caching doesn't work for multi-sample

	lashdw[0]= lashdw[1]= lashdw[2]= 1.0f;

	if(!(lar->shb || (lar->mode & LA_SHAD_RAY)))
		return;
	
	if(do_real || lss->samplenr!=shi->shading.samplenr) {
		if(lar->shb) {
			float fac;

			if(lar->buftype==LA_SHADBUF_IRREGULAR)
				fac= irregular_shadowbuf_test(re, lar->shb, shi);
			else
				fac= shadowbuf_test(re, lar->shb, co, shi->geometry.dxco, shi->geometry.dyco, inp, shi->material.mat->lbias);

			lashdw[0]= lashdw[1]= lashdw[2]= fac;
		}
		else if(lar->mode & LA_SHAD_RAY)
			ray_shadow_single(lashdw, re, shi, lar, co, lco);

		if(!is_one_v3(lashdw)) {
			float col[3];

			/* colored shadows */
			col[0]= lar->shdwr;
			col[1]= lar->shdwg;
			col[2]= lar->shdwb;

			if(!is_zero_v3(col)) {
				if(lar->mode & LA_SHAD_TEX)	do_lamp_tex(re, lar, lv, shi, col, LA_SHAD_TEX);

				lashdw[0] += (1.0f-lashdw[0])*col[0];
				lashdw[1] += (1.0f-lashdw[1])*col[1];
				lashdw[2] += (1.0f-lashdw[2])*col[2];
			}
		}
		
		if(shi->shading.depth==0) {
			copy_v3_v3(lss->lashdw, lashdw);
			lss->samplenr= shi->shading.samplenr;
		}
	}
	else
		copy_v3_v3(lashdw, lss->lashdw);
}

/***************************** Lamp create/free ******************************/

/* If lar takes more lamp data, the decoupling will be better. */
GroupObject *lamp_create(Render *re, Object *ob)
{
	Lamp *la= ob->data;
	LampRen *lar;
	GroupObject *go;
	float mat[4][4], angle, xn, yn;
	int c;

	/* previewrender sets this to zero... prevent accidents */
	if(la==NULL) return NULL;
	
	/* prevent only shadow from rendering light */
	if(la->mode & LA_ONLYSHADOW)
		if((re->params.r.mode & R_SHADOW)==0)
			return NULL;
	
	re->db.totlamp++;
	
	/* groups is used to unify support for lightgroups, this is the global lightgroup */
	go= MEM_callocN(sizeof(GroupObject), "groupobject");
	BLI_addtail(&re->db.lights, go);
	go->ob= ob;
	/* lamprens are in own list, for freeing */
	lar= (LampRen *)MEM_callocN(sizeof(LampRen),"lampren");
	BLI_addtail(&re->db.lampren, lar);
	go->lampren= lar;

	mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
	invert_m4_m4(ob->imat, mat);

	copy_m3_m4(lar->mat, mat);
	copy_m3_m4(lar->imat, ob->imat);

	lar->bufsize = la->bufsize;
	lar->samp = la->samp;
	lar->buffers= la->buffers;
	if(lar->buffers==0) lar->buffers= 1;
	lar->buftype= la->buftype;
	lar->filtertype= la->filtertype;
	lar->soft = la->soft;
	lar->shadhalostep = la->shadhalostep;
	lar->clipsta = la->clipsta;
	lar->clipend = la->clipend;
	
	lar->bias = la->bias;
	lar->compressthresh = la->compressthresh;

	lar->type= la->type;
	lar->mode= la->mode;

	lar->power= la->energy;
	if(la->mode & LA_NEG) lar->power= -lar->power;

	lar->vec[0]= -mat[2][0];
	lar->vec[1]= -mat[2][1];
	lar->vec[2]= -mat[2][2];
	normalize_v3(lar->vec);
	lar->co[0]= mat[3][0];
	lar->co[1]= mat[3][1];
	lar->co[2]= mat[3][2];
	lar->dist= la->dist;
	lar->haint= la->haint;
	lar->distkw= lar->dist*lar->dist;
	lar->r= lar->power*la->r;
	lar->g= lar->power*la->g;
	lar->b= lar->power*la->b;
	lar->shdwr= la->shdwr;
	lar->shdwg= la->shdwg;
	lar->shdwb= la->shdwb;
	lar->k= la->k;

	// area
	lar->ray_samp= la->ray_samp;
	lar->ray_sampy= la->ray_sampy;
	lar->ray_sampz= la->ray_sampz;
	
	lar->area_size= la->area_size;
	lar->area_sizey= la->area_sizey;
	lar->area_sizez= la->area_sizez;

	lar->area_shape= la->area_shape;
	
	lar->ray_samp_method= la->ray_samp_method;
	lar->ray_samp_type= la->ray_samp_type;
	
	lar->adapt_thresh= la->adapt_thresh;
	
	if( ELEM(lar->type, LA_SPOT, LA_LOCAL)) {
		lar->ray_totsamp= lar->ray_samp*lar->ray_samp;
		lar->area_shape = LA_AREA_SQUARE;
		lar->area_sizey= lar->area_size;
	}
	else if(lar->type==LA_AREA) {
		switch(lar->area_shape) {
		case LA_AREA_SQUARE:
			lar->ray_totsamp= lar->ray_samp*lar->ray_samp;
			lar->ray_sampy= lar->ray_samp;
			lar->area_sizey= lar->area_size;
			break;
		case LA_AREA_RECT:
			lar->ray_totsamp= lar->ray_samp*lar->ray_sampy;
			break;
		case LA_AREA_CUBE:
			lar->ray_totsamp= lar->ray_samp*lar->ray_samp*lar->ray_samp;
			lar->ray_sampy= lar->ray_samp;
			lar->ray_sampz= lar->ray_samp;
			lar->area_sizey= lar->area_size;
			lar->area_sizez= lar->area_size;
			break;
		case LA_AREA_BOX:
			lar->ray_totsamp= lar->ray_samp*lar->ray_sampy*lar->ray_sampz;
			break;
		}
	}
	else if(lar->type==LA_SUN){
		lar->ray_totsamp= lar->ray_samp*lar->ray_samp;
		lar->area_shape = LA_AREA_SQUARE;
		lar->area_sizey= lar->area_size;

		environment_sun_init(lar, la, ob->obmat);
	}
	else lar->ray_totsamp= 0;

	if(lar->ray_totsamp <= 1 && lar->type != LA_AREA) {
		lar->area_size= 0.0f;
		lar->area_sizey= 0.0f;
	}
	
	lar->spotsi= la->spotsize;
	if(lar->mode & LA_HALO) {
		if(lar->spotsi>170.0) lar->spotsi= 170.0;
	}
	lar->spotsi= cos( M_PI*lar->spotsi/360.0 );
	lar->spotbl= (1.0-lar->spotsi)*la->spotblend;

	memcpy(lar->mtex, la->mtex, MAX_MTEX*sizeof(void *));

	lar->lay= ob->lay & 0xFFFFFF;	// higher 8 bits are localview layers

	lar->falloff_type = la->falloff_type;
	lar->falloff_smooth = la->falloff_smooth;
	lar->curfalloff = curvemapping_copy(la->curfalloff);

	if(lar->type==LA_SPOT) {

		normalize_v3(lar->imat[0]);
		normalize_v3(lar->imat[1]);
		normalize_v3(lar->imat[2]);

		xn= saacos(lar->spotsi);
		xn= sin(xn)/cos(xn);
		lar->spottexfac= 1.0/(xn);

		if(lar->mode & LA_ONLYSHADOW) {
			if((lar->mode & (LA_SHAD_BUF|LA_SHAD_RAY))==0) lar->mode -= LA_ONLYSHADOW;
		}

	}

	/* set flag for spothalo en initvars */
	if(la->type==LA_SPOT && (la->mode & LA_HALO) && (la->buftype != LA_SHADBUF_DEEP)) {
		if(la->haint>0.0) {
			re->params.flag |= R_LAMPHALO;

			/* camera position (0,0,0) rotate around lamp */
			lar->sh_invcampos[0]= -lar->co[0];
			lar->sh_invcampos[1]= -lar->co[1];
			lar->sh_invcampos[2]= -lar->co[2];
			mul_m3_v3(lar->imat, lar->sh_invcampos);

			/* z factor, for a normalized volume */
			angle= saacos(lar->spotsi);
			xn= lar->spotsi;
			yn= sin(angle);
			lar->sh_zfac= yn/xn;
			/* pre-scale */
			lar->sh_invcampos[2]*= lar->sh_zfac;

		}
	}
	else if(la->type==LA_HEMI) {
		lar->mode &= ~(LA_SHAD_RAY|LA_SHAD_BUF);
	}

	for(c=0; c<MAX_MTEX; c++) {
		if(la->mtex[c] && la->mtex[c]->tex) {
			if (la->mtex[c]->mapto & LAMAP_COL) 
				lar->mode |= LA_TEXTURE;
			if (la->mtex[c]->mapto & LAMAP_SHAD)
				lar->mode |= LA_SHAD_TEX;

			if(G.rendering) {
				if(re->params.osa) {
					if(la->mtex[c]->tex->type==TEX_IMAGE) lar->mode |= LA_OSATEX;
				}
			}
		}
	}
	/* yafray: shadow flag should not be cleared, only used with internal renderer */
	if (re->params.r.renderer==R_INTERN) {
		/* to make sure we can check ray shadow easily in the render code */
		if(lar->mode & LA_SHAD_RAY) {
			if( (re->params.r.mode & R_RAYTRACE)==0)
				lar->mode &= ~LA_SHAD_RAY;
		}
	

		if(re->params.r.mode & R_SHADOW) {
			
			if (la->type==LA_SPOT && (lar->mode & LA_SHAD_BUF) ) {
				/* Per lamp, one shadow buffer is made. */
				lar->bufflag= la->bufflag;
				copy_m4_m4(mat, ob->obmat);
				shadowbuf_create(re, lar, mat);
			}
			
			
			/* this is the way used all over to check for shadow */
			if(lar->shb || (lar->mode & LA_SHAD_RAY)) {
				LampShadowSample *ls;
				LampShadowSubSample *lss;
				int a, b;

				memset(re->sample.shadowsamplenr, 0, sizeof(re->sample.shadowsamplenr));
				
				lar->shadsamp= MEM_mallocN(re->params.r.threads*sizeof(LampShadowSample), "lamp shadow sample");
				ls= lar->shadsamp;

				/* shadfacs actually mean light, let's put them to 1 to prevent unitialized accidents */
				for(a=0; a<re->params.r.threads; a++, ls++) {
					lss= ls->s;
					for(b=0; b<re->params.r.osa; b++, lss++) {
						lss->samplenr= -1;	/* used to detect whether we store or read */
						lss->lashdw[0]= 1.0f;
						lss->lashdw[1]= 1.0f;
						lss->lashdw[2]= 1.0f;
					}
				}
			}
		}
	}
	
	return go;
}

void lamp_free(LampRen *lar)
{
	shadowbuf_free(lar);

	if(lar->shadsamp) MEM_freeN(lar->shadsamp);
	if(lar->sunsky) environment_sun_free(lar);
	curvemapping_free(lar->curfalloff);
}

/************************** Lightgroup create/free ***************************/

/* layflag: allows material group to ignore layerflag */
void lightgroup_create(Render *re, Group *group, int exclusive)
{
	GroupObject *go, *gol;
	
	group->id.flag &= ~LIB_DOIT;

	/* it's a bit too many loops in loops... but will survive */
	/* note that 'exclusive' will remove it from the global list */
	for(go= group->gobject.first; go; go= go->next) {
		go->lampren= NULL;
		
		if(go->ob->lay & re->db.scene->lay) {
			if(go->ob && go->ob->type==OB_LAMP) {
				for(gol= re->db.lights.first; gol; gol= gol->next) {
					if(gol->ob==go->ob) {
						go->lampren= gol->lampren;
						break;
					}
				}
				if(go->lampren==NULL) 
					gol= lamp_create(re, go->ob);
				if(gol && exclusive) {
					BLI_remlink(&re->db.lights, gol);
					MEM_freeN(gol);
				}
			}
		}
	}
}

void lightgroup_free(Render *re)
{
}

