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
#include "shading.h"

int render_object_need_displacement(Render *re, ObjectRen *obr)
{
	/* return 1 when this object uses displacement textures. */
	Object *ob= obr->ob;
	Material *ma;
	int i;
	
	for(i=0; i<ob->totcol; i++) {
		ma=give_render_material(re, ob, i+1);

		/* ma->mapto is ORed total of all mapto channels */
		if(ma && (ma->mapto & MAP_DISPLACE))
			return 1;
	}

	return 0;
}

static void displace_shadeinput_init_face(ShadeInput *shi, ObjectInstanceRen *obi, VlakRen *vlr, int thread)
{
	memset(shi, 0, sizeof(ShadeInput)); 
	
	shi->primitive.obr= obi->obr;
	shi->primitive.obi= obi;
	shi->primitive.vlr= vlr;
	shi->material.mat= vlr->mat;
	shi->shading.thread= thread;
}

static void displace_shadeinput_init_vertex(Render *re, ShadeInput *shi, VertRen *vr)
{
	ShadeGeometry *geom= &shi->geometry;
	ShadePrimitive *prim= &shi->primitive;
	VlakRen *vlr= prim->vlr;

	/* setup primitive vertices so that current vertex is first */
	if(vlr->v1 == vr) {
		prim->v1= vlr->v1;
		prim->v2= vlr->v2;
		prim->v3= vlr->v3;
	}
	else if(vlr->v2 == vr) {
		prim->v1= vlr->v2;
		prim->v2= vlr->v3;
		prim->v3= vlr->v1;
	}
	else if(vlr->v3 == vr) {
		prim->v1= vlr->v3;
		prim->v2= vlr->v1;
		prim->v3= vlr->v2;
	}
	else if(vlr->v4 == vr) {
		prim->v1= vlr->v4;
		prim->v2= vlr->v1;
		prim->v3= vlr->v3;
	}

	/* setup uvw interpolation weights and derivatives */
	geom->uvw[0]= 1.0f;
	geom->uvw[1]= 0.0f;
	geom->uvw[2]= 0.0f;

	geom->duvw_dx[0]= -0.5f;
	geom->duvw_dx[1]= 0.5f;
	geom->duvw_dx[2]= 0.0f;

	geom->duvw_dx[0]= -0.5f;
	geom->duvw_dx[1]= 0.0f;
	geom->duvw_dx[2]= 0.5f;

	/* setup coordinate and derivatives */
	copy_v3_v3(geom->co, vr->co);
	interp_v3_v3v3v3(geom->dxco, prim->v1->co, prim->v2->co, prim->v3->co, geom->duvw_dx);
	interp_v3_v3v3v3(geom->dyco, prim->v1->co, prim->v2->co, prim->v3->co, geom->duvw_dy);

	/* setup normal */
	copy_v3_v3(shi->geometry.vn, (vlr->flag & R_SMOOTH)? vr->n: vlr->n);

	/* we've got derivatives, so antialias textures */
	shi->geometry.osatex= 1;

	/* setup material and texture coordinates */
	shade_input_init_material(re, shi);
	shade_input_set_shade_texco(re, shi);
}

static void displace_render_vert(Render *re, ObjectRen *obr, ShadeInput *shi, VertRen *vr, int vindex, float scale[3])
{
	float displacement[3], bound= obr->ob->displacebound;

	/* setup shadeinput for vertex */
	displace_shadeinput_init_vertex(re, shi, vr);

	/* compute displacement from materials */
	mat_displacement(re, shi, displacement);

	mul_v3_v3(displacement, scale);

	/* clamp by displacement bound */
	if(fabs(dot_v3v3(displacement, displacement)) > bound*bound)
		mul_v3_fl(displacement, bound/len_v3(displacement));

	/* add displacement to vertex */
	add_v3_v3(vr->co, displacement);

	/* tag vertex as done */
	vr->flag |= 1;
}

static void displace_render_face(Render *re, ObjectRen *obr, VlakRen *vlr, float scale[3], int thread)
{
	ObjectInstanceRen obi;
	ShadeInput shi;
	VertRen *v1= vlr->v1;
	VertRen *v2= vlr->v2;
	VertRen *v3= vlr->v3;
	VertRen *v4= vlr->v4;

	/* setup fake object instance and shadinput for face */
	memset(&obi, 0, sizeof(ObjectInstanceRen)); 
	obi.obr= obr;
	
	displace_shadeinput_init_face(&shi, &obi, vlr, thread);

	/* displace each vertex, flag is set when already done by another face */
	if(!v1->flag)
		displace_render_vert(re, obr, &shi, v1, 0, scale);
	
	if(!v2->flag)
		displace_render_vert(re, obr, &shi, v2, 1, scale);

	if(!v3->flag)
		displace_render_vert(re, obr, &shi, v3, 2, scale);

	if(v4 && !v4->flag)
		displace_render_vert(re, obr, &shi, v4, 3, scale);
	
	/* recalculate the face normal - if flipped before, flip now */
	if(v4) normal_quad_v3(vlr->n, v4->co, v3->co, v2->co, v1->co);
	else normal_tri_v3(vlr->n, v3->co, v2->co, v1->co);
}

void render_object_displace(Render *re, ObjectRen *obr, int thread)
{
	VlakRen *vlr;
	VertRen *vr;
	float scale[3], (*diffnor)[3]= NULL;
	int i;

	/* calculate new normals + for subdivision, the difference between
	   base smooth and new smooth normals so we can preserve smoothness */
	render_object_calc_vnormals(re, obr, (obr->flag & R_TEMP_COPY)? &diffnor: NULL);

	if((re->params.r.mode & R_SUBDIVISION) && !(obr->flag & R_TEMP_COPY)) {
		/* backup coordinates for later per tile subdivision */
		for(i=0; i<obr->totvert; i++) { 
			vr= render_object_vert_get(obr, i);
			copy_v3_v3(render_vert_get_baseco(obr, vr, 1), vr->co);
			copy_v3_v3(render_vert_get_basenor(obr, vr, 1), vr->n);
		}
	}

	/* displacement scaled by object size */
	mat4_to_size(scale, obr->ob->obmat);
	
	/* clear vertex done flags */
	for(i=0; i<obr->totvert; i++) { 
		vr= render_object_vert_get(obr, i);
		vr->flag= 0;
	}

	/* displace faces */
	for(i=0; i<obr->totvlak; i++){
		vlr=render_object_vlak_get(obr, i);
		displace_render_face(re, obr, vlr, scale, thread);

		if(re->cb.test_break(re->cb.tbh))
			break;
	}

	/* recalculate displaced smooth normals, and apply difference */
	if(!re->cb.test_break(re->cb.tbh))
		render_object_calc_vnormals(re, obr, (obr->flag & R_TEMP_COPY)? &diffnor: NULL);

	if(diffnor)
		MEM_freeN(diffnor);
}

