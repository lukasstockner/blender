/**
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
 * The Original Code is Copyright (C) 1990-1998 NeoGeo BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_lamp_types.h"

#include "BKE_global.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_jitter.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "PIL_time.h"

#include "RE_raytrace.h"

#include "database.h"
#include "environment.h"
#include "lamp.h"
#include "object.h"
#include "object_mesh.h"
#include "pixelfilter.h"
#include "raycounter.h"
#include "rayobject.h"
#include "render_types.h"
#include "rendercore.h"
#include "result.h"
#include "sampler.h"
#include "shading.h"
#include "texture.h"
#include "volumetric.h"


#define RAY_TRA		1
#define RAY_TRAFLIP	2

#define DEPTH_SHADOW_TRA  10

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static int test_break(void *data)
{
	Render *re = (Render*)data;
	return re->cb.test_break(re->cb.tbh);
}

static void RE_rayobject_config_control(RayObject *r, Render *re)
{
	if(RE_rayobject_isRayAPI(r))
	{
		r = RE_rayobject_align( r );
		r->control.data = re;
		r->control.test_break = test_break;
	}
}

RayObject*  RE_rayobject_create(Render *re, int type, int size)
{
	RayObject * res = NULL;

	if(type == R_RAYSTRUCTURE_AUTO)
	{
		//TODO
		//if(detect_simd())
#ifdef __SSE__
		type = R_RAYSTRUCTURE_SIMD_SVBVH;
#else
		type = R_RAYSTRUCTURE_VBVH;
#endif
	}
	
#ifndef __SSE__
	if(type == R_RAYSTRUCTURE_SIMD_SVBVH || type == R_RAYSTRUCTURE_SIMD_QBVH)
	{
		puts("Warning: Using VBVH (SSE was disabled at compile time)");
		type = R_RAYSTRUCTURE_VBVH;
	}
#endif
	
		
	if(type == R_RAYSTRUCTURE_OCTREE) //TODO dynamic ocres
		res = RE_rayobject_octree_create(re->params.r.ocres, size);
	else if(type == R_RAYSTRUCTURE_BLIBVH)
		res = RE_rayobject_blibvh_create(size);
	else if(type == R_RAYSTRUCTURE_VBVH)
		res = RE_rayobject_vbvh_create(size);
	else if(type == R_RAYSTRUCTURE_SIMD_SVBVH)
		res = RE_rayobject_svbvh_create(size);
	else if(type == R_RAYSTRUCTURE_SIMD_QBVH)
		res = RE_rayobject_qbvh_create(size);
	else
		res = RE_rayobject_vbvh_create(size);	//Fallback
	
	
	if(res)
		RE_rayobject_config_control( res, re );
	
	return res;
}

#ifdef RE_RAYCOUNTER
RayCounter re_rc_counter[BLENDER_MAX_THREADS];
#endif


void raytree_free(RenderDB *rdb)
{
	ObjectInstanceRen *obi;
	
	if(rdb->raytree)
	{
		RE_rayobject_free(rdb->raytree);
		rdb->raytree = NULL;
	}
	if(rdb->rayfaces)
	{
		MEM_freeN(rdb->rayfaces);
		rdb->rayfaces = NULL;
	}
	if(rdb->rayprimitives)
	{
		MEM_freeN(rdb->rayprimitives);
		rdb->rayprimitives = NULL;
	}

	for(obi=rdb->instancetable.first; obi; obi=obi->next)
	{
		ObjectRen *obr = obi->obr;
		if(obr->raytree)
		{
			RE_rayobject_free(obr->raytree);
			obr->raytree = NULL;
		}
		if(obr->rayfaces)
		{
			MEM_freeN(obr->rayfaces);
			obr->rayfaces = NULL;
		}
		if(obi->raytree)
		{
			RE_rayobject_free(obi->raytree);
			obi->raytree = NULL;
		}
	}
	
#ifdef RE_RAYCOUNTER
	{
		RayCounter sum;
		memset( &sum, 0, sizeof(sum) );
		int i;
		for(i=0; i<BLENDER_MAX_THREADS; i++)
			RE_RC_MERGE(&sum, re_rc_counter+i);
		RE_RC_INFO(&sum);
	}
#endif
}

static int is_raytraceable_vlr(Render *re, VlakRen *vlr)
{
	/* note: volumetric must be tracable, wire must not */
	if((re->params.flag & R_BAKE_TRACE) || (vlr->mat->mode & MA_TRACEBLE) || (vlr->mat->material_type == MA_TYPE_VOLUME))
		if(vlr->mat->material_type != MA_TYPE_WIRE)
			return 1;
	return 0;
}

static int is_raytraceable(Render *re, ObjectInstanceRen *obi)
{
	int v;
	ObjectRen *obr = obi->obr;

	if(re->db.excludeob && obr->ob == re->db.excludeob)
		return 0;

	for(v=0;v<obr->totvlak;v++)
	{
		VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
		if(is_raytraceable_vlr(re, vlr))
			return 1;
	}
	return 0;
}


RayObject* raytree_create_object(Render *re, ObjectInstanceRen *obi)
{
	//TODO
	// out-of-memory safeproof
	// break render
	// update render stats
	ObjectRen *obr = obi->obr;
	
	if(obr->raytree == NULL)
	{
		RayObject *raytree;
		RayFace *face = NULL;
		VlakPrimitive *vlakprimitive = NULL;
		int v;
		
		//Count faces
		int faces = 0;
		for(v=0;v<obr->totvlak;v++)
		{
			VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
			if(is_raytraceable_vlr(re, vlr))
				faces++;
		}
		assert( faces > 0 );

		//Create Ray cast accelaration structure		
		raytree = RE_rayobject_create( re,  re->params.r.raytrace_structure, faces );
		if(  (re->params.r.raytrace_options & R_RAYTRACE_USE_LOCAL_COORDS) )
			vlakprimitive = obr->rayprimitives = (VlakPrimitive*)MEM_callocN(faces*sizeof(VlakPrimitive), "ObjectRen primitives");
		else
			face = obr->rayfaces = (RayFace*)MEM_callocN(faces*sizeof(RayFace), "ObjectRen faces");

		obr->rayobi = obi;
		
		for(v=0;v<obr->totvlak;v++)
		{
			VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
			if(is_raytraceable_vlr(re, vlr))
			{
				if(  (re->params.r.raytrace_options & R_RAYTRACE_USE_LOCAL_COORDS) )
				{
					RE_rayobject_add( raytree, RE_vlakprimitive_from_vlak( vlakprimitive, obi, vlr ) );
					vlakprimitive++;
				}
				else
				{
					RE_rayface_from_vlak( face, obi, vlr );				
					RE_rayobject_add( raytree, RE_rayobject_unalignRayFace(face) );
					face++;
				}
			}
		}
		RE_rayobject_done( raytree );

		/* in case of cancel during build, raytree is not usable */
		if(test_break(re))
			RE_rayobject_free(raytree);
		else
			obr->raytree= raytree;
	}

	if(obr->raytree) {
		if((obi->flag & R_TRANSFORMED) && obi->raytree == NULL)
		{
			obi->transform_primitives = 0;
			obi->raytree = RE_rayobject_instance_create( obr->raytree, obi->mat, obi, obi->obr->rayobi );
		}
	}
	
	if(obi->raytree) return obi->raytree;
	return obi->obr->raytree;
}

static int has_special_rayobject(Render *re, ObjectInstanceRen *obi)
{
	if( (obi->flag & R_TRANSFORMED) && (re->params.r.raytrace_options & R_RAYTRACE_USE_INSTANCES) )
	{
		ObjectRen *obr = obi->obr;
		int v, faces = 0;
		
		for(v=0;v<obr->totvlak;v++)
		{
			VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
			if(is_raytraceable_vlr(re, vlr))
			{
				faces++;
				if(faces > 4)
					return 1;
			}
		}
	}
	return 0;
}
/*
 * create a single raytrace structure with all faces
 */
