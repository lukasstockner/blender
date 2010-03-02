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

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_texture_types.h"

#include "BLI_math.h"

#include "BKE_colortools.h"
#include "BKE_material.h"

#include "camera.h"
#include "environment.h"
#include "lamp.h"
#include "render_types.h"
#include "rendercore.h"
#include "sunsky.h"
#include "texture.h"
#include "texture_stack.h"

/* Only view vector is important here. Result goes to colf[3] */
static void env_shade_sky(Render *re, float *colf, float *rco, float *view, float *dxyview, short thread)
{
	float lo[3], zen[3], hor[3], blend, blendm;
	int skyflag;
	
	/* flag indicating if we render the top hemisphere */
	skyflag = WO_ZENUP;
	
	/* Some view vector stuff. */
	if(re->db.wrld.skytype & WO_SKYREAL) {
		
		blend= view[0]*re->cam.viewzvec[0]+ view[1]*re->cam.viewzvec[1]+ view[2]*re->cam.viewzvec[2];
		
		if(blend<0.0f) skyflag= 0;
		
		blend= fabsf(blend);
	}
	else if(re->db.wrld.skytype & WO_SKYPAPER) {
		blend= 0.5f + 0.5f*view[1];
	}
	else {
		/* the fraction of how far we are above the bottom of the screen */
		blend= fabsf(0.5f + view[1]);
	}
	
	copy_v3_v3(hor, &re->db.wrld.horr);
	copy_v3_v3(zen, &re->db.wrld.zenr);

	/* Careful: SKYTEX and SKYBLEND are NOT mutually exclusive! If           */
	/* SKYBLEND is active, the texture and color blend are added.           */
	if(re->db.wrld.skytype & WO_SKYTEX) {
		copy_v3_v3(lo, view);
		if(re->db.wrld.skytype & WO_SKYREAL) {
			
			mul_mat3_m4_v3(re->cam.viewinv, lo);
			
			SWAP(float, lo[1],  lo[2]);
			
		}
		do_sky_tex(re, rco, lo, dxyview, hor, zen, &blend, skyflag, thread);
	}
	
	if(blend>1.0f) blend= 1.0f;
	blendm= 1.0f-blend;
	
	/* No clipping, no conversion! */
	if(re->db.wrld.skytype & WO_SKYBLEND) {
		colf[0] = (blendm*hor[0] + blend*zen[0]);
		colf[1] = (blendm*hor[1] + blend*zen[1]);
		colf[2] = (blendm*hor[2] + blend*zen[2]);
	} else {
		/* Done when a texture was grabbed. */
		colf[0]= hor[0];
		colf[1]= hor[1];
		colf[2]= hor[2];
	}
}

/* shade sky according to sun lamps, all parameters are like env_shade_sky except sunsky*/
static void env_shade_sun(Render *re, float *colf, float *view)
{
	GroupObject *go;
	LampRen *lar;
	float sview[3];
	int do_init= 1;
	
	for(go=re->db.lights.first; go; go= go->next) {
		lar= go->lampren;
		if(lar->type==LA_SUN &&	lar->sunsky && (lar->sunsky->effect_type & LA_SUN_EFFECT_SKY)){
			float sun_col[3];
			float colorxyz[3];
			
			if(do_init) {
				
				copy_v3_v3(sview, view);
				normalize_v3(sview);
				mul_mat3_m4_v3(re->cam.viewinv, sview);
				if (sview[2] < 0.0f)
					sview[2] = 0.0f;
				normalize_v3(sview);
				do_init= 0;
			}
			
			GetSkyXYZRadiancef(lar->sunsky, sview, colorxyz);
			xyz_to_rgb(colorxyz[0], colorxyz[1], colorxyz[2], &sun_col[0], &sun_col[1], &sun_col[2], 
					   lar->sunsky->sky_colorspace);
			
			ramp_blend(lar->sunsky->skyblendtype, colf, colf+1, colf+2, lar->sunsky->skyblendfac, sun_col);
		}
	}
}


