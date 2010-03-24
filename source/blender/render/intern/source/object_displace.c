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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "RE_shader_ext.h"

#include "material.h"
#include "object.h"
#include "object_mesh.h"
#include "render_types.h"

int render_object_has_displacement(Render *re, ObjectRen *obr)
{
	/* return 1 when this object uses displacement textures. */
	Object *ob= obr->ob;
	Material *ma;
	int i;
	
	for (i=1; i<=ob->totcol; i++) {
		ma=give_render_material(re, ob, i);
		/* ma->mapto is ORed total of all mapto channels */
		if(ma && (ma->mapto & MAP_DISPLACE)) return 1;
	}

	return 0;
}

static void displace_derivatives(ShadeInput *shi)
{
	ShadeGeometry *geom= &shi->geometry;
	ShadePrimitive*prim= &shi->primitive;
	float dcodu[3], dcodv[3];

	/* compute dudx/dvdx */
	sub_v3_v3v3(dcodu, prim->v3->co, prim->v2->co);
	sub_v3_v3v3(dcodv, prim->v3->co, prim->v1->co);
	
	mul_v3_fl(dcodu, 1.0f/dot_v3v3(dcodu, dcodu));
	mul_v3_fl(dcodv, 1.0f/dot_v3v3(dcodv, dcodv));
	
	geom->dx_u= dot_v3v3(geom->dxco, dcodu);
	geom->dx_v= dot_v3v3(geom->dxco, dcodv);
	
	geom->dy_u= dot_v3v3(geom->dyco, dcodu);
	geom->dy_v= dot_v3v3(geom->dyco, dcodv);
}

static void displace_render_vert(Render *re, ObjectRen *obr, ShadeInput *shi, VertRen *vr, int vindex, float *scale, float mat[][4], float nmat[][3], float *sample)
{
	MTFace *tface;
	VlakRen *vlr= shi->primitive.vlr;
	short texco= shi->material.mat->texco;
	float displace[3], *orco;
	char *name;
	int i;

	/* shi->geometry.co is current render coord, just make sure at least some vector is here */
	copy_v3_v3(shi->geometry.co, vr->co);
	/* vertex normal is used for textures type 'col' and 'var' */
	copy_v3_v3(shi->geometry.vn, (vlr->flag & R_SMOOTH)? vr->n: vlr->n);

	if(mat)
		mul_m4_v3(mat, shi->geometry.co);
	if(nmat)
		mul_m3_v3(nmat, shi->geometry.vn);

	if (texco & TEXCO_UV) {
		shi->texture.totuv= 0;
		shi->texture.actuv= obr->actmtface;

		for (i=0; (tface=render_vlak_get_tface(obr, vlr, i, &name, 0)); i++) {
			ShadeInputUV *suv= &shi->texture.uv[i];

			/* shi.uv needs scale correction from tface uv */
			suv->uv[0]= 2*tface->uv[vindex][0]-1.0f;
			suv->uv[1]= 2*tface->uv[vindex][1]-1.0f;
			suv->uv[2]= 0.0f;
			suv->name= name;
			shi->texture.totuv++;
		}
	}

	/* set all rendercoords, 'texco' is an ORed value for all textures needed */
	if (texco & TEXCO_ORCO) {
		orco= render_vert_get_orco(obr, vr, 0);
		if(orco)
			copy_v3_v3(shi->texture.lo, orco);
	}
	if (texco & TEXCO_STICKY) {
		float *sticky= render_vert_get_sticky(obr, vr, 0);
		if(sticky) {
			shi->texture.sticky[0]= sticky[0];
			shi->texture.sticky[1]= sticky[1];
			shi->texture.sticky[2]= 0.0f;
		}
	}
	if (texco & TEXCO_GLOB) {
		copy_v3_v3(shi->texture.gl, shi->geometry.co);
		mul_m4_v3(re->cam.viewinv, shi->texture.gl);
	}
	if (texco & TEXCO_NORM) {
		copy_v3_v3(shi->texture.orn, shi->geometry.vn);
	}
	if(texco & TEXCO_REFL) {
		/* not (yet?) */
	}
	
	mat_displacement(re, shi, displace);
	
	//printf("no=%f, %f, %f\nbefore co=%f, %f, %f\n", vr->n[0], vr->n[1], vr->n[2], 
	//vr->co[0], vr->co[1], vr->co[2]);

	mul_v3_v3(displace, scale);
	
	if(mat)
		mul_m3_v3(nmat, displace);

	/* 0.5 could become button once?  */
	vr->co[0] += displace[0]; 
	vr->co[1] += displace[1];
	vr->co[2] += displace[2];
	
	//printf("after co=%f, %f, %f\n", vr->co[0], vr->co[1], vr->co[2]); 
	
	/* we just don't do this vertex again, bad luck for other face using same vertex with
		different material... */
	vr->flag |= 1;
	
	/* Pass sample back so displace_face can decide which way to split the quad */
	/* Should be sqrt(sample), but I'm only looking for "bigger".  Save the cycles. */
	sample[vr->index]= dot_v3v3(shi->texture.displace, shi->texture.displace);
}

