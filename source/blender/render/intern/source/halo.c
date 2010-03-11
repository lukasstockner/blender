/**
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <float.h>
#include <math.h>
#include <string.h>

#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"

#include "BLI_math.h"

#include "BKE_utildefines.h"

/* own module */
#include "database.h"
#include "environment.h"
#include "lamp.h"
#include "object_halo.h"
#include "render_types.h"
#include "rendercore.h"
#include "shadowbuf.h"
#include "texture_stack.h"

extern float hashvectf[];

static void render_lighting_halo(Render *re, HaloRen *har, float *colf)
{
	GroupObject *go;
	LampRen *lar;
	ShadeInput shi;
	float inp, inf[3], dco[3];
	
	/* Warning, This is not that nice, and possibly a bit slow,
	however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
	memset(&shi, 0, sizeof(ShadeInput)); 
	/* end warning! - Campbell */
		
	dco[0]=dco[1]=dco[2]= 1.0/har->rad;
	
	copy_v3_v3(shi.geometry.co, har->co);
	copy_v3_v3(shi.geometry.dxco, dco);
	copy_v3_v3(shi.geometry.dyco, dco);
	copy_v3_v3(shi.geometry.vn, har->no);
	shi.geometry.osatex= 0;
	shi.shading.lay= -1;
	shi.material.mat= har->mat;

	zero_v3(inf);

	for(go=re->db.lights.first; go; go= go->next) {
		float lv[3], lainf[3], lashdw[3];

		lar= go->lampren;
		
		/* test for lamplayer */
		if(lar->mode & LA_LAYER) if((lar->lay & har->lay)==0) continue;
		if(lamp_skip(re, lar, &shi)) continue;

		/* XXX test */
		if(lamp_sample(lv, lainf, lashdw, re, lar, &shi, har->co, NULL)) {
			inp= maxf(1.0f - fabs(dot_v3v3(har->no, lv)), 0.0);

			inf[0] += lainf[0]*lashdw[0]*inp;
			inf[1] += lainf[1]*lashdw[1]*inp;
			inf[2] += lainf[2]*lashdw[2]*inp;
		}
	}
	
	colf[0] *= maxf(inf[0], 0.0f);
	colf[1] *= maxf(inf[1], 0.0f);
	colf[2] *= maxf(inf[2], 0.0f);
}


/**
 * Converts a halo z-buffer value to distance from the camera's near plane
 * @param z The z-buffer value to convert
 * @return a distance from the camera's near plane in blender units
 */
static float haloZtoDist(Render *re, int z)
{
	float zco = 0;

	if(z >= 0x7FFFFF)
		return 10e10;
	else {
		zco = (float)z/(float)0x7FFFFF;
		if(re->cam.type == CAM_ORTHO)
			return (re->cam.winmat[3][2] - zco*re->cam.winmat[3][3])/(re->cam.winmat[2][2]);
		else
			return (re->cam.winmat[3][2])/(re->cam.winmat[2][2] - re->cam.winmat[2][3]*zco);
	}
}

/**
 * @param col (float[4]) Store the rgb color here (with alpha)
 * The alpha is used to blend the color to the background 
 * color_new = (1-alpha)*color_background + color
 * @param zz The current zbuffer value at the place of this pixel
 * @param dist Distance of the pixel from the center of the halo squared. Given in pixels
 * @param xn The x coordinate of the pixel relaticve to the center of the halo. given in pixels
 * @param yn The y coordinate of the pixel relaticve to the center of the halo. given in pixels
 */