static void raytree_create_single(Render *re)
{
	ObjectInstanceRen *obi;
	RayObject *raytree;
	RayFace *face = NULL;
	VlakPrimitive *vlakprimitive = NULL;
	int faces = 0, obs = 0, special = 0;

	for(obi=re->db.instancetable.first; obi; obi=obi->next)
	if(is_raytraceable(re, obi))
	{
		int v;
		ObjectRen *obr = obi->obr;
		obs++;
		
		if(has_special_rayobject(re, obi))
		{
			special++;
		}
		else
		{
			for(v=0;v<obr->totvlak;v++)
			{
				VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
				if(is_raytraceable_vlr(re, vlr))
					faces++;
			}
		}
	}
	
	if(faces + special == 0)
	{
		re->db.raytree = RE_rayobject_empty_create();
		return;
	}
	
	//Create raytree
	raytree = re->db.raytree = RE_rayobject_create( re, re->params.r.raytrace_structure, faces+special );

	if( (re->params.r.raytrace_options & R_RAYTRACE_USE_LOCAL_COORDS) )
	{
		vlakprimitive = re->db.rayprimitives = (VlakPrimitive*)MEM_callocN(faces*sizeof(VlakPrimitive), "Raytrace vlak-primitives");
	}
	else
	{
		face = re->db.rayfaces	= (RayFace*)MEM_callocN(faces*sizeof(RayFace), "Render ray faces");
	}
	
	for(obi=re->db.instancetable.first; obi; obi=obi->next)
	if(is_raytraceable(re, obi))
	{
		if(test_break(re))
			break;

		if(has_special_rayobject(re, obi))
		{
			RayObject *obj = raytree_create_object(re, obi);
			RE_rayobject_add( re->db.raytree, obj );
		}
		else
		{
			int v;
			ObjectRen *obr = obi->obr;
			
			if(obi->flag & R_TRANSFORMED)
			{
				obi->transform_primitives = 1;
			}

			for(v=0;v<obr->totvlak;v++)
			{
				VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
				if(is_raytraceable_vlr(re, vlr))
				{
					if( (re->params.r.raytrace_options & R_RAYTRACE_USE_LOCAL_COORDS) )
					{
						RayObject *obj = RE_vlakprimitive_from_vlak( vlakprimitive, obi, vlr );
						RE_rayobject_add( raytree, obj );
						vlakprimitive++;
					}
					else
					{
						RE_rayface_from_vlak(face, obi, vlr);
						if((obi->flag & R_TRANSFORMED))
						{
							mul_m4_v3(obi->mat, face->v1);
							mul_m4_v3(obi->mat, face->v2);
							mul_m4_v3(obi->mat, face->v3);
							if(RE_rayface_isQuad(face))
								mul_m4_v3(obi->mat, face->v4);
						}

						RE_rayobject_add( raytree, RE_rayobject_unalignRayFace(face) );
						face++;
					}
				}
			}
		}
	}
	
	if(!test_break(re))
	{	
		re->cb.i.infostr= "Raytree.. building";
		re->cb.stats_draw(re->cb.sdh, &re->cb.i);

		RE_rayobject_done( raytree );	
	}
}

void raytree_create(Render *re)
{
	float min[3], max[3], sub[3];
	int i;
	
	re->cb.i.infostr= "Raytree.. preparing";
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);

	/* disable options not yet suported by octree,
	   they might actually never be supported (unless people really need it) */
	if(re->params.r.raytrace_structure == R_RAYSTRUCTURE_OCTREE)
		re->params.r.raytrace_options &= ~( R_RAYTRACE_USE_INSTANCES | R_RAYTRACE_USE_LOCAL_COORDS);

	if(G.f & G_DEBUG) {
		BENCH(raytree_create_single(re), tree_build);
	}
	else
		raytree_create_single(re);

	if(test_break(re))
	{
		raytree_free(&re->db);

		re->cb.i.infostr= "Raytree building canceled";
		re->cb.stats_draw(re->cb.sdh, &re->cb.i);
	}
	else
	{
		//Calculate raytree max_size
		//This is ONLY needed to kept a bogus behaviour of SUN and HEMI lights
		INIT_MINMAX(min, max);
		RE_rayobject_merge_bb( re->db.raytree, min, max );
		for(i=0; i<3; i++)
		{
			min[i] += 0.01f;
			max[i] += 0.01f;
			sub[i] = max[i]-min[i];
		}
		re->db.maxdist = sqrt( sub[0]*sub[0] + sub[1]*sub[1] + sub[2]*sub[2] );

		re->cb.i.infostr= "Raytree finished";
		re->cb.stats_draw(re->cb.sdh, &re->cb.i);
	}

#ifdef RE_RAYCOUNTER
	memset( re_rc_counter, 0, sizeof(re_rc_counter) );
#endif
}

void shade_ray(Render *re, Isect *is, ShadeInput *shi, ShadeResult *shr)
{
	ObjectInstanceRen *obi= (ObjectInstanceRen*)is->hit.ob;
	VlakRen *vlr= (VlakRen*)is->hit.face;
	int osatex= 0;
	
	/* set up view vector */
	copy_v3_v3(shi->geometry.view, is->vec);

	/* render co */
	shi->geometry.co[0]= is->start[0]+is->labda*(shi->geometry.view[0]);
	shi->geometry.co[1]= is->start[1]+is->labda*(shi->geometry.view[1]);
	shi->geometry.co[2]= is->start[2]+is->labda*(shi->geometry.view[2]);
	
	normalize_v3(shi->geometry.view);

	shi->primitive.obi= obi;
	shi->primitive.obr= obi->obr;
	shi->primitive.vlr= vlr;
	shi->material.mat= vlr->mat;
	shade_input_init_material(re, shi);
	
	// Osa structs we leave unchanged now
	SWAP(int, osatex, shi->geometry.osatex);
	
	shi->geometry.dxco[0]= shi->geometry.dxco[1]= shi->geometry.dxco[2]= 0.0f;
	shi->geometry.dyco[0]= shi->geometry.dyco[1]= shi->geometry.dyco[2]= 0.0f;
	
	// but, set Osa stuff to zero where it can confuse texture code
	if(shi->material.mat->texco & (TEXCO_NORM|TEXCO_REFL) ) {
		shi->geometry.dxno[0]= shi->geometry.dxno[1]= shi->geometry.dxno[2]= 0.0f;
		shi->geometry.dyno[0]= shi->geometry.dyno[1]= shi->geometry.dyno[2]= 0.0f;
	}

	if(vlr->v4) {
		if(is->isect==2) 
			shade_input_set_triangle_i(re, shi, obi, vlr, 2, 1, 3);
		else
			shade_input_set_triangle_i(re, shi, obi, vlr, 0, 1, 3);
	}
	else {
		shade_input_set_triangle_i(re, shi, obi, vlr, 0, 1, 2);
	}

	shi->geometry.u= is->u;
	shi->geometry.v= is->v;
	shi->geometry.dx_u= shi->geometry.dx_v= shi->geometry.dy_u= shi->geometry.dy_v=  0.0f;

	shade_input_set_normals(shi);

	/* point normals to viewing direction */
	if(dot_v3v3(shi->geometry.facenor, shi->geometry.view) < 0.0f)
		shade_input_flip_normals(shi);

	shade_input_set_shade_texco(re, shi);
	if (shi->material.mat->material_type == MA_TYPE_VOLUME) {
		if(ELEM(is->mode, RE_RAY_SHADOW, RE_RAY_SHADOW_TRA)) {
			shade_volume_shadow(re, shi, shr, is);
		} else {
			shade_volume_outside(re, shi, shr);
		}
	}
	else if(is->mode==RE_RAY_SHADOW_TRA) {
		/* temp hack to prevent recursion */
		if(shi->shading.nodes==0 && shi->material.mat->nodetree && shi->material.mat->use_nodes) {
			ntreeShaderExecTree(shi->material.mat->nodetree, re, shi, shr);
			shi->material.mat= vlr->mat;		/* shi->material.mat is being set in nodetree */
		}
		else
			shade_color(re, shi, shr);
	}
	else {
		if(shi->material.mat->nodetree && shi->material.mat->use_nodes) {
			ntreeShaderExecTree(shi->material.mat->nodetree, re, shi, shr);
			shi->material.mat= vlr->mat;		/* shi->material.mat is being set in nodetree */
		}
		else {
			int tempdepth;
			/* XXX dodgy business here, set ray depth to -1
			 * to ignore raytrace in shade_material_loop()
			 * this could really use a refactor --Matt */
			if (shi->shading.volume_depth == 0) {
				tempdepth = shi->shading.depth;
				shi->shading.depth = -1;
				shade_material_loop(re, shi, shr);
				shi->shading.depth = tempdepth;
			} else {
				shade_material_loop(re, shi, shr);
			}
		}
	}	
	
	SWAP(int, osatex, shi->geometry.osatex);  // XXXXX!!!!

}

