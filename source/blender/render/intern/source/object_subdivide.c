/**
 * $Id: object.c 27113 2010-02-23 19:48:55Z blendix $
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
 * Contributor(s): 2010, Blender Foundation
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdio.h>

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_math.h"

#include "camera.h"
#include "object.h"
#include "object_mesh.h"
#include "part.h"
#include "render_types.h"

/* XXX hacked test code */

#define MAX_GRID_RES		16
#define MAX_SPLIT_DEPTH		5

static ObjectRen *render_object_temp_copy(ObjectRen *obr)
{
	ObjectRen *obrn;

	obrn= MEM_dupallocN(obr);
	obrn->flag |= R_TEMP_COPY;

	obrn->next= obrn->prev= NULL;
	obrn->vertnodes= NULL;
	obrn->vlaknodes= NULL;
	obrn->vertnodeslen= 0;
	obrn->vlaknodeslen= 0;
	obrn->totvert= 0;
	obrn->totvlak= 0;

	return obrn;
}

static ObjectInstanceRen *render_instance_temp_copy(ObjectInstanceRen *obi)
{
	ObjectInstanceRen *obin;

	obin= MEM_callocN(sizeof(ObjectInstanceRen), "ObjectInstanceRen");
	memcpy(obin, obi, sizeof(ObjectInstanceRen));
	obin->next= obin->prev= NULL;
	obin->vectors= NULL; // ...
	obin->raytree= NULL; // ...

	return obin;
}

static void extend_bb_disp(float result[2][3], float bb[2][3], float dispbound)
{
	result[0][0]= bb[0][0] - dispbound;
	result[0][1]= bb[0][1] - dispbound;
	result[0][2]= bb[0][2] - dispbound;

	result[1][0]= bb[1][0] + dispbound;
	result[1][1]= bb[1][1] + dispbound;
	result[1][2]= bb[1][2] + dispbound;
}

static int render_vlak_clip(VlakRen *vlr, float M[4][4], float winmat[4][4], float bounds[4], float dispbound, int quad)
{
	float *co1, *co2, *co3, co[3];
	float bb[2][3], dispbb[2][3];

	if(quad) {
		co1= vlr->v1->co;
		co2= vlr->v3->co;
		co3= vlr->v4->co;
	}
	else {
		co1= vlr->v1->co;
		co2= vlr->v2->co;
		co3= vlr->v3->co;
	}

	INIT_MINMAX(bb[0], bb[1]);

	if(M) {
		interp_v3_v3v3v3(co, co1, co2, co3, M[0]);
		DO_MINMAX(co, bb[0], bb[1]);
		interp_v3_v3v3v3(co, co1, co2, co3, M[1]);
		DO_MINMAX(co, bb[0], bb[1]);
		interp_v3_v3v3v3(co, co1, co2, co3, M[2]);
		DO_MINMAX(co, bb[0], bb[1]);
	}
	else {
		DO_MINMAX(co1, bb[0], bb[1]);
		DO_MINMAX(co2, bb[0], bb[1]);
		DO_MINMAX(co3, bb[0], bb[1]);
	}

	extend_bb_disp(dispbb, bb, dispbound);

	return box_clip_bounds_m4(dispbb, bounds, winmat);
}

static VlakRen *render_vlak_sub_copy(ObjectRen *obrn, ObjectRen *obr, VlakRen *vlr, VertRen *v1, VertRen *v2, VertRen *v3, float w[4][4])
{
	VlakRen *vlrn;

	vlrn= render_object_vlak_interp(obrn, obr, vlr, w);
	vlrn->ec= ME_V1V2|ME_V2V3|ME_V3V4;

	vlrn->v1= v1;
	vlrn->v2= v2;
	vlrn->v3= v3;
	vlrn->v4= NULL;

	return vlrn;
}