/* aerial perspective */
void atmosphere_shade_pixel(Render *re, SunSky *sunsky, float *col, float fx, float fy, float distance)
{
	float view[3];
		
	camera_raster_to_view(&re->cam, view, fx, fy);
	normalize_v3(view);
	/*mul_mat3_m4_v3(re->cam.viewinv, view);*/
	AtmospherePixleShader(sunsky, view, distance, col);
}


void environment_init(Render *re, World *world)
{
	int a;
	
	if(world) {
		re->db.wrld= *world;
		
		copy_v3_v3(re->cam.viewzvec, re->cam.viewmat[2]);
		normalize_v3(re->cam.viewzvec);
		
		for(a=0; a<MAX_MTEX; a++) 
			if(re->db.wrld.mtex[a] && re->db.wrld.mtex[a]->tex) re->db.wrld.skytype |= WO_SKYTEX;
		
		/* AO samples should be OSA minimum */
		if(re->params.osa)
			while(re->db.wrld.aosamp*re->db.wrld.aosamp < re->params.osa) 
				re->db.wrld.aosamp++;

		if(!(re->params.r.mode & R_RAYTRACE) && (re->db.wrld.ao_gather_method == WO_AOGATHER_RAYTRACE))
			re->db.wrld.mode &= ~(WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT);
	}
	else {
		memset(&re->db.wrld, 0, sizeof(World));
		
		/* for mist pass */
		re->db.wrld.miststa= re->cam.clipsta;
		re->db.wrld.mistdist= re->cam.clipend-re->cam.clipsta;
		re->db.wrld.misi= 1.0f;
	}
}

void environment_free(Render *re)
{
}

void environment_shade(Render *re, float col[3], float co[3], float view[3], float dxyview[2], int thread)
{
	env_shade_sky(re, col, co, view, dxyview, thread);
	env_shade_sun(re, col, view);
}

void environment_no_tex_shade(Render *re, float col[3], float view[3])
{
	float fac;

	fac= 0.5f*(1.0f+view[0]*re->cam.viewzvec[0]+ view[1]*re->cam.viewzvec[1]+ view[2]*re->cam.viewzvec[2]);

	col[0]= (1.0f-fac)*re->db.wrld.horr + fac*re->db.wrld.zenr;
	col[1]= (1.0f-fac)*re->db.wrld.horg + fac*re->db.wrld.zeng;
	col[2]= (1.0f-fac)*re->db.wrld.horb + fac*re->db.wrld.zenb;
}

void environment_shade_pixel(Render *re, float col[4], float fx, float fy, int thread)
{
	float view[3], dxyview[2];

	/*
	  The rules for sky:
	  1. Draw an image, if a background image was provided. Stop
	  2. get texture and color blend, and combine these.
	*/

	float fac;

	/* 1. Do a backbuffer image: */ 
	if((re->db.wrld.skytype & (WO_SKYBLEND+WO_SKYTEX))==0) {
		/* 2. solid color */
		copy_v3_v3(col, &re->db.wrld.horr);
		col[3] = 0.0f;
	} 
	else {
		/* 3. */

		/* This one true because of the context of this routine  */
		if(re->db.wrld.skytype & WO_SKYPAPER) {
			view[0]= -1.0f + 2.0f*(fx/(float)re->cam.winx);
			view[1]= -1.0f + 2.0f*(fy/(float)re->cam.winy);
			view[2]= 0.0;
			
			dxyview[0]= 1.0f/(float)re->cam.winx;
			dxyview[1]= 1.0f/(float)re->cam.winy;
		}
		else {
			camera_raster_to_view(&re->cam, view, fx, fy);
			fac= normalize_v3(view);
			
			if(re->db.wrld.skytype & WO_SKYTEX) {
				dxyview[0]= -re->cam.viewdx/fac;
				dxyview[1]= -re->cam.viewdy/fac;
			}
		}
		
		/* get sky color in the color */
		env_shade_sky(re, col, NULL, view, dxyview, thread);
		col[3] = 0.0f;
	}
	
	camera_raster_to_view(&re->cam, view, fx, fy);
	env_shade_sun(re, col, view);
}

/********************************* mist/fog ******************************/