static int refraction(float *refract, float *n, float *view, float index)
{
	float dot, fac;

	copy_v3_v3(refract, view);
	
	dot= view[0]*n[0] + view[1]*n[1] + view[2]*n[2];

	if(dot>0.0f) {
		index = 1.0f/index;
		fac= 1.0f - (1.0f - dot*dot)*index*index;
		if(fac<= 0.0f) return 0;
		fac= -dot*index + sqrt(fac);
	}
	else {
		fac= 1.0f - (1.0f - dot*dot)*index*index;
		if(fac<= 0.0f) return 0;
		fac= -dot*index - sqrt(fac);
	}

	refract[0]= index*view[0] + fac*n[0];
	refract[1]= index*view[1] + fac*n[1];
	refract[2]= index*view[2] + fac*n[2];

	return 1;
}

/* orn = original face normal */
static void reflection(float *ref, float *n, float *view, float *orn)
{
	float f1;
	
	f1= -2.0f*(n[0]*view[0]+ n[1]*view[1]+ n[2]*view[2]);
	
	ref[0]= (view[0]+f1*n[0]);
	ref[1]= (view[1]+f1*n[1]);
	ref[2]= (view[2]+f1*n[2]);

	if(orn) {
		/* test phong normals, then we should prevent vector going to the back */
		f1= ref[0]*orn[0]+ ref[1]*orn[1]+ ref[2]*orn[2];
		if(f1>0.0f) {
			f1+= .01f;
			ref[0]-= f1*orn[0];
			ref[1]-= f1*orn[1];
			ref[2]-= f1*orn[2];
		}
	}
}

#if 0
static void color_combine(float *result, float fac1, float fac2, float *col1, float *col2)
{
	float col1t[3], col2t[3];
	
	col1t[0]= sqrt(col1[0]);
	col1t[1]= sqrt(col1[1]);
	col1t[2]= sqrt(col1[2]);
	col2t[0]= sqrt(col2[0]);
	col2t[1]= sqrt(col2[1]);
	col2t[2]= sqrt(col2[2]);

	result[0]= (fac1*col1t[0] + fac2*col2t[0]);
	result[0]*= result[0];
	result[1]= (fac1*col1t[1] + fac2*col2t[1]);
	result[1]*= result[1];
	result[2]= (fac1*col1t[2] + fac2*col2t[2]);
	result[2]*= result[2];
}
#endif

static float shade_by_transmission(Isect *is, ShadeInput *shi, ShadeResult *shr)
{
	float dx, dy, dz, d, p;

	if (0 == (shi->material.mat->mode & MA_TRANSP))
		return -1;
	   
	if (shi->material.mat->tx_limit <= 0.0f) {
		d= 1.0f;
	} 
	else {
		/* shi.co[] calculated by shade_ray() */
		dx= shi->geometry.co[0] - is->start[0];
		dy= shi->geometry.co[1] - is->start[1];
		dz= shi->geometry.co[2] - is->start[2];
		d= sqrt(dx*dx+dy*dy+dz*dz);
		if (d > shi->material.mat->tx_limit)
			d= shi->material.mat->tx_limit;

		p = shi->material.mat->tx_falloff;
		if(p < 0.0f) p= 0.0f;
		else if (p > 10.0f) p= 10.0f;

		shr->alpha *= pow(d, p);
		if (shr->alpha > 1.0f)
			shr->alpha= 1.0f;
	}

	return d;
}

static void ray_fadeout_endcolor(Render *re, float *col, ShadeInput *origshi, ShadeInput *shi, ShadeResult *shr, Isect *isec, float *vec)
{
	/* un-intersected rays get either rendered material color or sky color */
	if (origshi->material.mat->fadeto_mir == MA_RAYMIR_FADETOMAT) {
		copy_v3_v3(col, shr->diff); /* assumes diffuse has already been computed! */
	} else if (origshi->material.mat->fadeto_mir == MA_RAYMIR_FADETOSKY) {
		copy_v3_v3(shi->geometry.view, vec);
		normalize_v3(shi->geometry.view);
		environment_shade(re, col, isec->start, shi->geometry.view, NULL, shi->shading.thread);
	}
}

static void ray_fadeout(Isect *is, ShadeInput *shi, float *col, float *blendcol, float dist_mir)
{
	/* if fading out, linear blend against fade color */
	float blendfac;

	blendfac = 1.0 - len_v3v3(shi->geometry.co, is->start)/dist_mir;
	
	col[0] = col[0]*blendfac + (1.0 - blendfac)*blendcol[0];
	col[1] = col[1]*blendfac + (1.0 - blendfac)*blendcol[1];
	col[2] = col[2]*blendfac + (1.0 - blendfac)*blendcol[2];
}