int shadeHaloFloat(Render *re, HaloRen *har,  float *col, int zz, 
					float dist, float xn,  float yn, short flarec)
{
	/* fill in col */
	float t, zn, radist, ringf=0.0f, linef=0.0f, alpha, si, co;
	int a;
   
	if(re->db.wrld.mode & WO_MIST) {
       if(har->type & HA_ONLYSKY) {
           /* stars but no mist */
           alpha= har->alfa;
       }
       else {
           /* a bit patchy... */
           alpha= environment_mist_factor(re, -har->co[2], har->co)*har->alfa;
       }
	}
	else alpha= har->alfa;
	
	if(alpha==0.0)
		return 0;

	/* soften the halo if it intersects geometry */
	if(har->mat && har->mat->mode & MA_HALO_SOFT) {
		float segment_length, halo_depth, distance_from_z, visible_depth, soften;
		
		/* calculate halo depth */
		segment_length= har->hasize*sasqrt(1.0f - dist/(har->rad*har->rad));
		halo_depth= 2.0f*segment_length;

		if(halo_depth < FLT_EPSILON)
			return 0;

		/* calculate how much of this depth is visible */
		distance_from_z = haloZtoDist(re, zz) - haloZtoDist(re, har->zs);
		visible_depth = halo_depth;
		if(distance_from_z < segment_length) {
			soften= (segment_length + distance_from_z)/halo_depth;

			/* apply softening to alpha */
			if(soften < 1.0f)
				alpha *= soften;
			if(alpha <= 0.0f)
				return 0;
		}
	}
	else {
		/* not a soft halo. use the old softening code */
		/* halo being intersected? */
		if(har->zs> zz-har->zd) {
			t= ((float)(zz-har->zs))/(float)har->zd;
			alpha*= sqrt(sqrt(t));
		}
	}

	radist= sqrt(dist);

	/* watch it: not used nicely: flarec is set at zero in pixstruct */
	if(flarec) har->pixels+= (int)(har->rad-radist);

	if(har->ringc) {
		float *rc, fac;
		int ofs;
		
		/* per ring an antialised circle */
		ofs= har->seed;
		
		for(a= har->ringc; a>0; a--, ofs+=2) {
			
			rc= hashvectf + (ofs % 768);
			
			fac= fabs( rc[1]*(har->rad*fabs(rc[0]) - radist) );
			
			if(fac< 1.0) {
				ringf+= (1.0-fac);
			}
		}
	}

	if(har->type & HA_VECT) {
		dist= fabs( har->cos*(yn) - har->sin*(xn) )/har->rad;
		if(dist>1.0) dist= 1.0;
		if(har->tex) {
			zn= har->sin*xn - har->cos*yn;
			yn= har->cos*xn + har->sin*yn;
			xn= zn;
		}
	}
	else dist= dist/har->radsq;

	if(har->type & HA_FLARECIRC) {
		
		dist= 0.5+fabs(dist-0.5);
		
	}

	if(har->hard>=30) {
		dist= sqrt(dist);
		if(har->hard>=40) {
			dist= sin(dist*M_PI_2);
			if(har->hard>=50) {
				dist= sqrt(dist);
			}
		}
	}
	else if(har->hard<20) dist*=dist;

	if(dist < 1.0f)
		dist= (1.0f-dist);
	else
		dist= 0.0f;
	
	if(har->linec) {
		float *rc, fac;
		int ofs;
		
		/* per starpoint an antialiased line */
		ofs= har->seed;
		
		for(a= har->linec; a>0; a--, ofs+=3) {
			
			rc= hashvectf + (ofs % 768);
			
			fac= fabs( (xn)*rc[0]+(yn)*rc[1]);
			
			if(fac< 1.0f )
				linef+= (1.0f-fac);
		}
		
		linef*= dist;
	}

	if(har->starpoints) {
		float ster, angle;
		/* rotation */
		angle= atan2(yn, xn);
		angle*= (1.0+0.25*har->starpoints);
		
		co= cos(angle);
		si= sin(angle);
		
		angle= (co*xn+si*yn)*(co*yn-si*xn);
		
		ster= fabs(angle);
		if(ster>1.0) {
			ster= (har->rad)/(ster);
			
			if(ster<1.0) dist*= sqrt(ster);
		}
	}

	/* disputable optimize... (ton) */
	if(dist<=0.00001)
		return 0;
	
	dist*= alpha;
	ringf*= dist;
	linef*= alpha;
	
	/* The color is either the rgb spec-ed by the user, or extracted from   */
	/* the texture                                                           */
	if(har->tex) {
		col[0]= har->r; 
		col[1]= har->g; 
		col[2]= har->b;
		col[3]= dist;
		
		do_halo_tex(re, har, xn, yn, col);
		
		col[0]*= col[3];
		col[1]*= col[3];
		col[2]*= col[3];
		
	}
	else {
		col[0]= dist*har->r;
		col[1]= dist*har->g;
		col[2]= dist*har->b;
		if(har->type & HA_XALPHA) col[3]= dist*dist;
		else col[3]= dist;
	}

	if(har->mat) {
		if(har->mat->mode & MA_HALO_SHADE) {
			/* we test for lights because of preview... */
			if(re->db.lights.first) render_lighting_halo(re, har, col);
		}

		/* Next, we do the line and ring factor modifications. */
		if(linef!=0.0) {
			Material *ma= har->mat;
			
			col[0]+= linef * ma->specr;
			col[1]+= linef * ma->specg;
			col[2]+= linef * ma->specb;
			
			if(har->type & HA_XALPHA) col[3]+= linef*linef;
			else col[3]+= linef;
		}
		if(ringf!=0.0) {
			Material *ma= har->mat;

			col[0]+= ringf * ma->mirr;
			col[1]+= ringf * ma->mirg;
			col[2]+= ringf * ma->mirb;
			
			if(har->type & HA_XALPHA) col[3]+= ringf*ringf;
			else col[3]+= ringf;
		}
	}
	
	/* alpha requires clip, gives black dots */
	if(col[3] > 1.0f)
		col[3]= 1.0f;

	return 1;
}

#if 0
static void halo_to_bvh(Render *re)
{
	BVHTree *tree;

	/* Create a bvh-tree of the given target */
	tree = BLI_bvhtree_new(re->tothalo, epsilon, tree_type, axis);

	for(i = 0; i < numFaces; i++)
	{
		float co[4][3];
		copy_v3_v3(co[0], vert[ face[i].v1 ].co);
		copy_v3_v3(co[1], vert[ face[i].v2 ].co);
		copy_v3_v3(co[2], vert[ face[i].v3 ].co);
		if(face[i].v4)
			copy_v3_v3(co[3], vert[ face[i].v4 ].co);

		BLI_bvhtree_insert(tree, i, co[0], face[i].v4 ? 4 : 3);
	}

	BLI_bvhtree_balance(tree);
}
#endif