static int render_face_view_resolution(RenderCamera *cam, float winmat[4][4], VlakRen *vlr, float M[4][4], int quad, float shadingrate)
{
	float maxlen, hoco[3][4], zco[3][3];
	float *co1, *co2, *co3, co[3];
	int a, res;

	if(quad) {
		co1= vlr->v1->co;
		co2= vlr->v3->co;
		co3= vlr->v4->co;
	}
	else {
		co1= vlr->v1->co;
		co2= vlr->v2->co;
		co3= vlr->v3->co;
	}

	if(M) {
		interp_v3_v3v3v3(co, co1, co2, co3, M[0]);
		camera_matrix_co_to_hoco(winmat, hoco[0], co);
		interp_v3_v3v3v3(co, co1, co2, co3, M[1]);
		camera_matrix_co_to_hoco(winmat, hoco[1], co);
		interp_v3_v3v3v3(co, co1, co2, co3, M[2]);
		camera_matrix_co_to_hoco(winmat, hoco[2], co);
	}
	else {
		camera_matrix_co_to_hoco(winmat, hoco[0], co1);
		camera_matrix_co_to_hoco(winmat, hoco[1], co2);
		camera_matrix_co_to_hoco(winmat, hoco[2], co3);
	}

	maxlen= 0.0f;

	for(a=0; a<3; a++)
		camera_hoco_to_zco(cam, zco[a], hoco[a]);
	for(a=0; a<3; a++)
		maxlen= maxf(maxlen, len_v2v2(zco[a], zco[(a+1)%3]));
	
	res= (int)(0.5f + maxlen/shadingrate);
	res= MAX2(2, res);

	return res;
}

static void vertex_weight(float w[4], float M[4][4], float u, float v, int vi[4])
{
	float tmp[4];

	tmp[0]= (1-u)*(1-v) + u*(1-v);
	tmp[1]= (1-u)*v;
	tmp[2]= u*v;
	tmp[3]= 0.0f;

	mul_m4_v4(M, tmp);

	w[vi[0]]= tmp[0];
	w[vi[1]]= tmp[1];
	w[vi[2]]= tmp[2];
	w[vi[3]]= tmp[3];
}

static void face_vertex_weight(float w[4], float M[4][4], int i, int j, int res, int vi[4])
{
	float u= ((j > 0)? i/(float)j: 0.0f);
	float v= (j/(float)(res-1));

	vertex_weight(w, M, u, v, vi);
}

static void render_face_subdivide(RenderCamera *cam, float winmat[4][4], float bounds[4], ObjectRen *obrn, ObjectRen *obr, VlakRen *vlr, int quad, int depth, float M[4][4], float displacebound, float shadingrate)
{
	VertRen *s1, *s2, *s3;
	float w[4], pM[4][4], fw[4][4];
	int i, j, vi[4], offset, res, split;

	res= render_face_view_resolution(cam, winmat, vlr, M, quad, shadingrate);

	split= 0;
	if(res > MAX_GRID_RES) {
		if(depth < MAX_SPLIT_DEPTH) {
			split= 1;
			res= 3;
		}
		else
			res= MAX_GRID_RES;
	}

	if(quad) {
		vi[0]= 0; vi[1]= 2; vi[2]= 3; vi[3]= 1;
	}
	else {
		vi[0]= 0; vi[1]= 1; vi[2]= 2; vi[3]= 3;
	}

	if(M) copy_m4_m4(pM, M);
	else unit_m4(pM);

	offset= obrn->totvert;

	if(split) {
		float pattern[4][4][2] = {
			{{0.0f, 0.0f}, {0.0f, 0.5f}, {1.0f, 0.5f}, {0.0f, 0.0f}},
			{{0.0f, 0.5f}, {0.0f, 1.0f}, {0.5f, 1.0f}, {0.0f, 0.0f}},
			{{0.0f, 0.5f}, {0.5f, 1.0f}, {1.0f, 0.5f}, {0.0f, 0.0f}},
			{{1.0f, 0.5f}, {0.5f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}}};
		float sM[4][4];

		for(i=0; i<4; i++) {
			for(j=0; j<4; j++) {
				int tmp[4] = {0, 1, 2, 3};
				vertex_weight(sM[j], pM, pattern[i][j][0], pattern[i][j][1], tmp);
			}

			if(!render_vlak_clip(vlr, sM, winmat, bounds, displacebound, quad))
				render_face_subdivide(cam, winmat, bounds, obrn, obr, vlr, quad, depth+1, sM, displacebound, shadingrate);
		}
	}
	else {
		for(j=0; j<res; j++) {
			for(i=0; i<=j; i++) {
				face_vertex_weight(w, pM, i, j, res, vi);

				render_object_vert_interp(obrn, obr, &vlr->v1, w, (vlr->v4)? 4: 3);

				if(i > 0 && j > 0) {
					s1= render_object_vert_get(obrn, offset-j-1);
					s2= render_object_vert_get(obrn, offset-1);
					s3= render_object_vert_get(obrn, offset);

					face_vertex_weight(fw[0], pM, i-1, j-1, res, vi);
					face_vertex_weight(fw[1], pM, i-1, j, res, vi);
					face_vertex_weight(fw[2], pM, i, j, res, vi);
					zero_v4(fw[3]);

					render_vlak_sub_copy(obrn, obr, vlr, s1, s2, s3, fw);

					if(i < j) {
						s2= render_object_vert_get(obrn, offset-j);

						face_vertex_weight(fw[0], pM, i-1, j-1, res, vi);
						face_vertex_weight(fw[1], pM, i, j-1, res, vi);
						face_vertex_weight(fw[2], pM, i-1, j, res, vi);
						zero_v4(fw[3]);

						render_vlak_sub_copy(obrn, obr, vlr, s1, s3, s2, fw);
					}
				}

				offset++;
			}
		}
	}
}