/* the main recursive tracer itself */
static void traceray(Render *re, ShadeInput *origshi, ShadeResult *origshr, short depth, float *start, float *vec, float *col, ObjectInstanceRen *obi, VlakRen *vlr, int traflag)
{
	ShadeInput shi;
	ShadeResult shr;
	Isect isec;
	float f, f1, fr, fg, fb;
	float ref[3];
	float dist_mir = origshi->material.mat->dist_mir;

	/* Warning, This is not that nice, and possibly a bit slow for every ray,
	however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
	memset(&shi, 0, sizeof(ShadeInput)); 
	/* end warning! - Campbell */
	
	copy_v3_v3(isec.start, start);
	copy_v3_v3(isec.vec, vec );
	isec.labda = dist_mir > 0 ? dist_mir : RE_RAYTRACE_MAXDIST;
	isec.mode= RE_RAY_MIRROR;
	isec.skip = RE_SKIP_VLR_NEIGHBOUR | RE_SKIP_VLR_RENDER_CHECK;
	isec.hint = 0;

	isec.orig.ob   = obi;
	isec.orig.face = vlr;
	RE_RC_INIT(isec, shi);

	if(RE_rayobject_raycast(re->db.raytree, &isec)) {
		float d= 1.0f;
		
		shi.shading.mask= origshi->shading.mask;
		shi.geometry.osatex= origshi->geometry.osatex;
		shi.shading.depth= 1;					/* only used to indicate tracing */
		shi.shading.thread= origshi->shading.thread;
		//shi.shading.sample= 0; // memset above, so dont need this
		shi.geometry.xs= origshi->geometry.xs;
		shi.geometry.ys= origshi->geometry.ys;
		shi.shading.lay= origshi->shading.lay;
		shi.shading.passflag= SCE_PASS_COMBINED; /* result of tracing needs no pass info */
		shi.shading.combinedflag= 0xFFFFFF;		 /* ray trace does all options */
		//shi.do_preview= 0; // memset above, so dont need this
		shi.material.light_override= origshi->material.light_override;
		shi.material.mat_override= origshi->material.mat_override;
		
		memset(&shr, 0, sizeof(ShadeResult));
		
		shade_ray(re, &isec, &shi, &shr);
		if (traflag & RAY_TRA)
			d= shade_by_transmission(&isec, &shi, &shr);

		if(depth>0) {

			if((shi.material.mat->mode_l & MA_TRANSP) && shr.alpha < 1.0f) {
				float nf, f, f1, refract[3], tracol[4];
				
				tracol[0]= shi.material.r;
				tracol[1]= shi.material.g;
				tracol[2]= shi.material.b;
				tracol[3]= col[3];	// we pass on and accumulate alpha
				
				if((shi.material.mat->mode & MA_TRANSP) && (shi.material.mat->mode & MA_RAYTRANSP)) {
					/* odd depths: use normal facing viewer, otherwise flip */
					if(traflag & RAY_TRAFLIP) {
						float norm[3];
						norm[0]= - shi.geometry.vn[0];
						norm[1]= - shi.geometry.vn[1];
						norm[2]= - shi.geometry.vn[2];
						if (!refraction(refract, norm, shi.geometry.view, shi.material.ang))
							reflection(refract, norm, shi.geometry.view, shi.geometry.vn);
					}
					else {
						if (!refraction(refract, shi.geometry.vn, shi.geometry.view, shi.material.ang))
							reflection(refract, shi.geometry.vn, shi.geometry.view, shi.geometry.vn);
					}
					traflag |= RAY_TRA;
					traceray(re, origshi, origshr, depth-1, shi.geometry.co, refract, tracol, shi.primitive.obi, shi.primitive.vlr, traflag ^ RAY_TRAFLIP);
				}
				else
					traceray(re, origshi, origshr, depth-1, shi.geometry.co, shi.geometry.view, tracol, shi.primitive.obi, shi.primitive.vlr, 0);
				
				f= shr.alpha; f1= 1.0f-f;
				nf= d * shi.material.mat->filter;
				fr= 1.0f+ nf*(shi.material.r-1.0f);
				fg= 1.0f+ nf*(shi.material.g-1.0f);
				fb= 1.0f+ nf*(shi.material.b-1.0f);
				shr.diff[0]= f*shr.diff[0] + f1*fr*tracol[0];
				shr.diff[1]= f*shr.diff[1] + f1*fg*tracol[1];
				shr.diff[2]= f*shr.diff[2] + f1*fb*tracol[2];
				
				shr.spec[0] *=f;
				shr.spec[1] *=f;
				shr.spec[2] *=f;

				col[3]= f1*tracol[3] + f;
			}
			else 
				col[3]= 1.0f;

			if(shi.material.mat->mode_l & MA_RAYMIRROR) {
				f= shi.material.ray_mirror;
				if(f!=0.0f) f*= fresnel_fac(shi.geometry.view, shi.geometry.vn, shi.material.mat->fresnel_mir_i, shi.material.mat->fresnel_mir);
			}
			else f= 0.0f;
			
			if(f!=0.0f) {
				float mircol[4];
				
				reflection(ref, shi.geometry.vn, shi.geometry.view, NULL);			
				traceray(re, origshi, origshr, depth-1, shi.geometry.co, ref, mircol, shi.primitive.obi, shi.primitive.vlr, 0);
			
				f1= 1.0f-f;

				/* combine */
				//color_combine(col, f*fr*(1.0f-shr.spec[0]), f1, col, shr.diff);
				//col[0]+= shr.spec[0];
				//col[1]+= shr.spec[1];
				//col[2]+= shr.spec[2];
				
				fr= shi.material.mirr;
				fg= shi.material.mirg;
				fb= shi.material.mirb;
		
				col[0]= f*fr*(1.0f-shr.spec[0])*mircol[0] + f1*shr.diff[0] + shr.spec[0];
				col[1]= f*fg*(1.0f-shr.spec[1])*mircol[1] + f1*shr.diff[1] + shr.spec[1];
				col[2]= f*fb*(1.0f-shr.spec[2])*mircol[2] + f1*shr.diff[2] + shr.spec[2];
			}
			else {
				col[0]= shr.diff[0] + shr.spec[0];
				col[1]= shr.diff[1] + shr.spec[1];
				col[2]= shr.diff[2] + shr.spec[2];
			}
			
			if (dist_mir > 0.0) {
				float blendcol[3];
				
				/* max ray distance set, but found an intersection, so fade this color
				 * out towards the sky/material color for a smooth transition */
				ray_fadeout_endcolor(re, blendcol, origshi, &shi, origshr, &isec, vec);
				ray_fadeout(&isec, &shi, col, blendcol, dist_mir);
			}
		}
		else {
			col[0]= shr.diff[0] + shr.spec[0];
			col[1]= shr.diff[1] + shr.spec[1];
			col[2]= shr.diff[2] + shr.spec[2];
		}
		
	}
	else {
		ray_fadeout_endcolor(re, col, origshi, &shi, origshr, &isec, vec);
	}
	RE_RC_MERGE(&origshi->material.raycounter, &shi.shading.raycounter);
}

static int adaptive_sample_variance(int samples, float *col, float *colsq, float thresh)
{
	float var[3], mean[3];

	/* scale threshold just to give a bit more precision in input rather than dealing with 
	 * tiny tiny numbers in the UI */
	thresh /= 2;
	
	mean[0] = col[0] / (float)samples;
	mean[1] = col[1] / (float)samples;
	mean[2] = col[2] / (float)samples;

	var[0] = (colsq[0] / (float)samples) - (mean[0]*mean[0]);
	var[1] = (colsq[1] / (float)samples) - (mean[1]*mean[1]);
	var[2] = (colsq[2] / (float)samples) - (mean[2]*mean[2]);
	
	if ((var[0] * 0.4 < thresh) && (var[1] * 0.3 < thresh) && (var[2] * 0.6 < thresh))
		return 1;
	else
		return 0;
}

static int adaptive_sample_contrast_val(int samples, float prev, float val, float thresh)
{
	/* if the last sample's contribution to the total value was below a small threshold
	 * (i.e. the samples taken are very similar), then taking more samples that are probably 
	 * going to be the same is wasting effort */
	if (fabs( prev/(float)(samples-1) - val/(float)samples ) < thresh) {
		return 1;
	} else
		return 0;
}

static float get_avg_speed(ShadeInput *shi)
{
	float pre_x, pre_y, post_x, post_y, speedavg;
	
	pre_x = (shi->texture.winspeed[0] == PASS_VECTOR_MAX)?0.0:shi->texture.winspeed[0];
	pre_y = (shi->texture.winspeed[1] == PASS_VECTOR_MAX)?0.0:shi->texture.winspeed[1];
	post_x = (shi->texture.winspeed[2] == PASS_VECTOR_MAX)?0.0:shi->texture.winspeed[2];
	post_y = (shi->texture.winspeed[3] == PASS_VECTOR_MAX)?0.0:shi->texture.winspeed[3];
	
	speedavg = (sqrt(pre_x*pre_x + pre_y*pre_y) + sqrt(post_x*post_x + post_y*post_y)) / 2.0;
	
	return speedavg;
}

/* ***************** main calls ************** */


static void trace_refract(Render *re, float *col, ShadeInput *shi, ShadeResult *shr)
{
	QMCSampler *qsa=NULL;
	int samp_type;
	
	float samp3d[3], orthx[3], orthy[3];
	float v_refract[3], v_refract_new[3];
	float sampcol[4], colsq[4];
	
	float blur = pow(1.0 - shi->material.mat->gloss_tra, 3);
	short max_samples = shi->material.mat->samp_gloss_tra;
	float adapt_thresh = shi->material.mat->adapt_thresh_tra;
	
	int samples=0;
	
	colsq[0] = colsq[1] = colsq[2] = 0.0;
	col[0] = col[1] = col[2] = 0.0;
	col[3]= shr->alpha;
	
	if (blur > 0.0) {
		if (adapt_thresh != 0.0) samp_type = SAMP_TYPE_HALTON;
		else samp_type = SAMP_TYPE_HAMMERSLEY;
			
		/* all samples are generated per pixel */
		qsa = sampler_acquire(re, shi->shading.thread, samp_type, max_samples);
	} else 
		max_samples = 1;
	

	while (samples < max_samples) {		
		refraction(v_refract, shi->geometry.vn, shi->geometry.view, shi->material.ang);
		
		if (max_samples > 1) {
			/* get a quasi-random vector from a phong-weighted disc */
			float s[2];

			sampler_get_float_2d(s, qsa, samples);
			sample_project_phong(samp3d, blur, s);
						
			ortho_basis_v3v3_v3( orthx, orthy,v_refract);
			mul_v3_fl(orthx, samp3d[0]);
			mul_v3_fl(orthy, samp3d[1]);
				
			/* and perturb the refraction vector in it */
			add_v3_v3v3(v_refract_new, v_refract, orthx);
			add_v3_v3v3(v_refract_new, v_refract_new, orthy);
			
			normalize_v3(v_refract_new);
		} else {
			/* no blurriness, use the original normal */
			copy_v3_v3(v_refract_new, v_refract);
		}
	
		traceray(re, shi, shr, shi->material.mat->ray_depth_tra, shi->geometry.co, v_refract_new, sampcol, shi->primitive.obi, shi->primitive.vlr, RAY_TRA|RAY_TRAFLIP);
	
		col[0] += sampcol[0];
		col[1] += sampcol[1];
		col[2] += sampcol[2];
		col[3] += sampcol[3];
		
		/* for variance calc */
		colsq[0] += sampcol[0]*sampcol[0];
		colsq[1] += sampcol[1]*sampcol[1];
		colsq[2] += sampcol[2]*sampcol[2];
		
		samples++;
		
		/* adaptive sampling */
		if (adapt_thresh < 1.0 && samples > max_samples/2) 
		{
			if (adaptive_sample_variance(samples, col, colsq, adapt_thresh))
				break;
			
			/* if the pixel so far is very dark, we can get away with less samples */
			if ( (col[0] + col[1] + col[2])/3.0/(float)samples < 0.01 )
				max_samples--;
		}
	}
	
	col[0] /= (float)samples;
	col[1] /= (float)samples;
	col[2] /= (float)samples;
	col[3] /= (float)samples;
	
	if (qsa)
		sampler_release(re, qsa);
}