static void displace_render_face(Render *re, ObjectRen *obr, VlakRen *vlr, float *scale, float mat[][4], float nmat[][3], float *sample)
{
	ShadeInput shi;
	VertRen *v1= vlr->v1;
	VertRen *v2= vlr->v2;
	VertRen *v3= vlr->v3;
	VertRen *v4= vlr->v4;

	/* Warning, This is not that nice, and possibly a bit slow,
	however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
	memset(&shi, 0, sizeof(ShadeInput)); 
	/* end warning! - Campbell */
	
	/* set up shadeinput struct for multitex() */
	
	/* memset above means we dont need this */
	/*shi.osatex= 0;*/		/* signal not to use dx[] and dy[] texture AA vectors */

	shi.primitive.obr= obr;
	shi.primitive.vlr= vlr;		/* current render face */
	shi.material.mat= vlr->mat;		/* current input material */
	shi.shading.thread= 0;

	/* TODO, assign these, displacement with new bumpmap is skipped without - campbell */
#if 0
	/* order is not known ? */
	shi.primitive.v1= v1;
	shi.primitive.v2= v2;
	shi.primitive.v3= v3;
#endif
	
	/* Displace the verts, flag is set when done */
	if(!v1->flag)
		displace_render_vert(re, obr, &shi, v1,0,  scale, mat, nmat, sample);
	
	if(!v2->flag)
		displace_render_vert(re, obr, &shi, v2, 1, scale, mat, nmat, sample);

	if(!v3->flag)
		displace_render_vert(re, obr, &shi, v3, 2, scale, mat, nmat, sample);

	if(v4) {
		if(!v4->flag)
			displace_render_vert(re, obr, &shi, v4, 3, scale, mat, nmat, sample);

		/*	closest in displace value.  This will help smooth edges.   */ 
		if(fabs(sample[v1->index] - sample[v3->index]) > fabs(sample[v2->index] - sample[v4->index]))
			vlr->flag |= R_DIVIDE_24;
		else vlr->flag &= ~R_DIVIDE_24;
	}
	
	/* Recalculate the face normal  - if flipped before, flip now */
	if(v4)
		normal_quad_v3(vlr->n, v4->co, v3->co, v2->co, v1->co);
	else
		normal_tri_v3(vlr->n, v3->co, v2->co, v1->co);
}

void render_object_displace(Render *re, ObjectRen *obr, float mat[][4], float nmat[][3])
{
	Object *obt;
	VlakRen *vlr;
	VertRen *vr;
	float scale[3]={1.0f, 1.0f, 1.0f}, temp[3], *sample, (*diffnor)[3]= NULL;
	int i;

	sample= MEM_callocN(sizeof(float)*obr->totvert, "render_object_displace sample");

	/* calculate difference between base smooth and new smooth normals */
	render_object_calc_vnormals(re, obr, 0, 0, &diffnor);

	/* Object Size with parenting */
	obt=obr->ob;
	while(obt){
		add_v3_v3v3(temp, obt->size, obt->dsize);
		scale[0]*=temp[0]; scale[1]*=temp[1]; scale[2]*=temp[2];
		obt=obt->parent;
	}
	
	/* Clear all flags */
	for(i=0; i<obr->totvert; i++){ 
		vr= render_object_vert_get(obr, i);
		vr->flag= 0;
	}

	for(i=0; i<obr->totvlak; i++){
		vlr=render_object_vlak_get(obr, i);
		displace_render_face(re, obr, vlr, scale, mat, nmat, sample);
	}

	MEM_freeN(sample);
	
	/* recalculate displaced smooth normals, and apply difference */
	render_object_calc_vnormals(re, obr, 0, 0, &diffnor);
}