ObjectRen *render_object_tile_subdivide(ObjectRen *obr, RenderCamera *cam, float winmat[4][4], float bounds[4])
{
	ObjectRen *obrn;
	VlakRen *vlr;
	int a;
	float displacebound= obr->ob->displacebound;
	float shadingrate= obr->ob->shadingrate;

	obrn= render_object_temp_copy(obr);

	/* copy subdivided faces */
	for(a=0; a<obr->totvlak; a++) {
		vlr= render_object_vlak_get(obr, a);

		if(!render_vlak_clip(vlr, NULL, winmat, bounds, displacebound, 0))
			render_face_subdivide(cam, winmat, bounds, obrn, obr, vlr, 0, 0, NULL, displacebound, shadingrate);

		if(vlr->v4 && !render_vlak_clip(vlr, NULL, winmat, bounds, displacebound, 1))
			render_face_subdivide(cam, winmat, bounds, obrn, obr, vlr, 1, 0, NULL, displacebound, shadingrate);
	}

	return obrn;
}

static ObjectRen *render_instance_tile_subdivide(Render *re, ObjectInstanceRen *obi, float winmat[4][4], float bounds[4])
{
	ObjectRen *obrn;

	obrn= render_object_tile_subdivide(obi->obr, &re->cam, winmat, bounds);
	finalize_render_object(re, obrn, 0);

	return obrn;
}

void part_subdivide_objects(Render *re, RenderPart *pa)
{
	ObjectRen *obr, *obrn;
	ObjectInstanceRen *obi= re->db.objectinstance, *obin;
	float winmat[4][4], bounds[4], obwinmat[4][4];
	float dispbb[2][3];
	int a, tot= re->db.totinstance;

	camera_window_matrix(&re->cam, winmat);
	camera_window_rect_bounds(re->cam.winx, re->cam.winy, &pa->disprect, bounds);

	pa->subdivhash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);

	for(a=0; a<tot; a++, obi++) {
		obr= obi->obr;

		if(!(obr->ob->renderflag & OB_RENDER_SUBDIVIDE))
			continue;

		if(obi->flag & R_TRANSFORMED)
			mul_m4_m4m4(obwinmat, obi->mat, winmat);
		else
			copy_m4_m4(obwinmat, winmat);

		extend_bb_disp(dispbb, obr->boundbox, obr->ob->displacebound);

		if(box_clip_bounds_m4(dispbb, bounds, obwinmat))
			continue;

		obin= render_instance_temp_copy(obi);

		obrn= render_instance_tile_subdivide(re, obi, winmat, bounds);
		obin->obr= obrn;

		BLI_ghash_insert(pa->subdivhash, obi, obin);
	}
}

static void part_subdiv_object_free(void *obi_p)
{
	ObjectInstanceRen *obi= obi_p;

	render_object_free(obi->obr);
	render_instance_free(obi);

	MEM_freeN(obi->obr);
	MEM_freeN(obi);
}

void part_subdivide_free(RenderPart *pa)
{
	if(pa && pa->subdivhash) {
		BLI_ghash_free(pa->subdivhash, NULL, part_subdiv_object_free);
		pa->subdivhash= NULL;
	}
}

ObjectInstanceRen *part_get_instance(RenderPart *pa, ObjectInstanceRen *obi)
{
	if(pa && pa->subdivhash) {
		ObjectInstanceRen *subobi= BLI_ghash_lookup(pa->subdivhash, obi);
		return (subobi)? subobi: obi;
	}

	return obi;
}