static void trace_reflect(Render *re, float *col, ShadeInput *shi, ShadeResult *shr, float fresnelfac)
{
	QMCSampler *qsa=NULL;
	int samp_type;
	
	float samp3d[3], orthx[3], orthy[3];
	float v_nor_new[3], v_reflect[3];
	float sampcol[4], colsq[4];
		
	float blur = pow(1.0 - shi->material.mat->gloss_mir, 3);
	short max_samples = shi->material.mat->samp_gloss_mir;
	float adapt_thresh = shi->material.mat->adapt_thresh_mir;
	float aniso = 1.0 - shi->material.mat->aniso_gloss_mir;
	
	int samples=0;
	
	col[0] = col[1] = col[2] = 0.0;
	colsq[0] = colsq[1] = colsq[2] = 0.0;
	
	if (blur > 0.0) {
		if (adapt_thresh != 0.0) samp_type = SAMP_TYPE_HALTON;
		else samp_type = SAMP_TYPE_HAMMERSLEY;
			
		/* all samples are generated per pixel */
		qsa = sampler_acquire(re, shi->shading.thread, samp_type, max_samples);
	} else 
		max_samples = 1;
	
	while (samples < max_samples) {
				
		if (max_samples > 1) {
			/* get a quasi-random vector from a phong-weighted disc */
			float s[2];

			sampler_get_float_2d(s, qsa, samples);
			sample_project_phong(samp3d, blur, s);

			/* find the normal's perpendicular plane, blurring along tangents
			 * if tangent shading enabled */
			if (shi->material.mat->mode & (MA_TANGENT_V)) {
				cross_v3_v3v3(orthx, shi->geometry.vn, shi->geometry.tang);      // bitangent
				copy_v3_v3(orthy, shi->geometry.tang);
				mul_v3_fl(orthx, samp3d[0]);
				mul_v3_fl(orthy, samp3d[1]*aniso);
			} else {
				ortho_basis_v3v3_v3( orthx, orthy,shi->geometry.vn);
				mul_v3_fl(orthx, samp3d[0]);
				mul_v3_fl(orthy, samp3d[1]);
			}

			/* and perturb the normal in it */
			add_v3_v3v3(v_nor_new, shi->geometry.vn, orthx);
			add_v3_v3v3(v_nor_new, v_nor_new, orthy);
			normalize_v3(v_nor_new);
		} else {
			/* no blurriness, use the original normal */
			copy_v3_v3(v_nor_new, shi->geometry.vn);
		}
		
		if((shi->primitive.vlr->flag & R_SMOOTH)) 
			reflection(v_reflect, v_nor_new, shi->geometry.view, shi->geometry.facenor);
		else
			reflection(v_reflect, v_nor_new, shi->geometry.view, NULL);
		
		traceray(re, shi, shr, shi->material.mat->ray_depth, shi->geometry.co, v_reflect, sampcol, shi->primitive.obi, shi->primitive.vlr, 0);

		
		col[0] += sampcol[0];
		col[1] += sampcol[1];
		col[2] += sampcol[2];
	
		/* for variance calc */
		colsq[0] += sampcol[0]*sampcol[0];
		colsq[1] += sampcol[1]*sampcol[1];
		colsq[2] += sampcol[2]*sampcol[2];
		
		samples++;

		/* adaptive sampling */
		if (adapt_thresh > 0.0 && samples > max_samples/3) 
		{
			if (adaptive_sample_variance(samples, col, colsq, adapt_thresh))
				break;
			
			/* if the pixel so far is very dark, we can get away with less samples */
			if ( (col[0] + col[1] + col[2])/3.0/(float)samples < 0.01 )
				max_samples--;
		
			/* reduce samples when reflection is dim due to low ray mirror blend value or fresnel factor
			 * and when reflection is blurry */
			if (fresnelfac < 0.1 * (blur+1)) {
				max_samples--;
				
				/* even more for very dim */
				if (fresnelfac < 0.05 * (blur+1)) 
					max_samples--;
			}
		}
	}
	
	col[0] /= (float)samples;
	col[1] /= (float)samples;
	col[2] /= (float)samples;
	
	if (qsa)
		sampler_release(re, qsa);
}

/* extern call from render loop */
void ray_trace(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	float i, f, f1, fr, fg, fb;
	float mircol[4], tracol[4];
	float diff[3];
	int do_tra, do_mir;
	
	do_tra= ((shi->material.mat->mode & MA_TRANSP) && (shi->material.mat->mode & MA_RAYTRANSP) && shr->alpha!=1.0f);
	do_mir= ((shi->material.mat->mode & MA_RAYMIRROR) && shi->material.ray_mirror!=0.0f);

	copy_v3_v3(diff, shr->diff);
	
	if(do_tra) {
		float olddiff[3];
		
		trace_refract(re, tracol, shi, shr);
		
		f= shr->alpha; f1= 1.0f-f;
		fr= 1.0f+ shi->material.mat->filter*(shi->material.r-1.0f);
		fg= 1.0f+ shi->material.mat->filter*(shi->material.g-1.0f);
		fb= 1.0f+ shi->material.mat->filter*(shi->material.b-1.0f);
		
		/* for refract pass */
		copy_v3_v3(olddiff, diff);
		
		diff[0]= f*diff[0] + f1*fr*tracol[0];
		diff[1]= f*diff[1] + f1*fg*tracol[1];
		diff[2]= f*diff[2] + f1*fb*tracol[2];
		
		if(shi->shading.passflag & SCE_PASS_REFRACT)
			sub_v3_v3v3(shr->refr, diff, olddiff);
		
		if(!(shi->shading.combinedflag & SCE_PASS_REFRACT))
			sub_v3_v3v3(diff, diff, shr->refr);
		
		shr->alpha= tracol[3];
	}
	
	if(do_mir) {
		i= shi->material.ray_mirror*fresnel_fac(shi->geometry.view, shi->geometry.vn, shi->material.mat->fresnel_mir_i, shi->material.mat->fresnel_mir);
		if(i!=0.0f) {
		
			trace_reflect(re, mircol, shi, shr, i);
			
			fr= i*shi->material.mirr;
			fg= i*shi->material.mirg;
			fb= i*shi->material.mirb;

			if(shi->shading.passflag & SCE_PASS_REFLECT) {
				/* mirror pass is not blocked out with spec */
				shr->refl[0]= fr*mircol[0] - fr*diff[0];
				shr->refl[1]= fg*mircol[1] - fg*diff[1];
				shr->refl[2]= fb*mircol[2] - fb*diff[2];
			}
			
			if(shi->shading.combinedflag & SCE_PASS_REFLECT) {
				/* values in shr->spec can be greater then 1.0.
				 * In this case the mircol uses a zero blending factor, so ignoring it is ok.
				 * Fixes bug #18837 - when the spec is higher then 1.0,
				 * diff can become a negative color - Campbell  */
				
				f1= 1.0f-i;
				
				diff[0] *= f1;
				diff[1] *= f1;
				diff[2] *= f1;
				
				if(shr->spec[0]<1.0f)	diff[0] += mircol[0] * (fr*(1.0f-shr->spec[0]));
				if(shr->spec[1]<1.0f)	diff[1] += mircol[1] * (fg*(1.0f-shr->spec[1]));
				if(shr->spec[2]<1.0f)	diff[2] += mircol[2] * (fb*(1.0f-shr->spec[2]));
			}
		}
	}

	copy_v3_v3(shr->diff, diff);
}