#if 0
static void fogcolor(float *colf, float *rco, float *view)
{
	float alpha, stepsize, startdist, dist, hor[4], zen[3], vec[3], dview[3];
	float div=0.0f, distfac;
	
	hor[0]= re->db.wrld.horr; hor[1]= re->db.wrld.horg; hor[2]= re->db.wrld.horb;
	zen[0]= re->db.wrld.zenr; zen[1]= re->db.wrld.zeng; zen[2]= re->db.wrld.zenb;
	
	copy_v3_v3(vec, rco);
	
	/* we loop from cur coord to mist start in steps */
	stepsize= 1.0f;
	
	div= ABS(view[2]);
	dview[0]= view[0]/(stepsize*div);
	dview[1]= view[1]/(stepsize*div);
	dview[2]= -stepsize;

	startdist= -rco[2] + BLI_frand();
	for(dist= startdist; dist>re->db.wrld.miststa; dist-= stepsize) {
		
		hor[0]= re->db.wrld.horr; hor[1]= re->db.wrld.horg; hor[2]= re->db.wrld.horb;
		alpha= 1.0f;
		do_sky_tex(vec, vec, NULL, hor, zen, &alpha);
		
		distfac= (dist-re->db.wrld.miststa)/re->db.wrld.mistdist;
		
		hor[3]= hor[0]*distfac*distfac;
		
		/* premul! */
		alpha= hor[3];
		hor[0]= hor[0]*alpha;
		hor[1]= hor[1]*alpha;
		hor[2]= hor[2]*alpha;
		pxf_add_alpha_over(colf, hor);
		
		sub_v3_v3v3(vec, vec, dview);
	}	
}
#endif

/* zcor is distance, co the 3d coordinate in eye space, return alpha */
float environment_mist_factor(Render *re, float zcor, float *co)	
{
	float fac, hi;
	
	fac= zcor - re->db.wrld.miststa;	/* zcor is calculated per pixel */

	/* fac= -co[2]-re->db.wrld.miststa; */

	if(fac>0.0f) {
		if(fac< re->db.wrld.mistdist) {
			
			fac= (fac/(re->db.wrld.mistdist));
			
			if(re->db.wrld.mistype==0) fac*= fac;
			else if(re->db.wrld.mistype==1);
			else fac= sqrtf(fac);
		}
		else fac= 1.0f;
	}
	else fac= 0.0f;
	
	/* height switched off mist */
	if(re->db.wrld.misthi!=0.0f && fac!=0.0f) {
		/* at height misthi the mist is completely gone */

		hi= re->cam.viewinv[0][2]*co[0]+re->cam.viewinv[1][2]*co[1]+re->cam.viewinv[2][2]*co[2]+re->cam.viewinv[3][2];
		
		if(hi>re->db.wrld.misthi) fac= 0.0f;
		else if(hi>0.0f) {
			hi= (re->db.wrld.misthi-hi)/re->db.wrld.misthi;
			fac*= hi*hi;
		}
	}

	return (1.0f-fac)* (1.0f-re->db.wrld.misi);	
}

/********************************* sun/sky ******************************/

void environment_sun_init(LampRen *lar, Lamp *la, float obmat[4][4])
{
	float vec[3];

	if((la->sun_effect_type & LA_SUN_EFFECT_SKY) || (la->sun_effect_type & LA_SUN_EFFECT_AP)){
		lar->sunsky = (SunSky*)MEM_callocN(sizeof(SunSky), "sunskyren");
		lar->sunsky->effect_type = la->sun_effect_type;
	
		copy_v3_v3(vec, obmat[2]);
		normalize_v3(vec);
		
		InitSunSky(lar->sunsky, la->atm_turbidity, vec, la->horizon_brightness, 
				la->spread, la->sun_brightness, la->sun_size, la->backscattered_light,
				   la->skyblendfac, la->skyblendtype, la->sky_exposure, la->sky_colorspace);
		
		InitAtmosphere(lar->sunsky, la->sun_intensity, 1.0f, 1.0f, la->atm_inscattering_factor, la->atm_extinction_factor,
				la->atm_distance_factor);
	}
}

void environment_sun_free(LampRen *lar)
{
	if(lar->sunsky) {
		MEM_freeN(lar->sunsky);
		lar->sunsky= NULL;
	}
}