/* color 'shadfac' passes through 'col' with alpha and filter */
/* filter is only applied on alpha defined transparent part */
static void addAlphaLight(float *shadfac, float *col, float alpha, float filter)
{
	float fr, fg, fb;
	
	fr= 1.0f+ filter*(col[0]-1.0f);
	fg= 1.0f+ filter*(col[1]-1.0f);
	fb= 1.0f+ filter*(col[2]-1.0f);
	
	shadfac[0]= alpha*col[0] + fr*(1.0f-alpha)*shadfac[0];
	shadfac[1]= alpha*col[1] + fg*(1.0f-alpha)*shadfac[1];
	shadfac[2]= alpha*col[2] + fb*(1.0f-alpha)*shadfac[2];
	
	shadfac[3]= (1.0f-alpha)*shadfac[3];
}

static void ray_trace_shadow_tra(Render *re, Isect *is, ShadeInput *origshi, int depth, int traflag)
{
	/* ray to lamp, find first face that intersects, check alpha properties,
	   if it has col[3]>0.0f  continue. so exit when alpha is full */
	ShadeInput shi;
	ShadeResult shr;
	float initial_labda = is->labda;
	
	if(RE_rayobject_raycast(re->db.raytree, is)) {
		float d= 1.0f;
		/* we got a face */
		
		/* Warning, This is not that nice, and possibly a bit slow for every ray,
		however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
		memset(&shi, 0, sizeof(ShadeInput)); 
		/* end warning! - Campbell */
		
		shi.shading.depth= 1;					/* only used to indicate tracing */
		shi.shading.mask= origshi->shading.mask;
		shi.shading.thread= origshi->shading.thread;
		shi.shading.passflag= SCE_PASS_COMBINED;
		shi.shading.combinedflag= 0xFFFFFF;		 /* ray trace does all options */
	
		shi.geometry.xs= origshi->geometry.xs;
		shi.geometry.ys= origshi->geometry.ys;
		shi.shading.lay= origshi->shading.lay;
		shi.shading.nodes= origshi->shading.nodes;
		
		shade_ray(re, is, &shi, &shr);
		if (shi.material.mat->material_type == MA_TYPE_SURFACE) {
			if (traflag & RAY_TRA)
				d= shade_by_transmission(is, &shi, &shr);
			
			/* mix colors based on shadfac (rgb + amount of light factor) */
			addAlphaLight(is->col, shr.diff, shr.alpha, d*shi.material.mat->filter);
		} else if (shi.material.mat->material_type == MA_TYPE_VOLUME) {
			copy_v4_v4(is->col, shr.combined);
			is->col[3] = 1.f;
		}
		
		if(depth>0 && is->col[3]>0.0f) {
			
			/* adapt isect struct */
			copy_v3_v3(is->start, shi.geometry.co);
			is->labda = initial_labda-is->labda;
			is->orig.ob   = shi.primitive.obi;
			is->orig.face = shi.primitive.vlr;

			ray_trace_shadow_tra(re, is, origshi, depth-1, traflag | RAY_TRA);
		}
		
		RE_RC_MERGE(&origshi->material.raycounter, &shi.raycounter);
	}
}

/* not used, test function for ambient occlusion (yaf: pathlight) */
/* main problem; has to be called within shading loop, giving unwanted recursion */
int ray_trace_shadow_rad(Render *re, ShadeInput *ship, ShadeResult *shr)
{
	static int counter=0, only_one= 0;
	extern float hashvectf[];
	Isect isec;
	ShadeInput shi;
	ShadeResult shr_t;
	float vec[3], accum[3], div= 0.0f;
	int a;
	
	assert(0);
	
	if(only_one) {
		return 0;
	}
	only_one= 1;
	
	accum[0]= accum[1]= accum[2]= 0.0f;
	isec.mode= RE_RAY_MIRROR;
	isec.orig.ob   = ship->primitive.obi;
	isec.orig.face = ship->primitive.vlr;
	isec.hint = 0;

	copy_v3_v3(isec.start, ship->geometry.co);
	
	RE_RC_INIT(isec, shi);
	
	for(a=0; a<8*8; a++) {
		
		counter+=3;
		counter %= 768;
		copy_v3_v3(vec, hashvectf+counter);
		if(dot_v3v3(ship->geometry.vn, vec)>0.0f) {
			vec[0]-= vec[0];
			vec[1]-= vec[1];
			vec[2]-= vec[2];
		}

		copy_v3_v3(isec.vec, vec );
		isec.labda = RE_RAYTRACE_MAXDIST;

		if(RE_rayobject_raycast(re->db.raytree, &isec)) {
			float fac;
			
			/* Warning, This is not that nice, and possibly a bit slow for every ray,
			however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
			memset(&shi, 0, sizeof(ShadeInput)); 
			/* end warning! - Campbell */
			
			shade_ray(re, &isec, &shi, &shr_t);
			fac= isec.labda*isec.labda;
			fac= 1.0f;
			accum[0]+= fac*(shr_t.diff[0]+shr_t.spec[0]);
			accum[1]+= fac*(shr_t.diff[1]+shr_t.spec[1]);
			accum[2]+= fac*(shr_t.diff[2]+shr_t.spec[2]);
			div+= fac;
		}
		else div+= 1.0f;
	}
	
	if(div!=0.0f) {
		shr->diff[0]+= accum[0]/div;
		shr->diff[1]+= accum[1]/div;
		shr->diff[2]+= accum[2]/div;
	}
	shr->alpha= 1.0f;
	
	only_one= 0;
	return 1;
}

static void ray_ao_qmc(Render *re, ShadeInput *shi, float *shadfac)
{
	Isect isec;
	RayHint point_hint;
	QMCSampler *qsa=NULL;
	float samp3d[3];
	float up[3], side[3], dir[3], nrm[3];
	
	float maxdist = re->db.wrld.aodist;
	float fac=0.0f, prev=0.0f;
	float adapt_thresh = re->db.wrld.ao_adapt_thresh;
	float adapt_speed_fac = re->db.wrld.ao_adapt_speed_fac;
	
	int samples=0;
	int max_samples = re->db.wrld.aosamp*re->db.wrld.aosamp;
	
	float dxyview[3], skyadded=0, div;
	int aocolor;
	
	RE_RC_INIT(isec, *shi);
	isec.orig.ob   = shi->primitive.obi;
	isec.orig.face = shi->primitive.vlr;
	isec.skip = RE_SKIP_VLR_NEIGHBOUR | RE_SKIP_VLR_RENDER_CHECK | RE_SKIP_VLR_NON_SOLID_MATERIAL;
	isec.hint = 0;

	isec.hit.ob   = 0;
	isec.hit.face = 0;

	isec.last_hit = NULL;
	
	isec.mode= (re->db.wrld.aomode & WO_AODIST)?RE_RAY_SHADOW_TRA:RE_RAY_SHADOW;
	isec.lay= -1;
	
	copy_v3_v3(isec.start, shi->geometry.co);		
	RE_rayobject_hint_bb( re->db.raytree, &point_hint, isec.start, isec.start );
	isec.hint = &point_hint;

	
	shadfac[0]= shadfac[1]= shadfac[2]= 0.0f;
	
	/* prevent sky colors to be added for only shadow (shadow becomes alpha) */
	aocolor= re->db.wrld.aocolor;
	if(shi->material.mat->mode & MA_ONLYSHADOW)
		aocolor= WO_AOPLAIN;
	
	if(aocolor == WO_AOSKYTEX) {
		dxyview[0]= 1.0f/(float)re->db.wrld.aosamp;
		dxyview[1]= 1.0f/(float)re->db.wrld.aosamp;
		dxyview[2]= 0.0f;
	}
	
	if(shi->primitive.vlr->flag & R_SMOOTH) {
		copy_v3_v3(nrm, shi->geometry.vn);
	}
	else {
		copy_v3_v3(nrm, shi->geometry.facenor);
	}
	
	ortho_basis_v3v3_v3( up, side,nrm);
	
	/* sampling init */
	if (re->db.wrld.ao_samp_method==WO_AOSAMP_HALTON) {
		float speedfac;
		
		speedfac = get_avg_speed(shi) * adapt_speed_fac;
		CLAMP(speedfac, 1.0, 1000.0);
		max_samples /= speedfac;
		if (max_samples < 5) max_samples = 5;
		
		qsa = sampler_acquire(re, shi->shading.thread, SAMP_TYPE_HALTON, max_samples);
	} else if (re->db.wrld.ao_samp_method==WO_AOSAMP_HAMMERSLEY)
		qsa = sampler_acquire(re, shi->shading.thread, SAMP_TYPE_HAMMERSLEY, max_samples);

	while (samples < max_samples) {
		float s[2];

		/* sampling, returns quasi-random vector in unit hemisphere */
		sampler_get_float_2d(s, qsa, samples);
		sample_project_hemi(samp3d, s);

		dir[0] = (samp3d[0]*up[0] + samp3d[1]*side[0] + samp3d[2]*nrm[0]);
		dir[1] = (samp3d[0]*up[1] + samp3d[1]*side[1] + samp3d[2]*nrm[1]);
		dir[2] = (samp3d[0]*up[2] + samp3d[1]*side[2] + samp3d[2]*nrm[2]);
		
		normalize_v3(dir);
			
		isec.vec[0] = -dir[0];
		isec.vec[1] = -dir[1];
		isec.vec[2] = -dir[2];
		isec.labda = maxdist;
		
		prev = fac;
		
		if(RE_rayobject_raycast(re->db.raytree, &isec)) {
			if (re->db.wrld.aomode & WO_AODIST) fac+= exp(-isec.labda*re->db.wrld.aodistfac); 
			else fac+= 1.0f;
		}
		else if(aocolor!=WO_AOPLAIN) {
			float skycol[4], view[3];
			
			view[0]= -dir[0];
			view[1]= -dir[1];
			view[2]= -dir[2];
			
			if(aocolor==WO_AOSKYCOL)
				environment_no_tex_shade(re, skycol, view);
			else /* WO_AOSKYTEX */
				environment_shade(re, skycol, isec.start, view, dxyview, shi->shading.thread);

			shadfac[0]+= skycol[0];
			shadfac[1]+= skycol[1];
			shadfac[2]+= skycol[2];

			skyadded++;
		}
		
		samples++;
		
		if (re->db.wrld.ao_samp_method==WO_AOSAMP_HALTON) {
			/* adaptive sampling - consider samples below threshold as in shadow (or vice versa) and exit early */		
			if (adapt_thresh > 0.0 && (samples > max_samples/2) ) {
				
				if (adaptive_sample_contrast_val(samples, prev, fac, adapt_thresh)) {
					break;
				}
			}
		}
	}
	
	if(aocolor!=WO_AOPLAIN && skyadded) {
		div= (1.0f - fac/(float)samples)/((float)skyadded);
		
		shadfac[0]*= div;	// average color times distances/hits formula
		shadfac[1]*= div;	// average color times distances/hits formula
		shadfac[2]*= div;	// average color times distances/hits formula
	} else {
		shadfac[0]= shadfac[1]= shadfac[2]= 1.0f - fac/(float)samples;
	}
	
	if (qsa)
		sampler_release(re, qsa);
}

void ray_ao(Render *re, ShadeInput *shi, float *shadfac)
{
	ray_ao_qmc(re, shi, shadfac);
}

static void ray_shadow_jittered_coords(Render *re, ShadeInput *shi, int max, float jitco[RE_MAX_OSA][3], int *totjitco)
{
	/* magic numbers for reordering sample positions to give better
	 * results with adaptive sample, when it usually only takes 4 samples */
	int order8[8] = {0, 1, 5, 6, 2, 3, 4, 7};
	int order11[11] = {1, 3, 8, 10, 0, 2, 4, 5, 6, 7, 9};
	int order16[16] = {1, 3, 9, 12, 0, 6, 7, 8, 13, 2, 4, 5, 10, 11, 14, 15};
	int count = pxf_mask_count(&re->sample, shi->shading.mask);

	/* for better antialising shadow samples are distributed over the subpixel
	 * sample coordinates, this only works for raytracing depth 0 though */
	if(!shi->primitive.strand && shi->shading.depth == 0 && count > 1 && count <= max) {
		float xs, ys, zs, view[3];
		int samp, ordsamp, tot= 0;

		for(samp=0; samp<re->params.osa; samp++) {
			if(re->params.osa == 8) ordsamp = order8[samp];
			else if(re->params.osa == 11) ordsamp = order11[samp];
			else if(re->params.osa == 16) ordsamp = order16[samp];
			else ordsamp = samp;

			if(shi->shading.mask & (1<<ordsamp)) {
				float ofs[2];

				/* zbuffer has this inverse corrected, ensures xs,ys are inside pixel */
				pxf_sample_offset(&re->sample, ordsamp, ofs);
				xs= (float)shi->geometry.scanco[0] + ofs[0];
				ys= (float)shi->geometry.scanco[1] + ofs[1];
				zs= shi->geometry.scanco[2];

				shade_input_calc_viewco(re, shi, xs, ys, zs, view, NULL, jitco[tot], NULL, NULL);
				tot++;
			}
		}

		*totjitco= tot;
	}
	else {
		copy_v3_v3(jitco[0], shi->geometry.co);
		*totjitco= 1;
	}
}

static void ray_shadow_qmc(Render *re, ShadeInput *shi, LampRen *lar, float *lampco, float *shadfac, Isect *isec)
{
	QMCSampler *qsa=NULL;
	int samples=0;
	float samp3d[3];

	float fac=0.0f, vec[3], end[3];
	float colsq[4];
	float adapt_thresh = lar->adapt_thresh;
	int min_adapt_samples=4, max_samples = lar->ray_totsamp;
	float *co;
	int do_soft=1, full_osa=0, i;

	float min[3], max[3];
	RayHint bb_hint;

	float jitco[RE_MAX_OSA][3];
	int totjitco;

	colsq[0] = colsq[1] = colsq[2] = 0.0;
	if(isec->mode==RE_RAY_SHADOW_TRA) {
		shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 0.0f;
	} else
		shadfac[3]= 1.0f;
	
	if (lar->ray_totsamp < 2) do_soft = 0;
	if ((re->params.r.mode & R_OSA) && (re->params.osa > 0) && (shi->primitive.vlr->flag & R_FULL_OSA)) full_osa = 1;
	
	if (full_osa) {
		if (do_soft) max_samples  = max_samples/re->params.osa + 1;
		else max_samples = 1;
	} else {
		if (do_soft) max_samples = lar->ray_totsamp;
		else if (shi->shading.depth == 0) max_samples = (re->params.osa > 4)? re->params.osa:5;
		else max_samples = 1;
	}
	
	ray_shadow_jittered_coords(re, shi, max_samples, jitco, &totjitco);

	/* sampling init */
	if (lar->ray_samp_method==LA_SAMP_HALTON)
		qsa = sampler_acquire(re, shi->shading.thread, SAMP_TYPE_HALTON, max_samples);
	else if (lar->ray_samp_method==LA_SAMP_HAMMERSLEY)
		qsa = sampler_acquire(re, shi->shading.thread, SAMP_TYPE_HAMMERSLEY, max_samples);
	
	INIT_MINMAX(min, max);
	for(i=0; i<totjitco; i++)
	{
		DO_MINMAX(jitco[i], min, max);
	}
	RE_rayobject_hint_bb( re->db.raytree, &bb_hint, min, max);
	
	isec->hint = &bb_hint;
	isec->skip = RE_SKIP_VLR_NEIGHBOUR | RE_SKIP_VLR_RENDER_CHECK;
	copy_v3_v3(vec, lampco);
	
	while (samples < max_samples) {

		isec->orig.ob   = shi->primitive.obi;
		isec->orig.face = shi->primitive.vlr;

		/* manually jitter the start shading co-ord per sample
		 * based on the pre-generated OSA texture sampling offsets, 
		 * for anti-aliasing sharp shadow edges. */
		co = jitco[samples % totjitco];

		if (do_soft) {
			float r[2];

			sampler_get_float_2d(r, qsa, samples);

			/* sphere shadow source */
			if (lar->type == LA_LOCAL) {
				float ru[3], rv[3], v[3], s[3];
				
				/* calc tangent plane vectors */
				v[0] = co[0] - lampco[0];
				v[1] = co[1] - lampco[1];
				v[2] = co[2] - lampco[2];
				normalize_v3(v);
				ortho_basis_v3v3_v3( ru, rv,v);
				
				/* sampling, returns quasi-random vector in area_size disc */
				sample_project_disc(samp3d, lar->area_size, r);

				/* distribute disc samples across the tangent plane */
				s[0] = samp3d[0]*ru[0] + samp3d[1]*rv[0];
				s[1] = samp3d[0]*ru[1] + samp3d[1]*rv[1];
				s[2] = samp3d[0]*ru[2] + samp3d[1]*rv[2];
				
				copy_v3_v3(samp3d, s);
			}
			else {
				/* sampling, returns quasi-random vector in [sizex,sizey]^2 plane */
				sample_project_rect(samp3d, lar->area_size, lar->area_sizey, r);
								
				/* align samples to lamp vector */
				mul_m3_v3(lar->mat, samp3d);
			}
			end[0] = vec[0]+samp3d[0];
			end[1] = vec[1]+samp3d[1];
			end[2] = vec[2]+samp3d[2];
		} else {
			copy_v3_v3(end, vec);
		}

		if(shi->primitive.strand) {
			/* bias away somewhat to avoid self intersection */
			float jitbias= 0.5f*(len_v3(shi->geometry.dxco) + len_v3(shi->geometry.dyco));
			float v[3];

			sub_v3_v3v3(v, co, end);
			normalize_v3(v);

			co[0] -= jitbias*v[0];
			co[1] -= jitbias*v[1];
			co[2] -= jitbias*v[2];
		}

		copy_v3_v3(isec->start, co);
		isec->vec[0] = end[0]-isec->start[0];
		isec->vec[1] = end[1]-isec->start[1];
		isec->vec[2] = end[2]-isec->start[2];
		isec->labda = 1.0f; // * normalize_v3(isec->vec);
		
		/* trace the ray */
		if(isec->mode==RE_RAY_SHADOW_TRA) {
			isec->col[0]= isec->col[1]= isec->col[2]=  1.0f;
			isec->col[3]= 1.0f;
			
			ray_trace_shadow_tra(re, isec, shi, DEPTH_SHADOW_TRA, 0);
			shadfac[0] += isec->col[0];
			shadfac[1] += isec->col[1];
			shadfac[2] += isec->col[2];
			shadfac[3] += isec->col[3];
			
			/* for variance calc */
			colsq[0] += isec->col[0]*isec->col[0];
			colsq[1] += isec->col[1]*isec->col[1];
			colsq[2] += isec->col[2]*isec->col[2];
		}
		else {
			if( RE_rayobject_raycast(re->db.raytree, isec) ) fac+= 1.0f;
		}
		
		samples++;
		
		if ((lar->ray_samp_method == LA_SAMP_HALTON)) {
		
			/* adaptive sampling - consider samples below threshold as in shadow (or vice versa) and exit early */
			if ((max_samples > min_adapt_samples) && (adapt_thresh > 0.0) && (samples > max_samples / 3)) {
				if (isec->mode==RE_RAY_SHADOW_TRA) {
					if ((shadfac[3] / samples > (1.0-adapt_thresh)) || (shadfac[3] / samples < adapt_thresh))
						break;
					else if (adaptive_sample_variance(samples, shadfac, colsq, adapt_thresh))
						break;
				} else {
					if ((fac / samples > (1.0-adapt_thresh)) || (fac / samples < adapt_thresh))
						break;
				}
			}
		}
	}
	
	if(isec->mode==RE_RAY_SHADOW_TRA) {
		shadfac[0] /= samples;
		shadfac[1] /= samples;
		shadfac[2] /= samples;
		shadfac[3] /= samples;
	} else
		shadfac[3]= 1.0f-fac/samples;

	if (qsa)
		sampler_release(re, qsa);
}

/* extern call from shade_lamp_loop */
void ray_shadow(Render *re, ShadeInput *shi, LampRen *lar, float *shadfac)
{
	Isect isec;
	float lampco[3];

	/* setup isec */
	RE_RC_INIT(isec, *shi);
	if(shi->material.mat->mode & MA_SHADOW_TRA) isec.mode= RE_RAY_SHADOW_TRA;
	else isec.mode= RE_RAY_SHADOW;
	isec.hint = 0;
	
	if(lar->mode & (LA_LAYER|LA_LAYER_SHADOW))
		isec.lay= lar->lay;
	else
		isec.lay= -1;

	/* only when not mir tracing, first hit optimm */
	if(shi->shading.depth==0) {
		isec.last_hit = lar->last_hit[shi->shading.thread];
	}
	else {
		isec.last_hit = NULL;
	}
	
	if(lar->type==LA_SUN || lar->type==LA_HEMI) {
		/* jitter and QMC sampling add a displace vector to the lamp position
		 * that's incorrect because a SUN lamp does not has an exact position
		 * and the displace should be done at the ray vector instead of the
		 * lamp position.
		 * This is easily verified by noticing that shadows of SUN lights change
		 * with the scene BB.
		 * 
		 * This was detected during SoC 2009 - Raytrace Optimization, but to keep
		 * consistency with older render code it wasn't removed.
		 * 
		 * If the render code goes through some recode/serious bug-fix then this
		 * is something to consider!
		 */
		lampco[0]= shi->geometry.co[0] - re->db.maxdist*lar->vec[0];
		lampco[1]= shi->geometry.co[1] - re->db.maxdist*lar->vec[1];
		lampco[2]= shi->geometry.co[2] - re->db.maxdist*lar->vec[2];
	}
	else {
		copy_v3_v3(lampco, lar->co);
	}
	
	ray_shadow_qmc(re, shi, lar, lampco, shadfac, &isec);
	
	/* for first hit optim, set last interesected shadow face */
	if(shi->shading.depth==0) {
		lar->last_hit[shi->shading.thread] = isec.last_hit;
	}

}

#if 0
/* only when face points away from lamp, in direction of lamp, trace ray and find first exit point */
static void ray_translucent(ShadeInput *shi, LampRen *lar, float *distfac, float *co)
{
	Isect isec;
	float lampco[3];
	
	assert(0);
	
	/* setup isec */
	RE_RC_INIT(isec, *shi);
	isec.mode= RE_RAY_SHADOW_TRA;
	isec.hint = 0;
	
	if(lar->mode & LA_LAYER) isec.lay= lar->lay; else isec.lay= -1;
	
	if(lar->type==LA_SUN || lar->type==LA_HEMI) {
		lampco[0]= shi->geometry.co[0] - RE_RAYTRACE_MAXDIST*lar->vec[0];
		lampco[1]= shi->geometry.co[1] - RE_RAYTRACE_MAXDIST*lar->vec[1];
		lampco[2]= shi->geometry.co[2] - RE_RAYTRACE_MAXDIST*lar->vec[2];
	}
	else {
		copy_v3_v3(lampco, lar->co);
	}
	
	isec.orig.ob   = shi->primitive.obi;
	isec.orig.face = shi->primitive.vlr;
	
	/* set up isec vec */
	copy_v3_v3(isec.start, shi->geometry.co);
	copy_v3_v3(isec.end, lampco);
	
	if(RE_rayobject_raycast(re->db.raytree, &isec)) {
		/* we got a face */
		
		/* render co */
		co[0]= isec.start[0]+isec.labda*(isec.vec[0]);
		co[1]= isec.start[1]+isec.labda*(isec.vec[1]);
		co[2]= isec.start[2]+isec.labda*(isec.vec[2]);
		
		*distfac= len_v3(isec.vec);
	}
	else
		*distfac= 0.0f;
}

#endif

