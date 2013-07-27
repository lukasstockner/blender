/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Your name
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */
 
/** \file blender/modifiers/intern/MOD_scaling.c
 *  \ingroup modifiers
 */
 
 
#include "DNA_meshdata_types.h"
 
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_array.h"

 
#include "MEM_guardedalloc.h"
 
#include "BKE_cdderivedmesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"

#include "BKE_DerivedMesh.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
 
#include "MOD_modifiertypes.h"
#include "MOD_util.h"

#include "ONL_opennl.h"

struct BStaticAnchors {
	int numStatics;				/* Number of static anchors*/
	int numVerts;				/* Number of verts*/
	int * list_index;			/* Static vertex index list*/
	float (*co)[3];				/* Original vertex coordinates*/
	float (*no)[3];				/* Original vertex normal*/
	BMVert ** list_verts;		/* Vertex order by index*/
};
typedef struct BStaticAnchors StaticAnchors;

struct BHandlerAnchors {
	int numHandlers;			/* Number of handler anchors*/
	int * list_handlers;		/* Static vertex index list*/
};
typedef struct BHandlerAnchors HandlerAnchors;

struct BLaplacianSystem {
	float (*delta)[3];			/* Differential Coordinates*/
	int *list_uverts;			/* Unit vectors of projected edges onto the plane orthogonal to  n*/
	/* Pointers to data*/
	int numVerts;
	int numHandlers;
	int numStatics;
	BMesh *bm;
	NLContext *context;			/* System for solve general implicit rotations*/
};
typedef struct BLaplacianSystem LaplacianSystem;

enum {
	LAP_STATE_INIT = 1,
	LAP_STATE_HAS_STATIC,
	LAP_STATE_HAS_HANDLER,
	LAP_STATE_HAS_STATIC_AND_HANDLER,
	LAP_STATE_HAS_L_COMPUTE,
	LAP_STATE_UPDATE_REQUIRED
};

struct BSystemCustomData {
	LaplacianSystem * sys;
	StaticAnchors  * sa;
	HandlerAnchors * shs;
	int stateSystem;
	bool update_required;
};

typedef struct BSystemCustomData SystemCustomData;

static StaticAnchors * init_static_anchors(int numv, int nums);
static HandlerAnchors * init_handler_anchors(int numh);
static LaplacianSystem * init_laplacian_system(int numv, int nums, int numh);
static float cotan_weight(float *v1, float *v2, float *v3);
static void compute_implict_rotations(SystemCustomData * data);
static void delete_void_pointer(void *data);
static void delete_static_anchors(StaticAnchors * sa);
static void delete_handler_anchors(HandlerAnchors * sh);
static void delete_laplacian_system(LaplacianSystem *sys);
static void init_laplacian_matrix( SystemCustomData * data);
static void rotate_differential_coordinates(SystemCustomData * data);
static void update_system_state(SystemCustomData * data, int state);
static void laplacian_deform_preview(SystemCustomData * data, DerivedMesh *dm, float (*vertexCos)[3]);

static void delete_void_pointer(void *data)
{
	if (data) {
		MEM_freeN(data);
	}
}

static StaticAnchors * init_static_anchors(int numv, int nums)
{
	StaticAnchors * sa;
	sa = (StaticAnchors *)MEM_callocN(sizeof(StaticAnchors), "LapStaticAnchors");
	sa->numVerts = numv;
	sa->numStatics = nums;
	sa->list_index = (int *)MEM_callocN(sizeof(int)*(sa->numStatics), "LapListStatics");
	sa->list_verts = (BMVert**)MEM_callocN(sizeof(BMVert*)*(sa->numVerts), "LapListverts");
	sa->co = (float (*)[3])MEM_callocN(sizeof(float)*(sa->numVerts*3), "LapCoordinates");
	sa->no = (float (*)[3])MEM_callocN(sizeof(float)*(sa->numVerts*3), "LapNormals");
	memset(sa->no, 0.0, sizeof(float) * sa->numVerts * 3);
	return sa;
}

static HandlerAnchors * init_handler_anchors(int numh)
{
	HandlerAnchors * sh;
	sh = (HandlerAnchors *)MEM_callocN(sizeof(HandlerAnchors), "LapHandlerAnchors");
	sh->numHandlers = numh;
	sh->list_handlers = (int *)MEM_callocN(sizeof(int)*(sh->numHandlers), "LapListHandlers");
	return sh;
}

static LaplacianSystem * init_laplacian_system(int numv, int nums, int numh)
{
	LaplacianSystem *sys;
	int rows, cols;
	sys = (LaplacianSystem *)MEM_callocN(sizeof(LaplacianSystem), "LapSystem");
	if (!sys) {
		return NULL;
	}
	sys->numVerts = numv;
	sys->numStatics = nums;
	sys->numHandlers = numh;
	rows = (sys->numVerts + sys->numStatics + sys->numHandlers) * 3;
	cols = sys->numVerts * 3;
	sys->list_uverts = (int *)MEM_callocN(sizeof(BMVert *) * sys->numVerts, "LapUverts");
	sys->delta = (float (*)[3])MEM_callocN(sizeof(float) * sys->numVerts * 3, "LapDelta");
	memset(sys->delta, 0.0, sizeof(float) * sys->numVerts * 3);
	return sys;
}

static void delete_static_anchors(StaticAnchors * sa)
{
	if (!sa) return;
	delete_void_pointer(sa->co);
	delete_void_pointer(sa->list_index);
	delete_void_pointer(sa->no);
	delete_void_pointer(sa->list_verts);
	delete_void_pointer(sa);
	sa = NULL;
}

static void delete_handler_anchors(HandlerAnchors * sh)
{
	if (!sh) return;
	delete_void_pointer(sh->list_handlers);
	delete_void_pointer(sh);
	sh = NULL;
}

static void delete_laplacian_system(LaplacianSystem *sys)
{
	if (!sys) return;
	delete_void_pointer(sys->delta);
	delete_void_pointer(sys->list_uverts);
	sys->bm = NULL;
	if (sys->context) nlDeleteContext(sys->context);
	delete_void_pointer(sys);
	sys = NULL;
}

static void update_system_state(SystemCustomData * data, int state)
{
	if (!data) return;
	switch(data->stateSystem) {
		case LAP_STATE_INIT:
			if (state == LAP_STATE_HAS_STATIC || state == LAP_STATE_HAS_HANDLER) {
				data->stateSystem = state;
			}
			break;
		case LAP_STATE_HAS_STATIC:
			if (state == LAP_STATE_HAS_HANDLER) {
				data->stateSystem = LAP_STATE_HAS_STATIC_AND_HANDLER;
			}
			break;
		case LAP_STATE_HAS_HANDLER:
			if (state == LAP_STATE_HAS_STATIC)  {
				data->stateSystem = LAP_STATE_HAS_STATIC_AND_HANDLER;
			}
			break;
		case LAP_STATE_HAS_STATIC_AND_HANDLER:
			if (state == LAP_STATE_HAS_L_COMPUTE) {
				data->stateSystem = LAP_STATE_HAS_L_COMPUTE;
			} 
			break;
		case LAP_STATE_HAS_L_COMPUTE:
			if (state == LAP_STATE_HAS_STATIC || state == LAP_STATE_HAS_HANDLER) {
				data->stateSystem = LAP_STATE_HAS_STATIC_AND_HANDLER;
			}
			break;
	}

}
 
static void initData(ModifierData *md)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	SystemCustomData * sys;
	sys = smd->custom_data = MEM_callocN(sizeof(SystemCustomData), "LapSystemCustomData");
	if (!sys) {
		return;
	}
	sys->sa = NULL;
	sys->shs = NULL;
	sys->sys = NULL;
	sys->stateSystem = LAP_STATE_INIT;
}
 
static void copyData(ModifierData *md, ModifierData *target)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	LaplacianDeformModifierData *tsmd = (LaplacianDeformModifierData *) target;
	//tsmd->scale = smd->scale;
	tsmd->custom_data = smd->custom_data;
}
 
static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	/* disable if modifier is 1.0 for scale*/
	//if (smd->scale == 1.0f) return 1;
	return 0;
}
 
static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *)md;
	CustomDataMask dataMask = 0;
	return dataMask;
}
 
static void LaplacianDeformModifier_do(
        LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	int i;
	float wpaint;
	SystemCustomData * data = (SystemCustomData *)smd->custom_data;
	MDeformVert *dvert_h = NULL;
	MDeformVert *dvert_s = NULL;
	MDeformVert *dv_h = NULL;
	MDeformVert *dv_s = NULL;
	int defgrp_index_s;
	int defgrp_index_h;
	int * index_h = NULL;
	int * index_s = NULL;
	int nums, numh;
	BMesh * bm;

	int vid;
	BMIter viter;
	BMVert *v;


	BLI_array_declare(index_h);
	BLI_array_declare(index_s);

	modifier_get_vgroup(ob, dm, smd->defgrp_name_s, &dvert_s, &defgrp_index_s);
	modifier_get_vgroup(ob, dm, smd->defgrp_name_h, &dvert_h, &defgrp_index_h);

	if (!data) return;
	if (!dvert_s) return;
	if (!dvert_h) return;

	if (data->stateSystem == LAP_STATE_INIT) {
		dv_h = dvert_h;
		dv_s = dvert_s;
		bm = DM_to_bmesh(dm, false);
		for (i=0; i<numVerts; i++) {
			if (dv_s) {
				wpaint = defvert_find_weight(dv_s, defgrp_index_s);
				dv_s++;
				if (wpaint > 0.0f) {
					BLI_array_append(index_s, i);
				}
			}
			if (dv_h) {
				wpaint = defvert_find_weight(dv_h, defgrp_index_h);
				dv_h++;
				if (wpaint > 0.0f) {
					BLI_array_append(index_h, i);
				}
			}
		}
		nums = BLI_array_count(index_s);
		numh = BLI_array_count(index_h);

		////////// init static
		if (data->sa) {
			if (data->sa->numVerts != numVerts) {
				delete_static_anchors(data->sa);
				data->sa = init_static_anchors(numVerts, nums);
			}
			else {
				delete_void_pointer( data->sa->list_index);
				data->sa->numStatics = nums;
				data->sa->list_index = (int *)MEM_callocN(sizeof(int)*(data->sa->numStatics), "LapListStatics");
			}
		} 
		else {
			data->sa = init_static_anchors(numVerts, nums);
		}

		for (i=0; i<nums; i++) {
			data->sa->list_index[i] = index_s[i];
		}
		
		for (i=0; i<numVerts; i++) {
			copy_v3_v3(data->sa->co[i], vertexCos[i]);
		}

		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			vid = BM_elem_index_get(v);
			data->sa->list_verts[vid] = v;
		}

		////////// init handler

		if (data->shs) {
			delete_handler_anchors(data->shs);
		}
		data->shs = init_handler_anchors(numh);

		for (i=0; i<numh; i++) {
			data->shs->list_handlers[i] = index_h[i];
		}

		BLI_array_free(index_h);
		BLI_array_free(index_s);

		update_system_state(data, LAP_STATE_HAS_STATIC);
		update_system_state(data, LAP_STATE_HAS_HANDLER);
	}

	if (data->stateSystem >= LAP_STATE_HAS_STATIC_AND_HANDLER) {
		laplacian_deform_preview(data, dm, vertexCos);
	}

}
 
static void deformVerts(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                        float (*vertexCos)[3], int numVerts, ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = get_dm(ob, NULL, derivedData, NULL, false, false);
 
	LaplacianDeformModifier_do((LaplacianDeformModifierData *)md, ob, dm,
	                  vertexCos, numVerts);
 
	if (dm != derivedData)
		dm->release(dm);
}
 
static void deformVertsEM(
        ModifierData *md, Object *ob, struct BMEditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = get_dm(ob, editData, derivedData, NULL, false, false);
 
	LaplacianDeformModifier_do((LaplacianDeformModifierData *)md, ob, dm,
	                  vertexCos, numVerts);
 
	if (dm != derivedData)
		dm->release(dm);
}
 

static float cotan_weight(float *v1, float *v2, float *v3)
{
	float a[3], b[3], c[3], clen;

	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v3, v1);
	cross_v3_v3v3(c, a, b);

	clen = len_v3(c);
	if (clen < FLT_EPSILON)
		return 0.0f;

	return dot_v3v3(a, b) / clen;
}

static void init_laplacian_matrix( SystemCustomData * data)
{
	float v1[3], v2[3], v3[3], v4[3], no[3];
	float w2, w3, w4;
	int i, j, vid, vidf[4];
	bool has_4_vert;
	unsigned int idv1, idv2, idv3, idv4, idv[4];
	BMFace *f;
	BMIter fiter;
	BMIter vi;
	BMVert *vn;
	LaplacianSystem * sys = data->sys;
	StaticAnchors * sa = data->sa;

	BM_ITER_MESH (f, &fiter, sys->bm, BM_FACES_OF_MESH) {
	
		BM_ITER_ELEM_INDEX (vn, &vi, f, BM_VERTS_OF_FACE, i) {
			vid = BM_elem_index_get(vn);
			vidf[i] = vid;
		}
		has_4_vert = (i == 4) ? 1 : 0;
		idv1 = vidf[0];
		idv2 = vidf[1];
		idv3 = vidf[2];
		idv4 = has_4_vert ? vidf[3] : 0;
		if (has_4_vert) {
			normal_quad_v3(no, sa->co[idv1], sa->co[idv2], sa->co[idv3], sa->co[idv4]); 
			add_v3_v3(sa->no[idv4], no);
		} 
		else {
			normal_tri_v3(no, sa->co[idv1], sa->co[idv2], sa->co[idv3]); 
		}
		add_v3_v3(sa->no[idv1], no);
		add_v3_v3(sa->no[idv2], no);
		add_v3_v3(sa->no[idv3], no);


		idv[0] = idv1;
		idv[1] = idv2;
		idv[2] = idv3;
		idv[3] = idv4;

		for (j = 0; j < i; j++) {
			idv1 = idv[j];
			idv2 = idv[(j + 1) % i];
			idv3 = idv[(j + 2) % i];
			idv4 = has_4_vert ? idv[(j + 3) % i] : 0;

			copy_v3_v3( v1, sa->co[idv1]);
			copy_v3_v3( v2, sa->co[idv2]);
			copy_v3_v3( v3, sa->co[idv3]);
			if (has_4_vert) copy_v3_v3(v4, sa->co[idv4]);

			if (has_4_vert) {

				w2 = (cotan_weight(v4, v1, v2) + cotan_weight(v3, v1, v2)) /2.0f ;
				w3 = (cotan_weight(v2, v3, v1) + cotan_weight(v4, v1, v3)) /2.0f ;
				w4 = (cotan_weight(v2, v4, v1) + cotan_weight(v3, v4, v1)) /2.0f;

				sys->delta[idv1][0] -=  v4[0] * w4;
				sys->delta[idv1][1] -=  v4[1] * w4;
				sys->delta[idv1][2] -=  v4[2] * w4;

				nlRightHandSideAdd(0, idv1, -v4[0] * w4);
				nlRightHandSideAdd(1, idv1, -v4[1] * w4);
				nlRightHandSideAdd(2, idv1, -v4[2] * w4);

				nlMatrixAdd(idv1, idv4, -w4 );				
			}
			else {
				w2 = cotan_weight(v3, v1, v2);
				w3 = cotan_weight(v2, v3, v1);
				w4 = 0.0f;
			}

			sys->delta[idv1][0] +=  v1[0] * (w2 + w3 + w4);
			sys->delta[idv1][1] +=  v1[1] * (w2 + w3 + w4);
			sys->delta[idv1][2] +=  v1[2] * (w2 + w3 + w4);

			sys->delta[idv1][0] -=  v2[0] * w2;
			sys->delta[idv1][1] -=  v2[1] * w2;
			sys->delta[idv1][2] -=  v2[2] * w2;

			sys->delta[idv1][0] -=  v3[0] * w3;
			sys->delta[idv1][1] -=  v3[1] * w3;
			sys->delta[idv1][2] -=  v3[2] * w3;

			nlMatrixAdd(idv1, idv2, -w2);
			nlMatrixAdd(idv1, idv3, -w3);
			nlMatrixAdd(idv1, idv1, w2 + w3 + w4);
			
		}
	}
}


static void compute_implict_rotations(SystemCustomData * data)
{
	BMEdge *e;
	BMIter eiter;
	BMIter viter;
	BMVert *v;
	int vid, * vidn = NULL;
	float minj, mjt, qj[3], vj[3];
	int i, j, ln;
	LaplacianSystem * sys = data->sys;
	StaticAnchors * sa = data->sa;
	BLI_array_declare(vidn);

	BM_ITER_MESH (v, &viter, sys->bm, BM_VERTS_OF_MESH) {
		i = BM_elem_index_get(v);
		normalize_v3( sa->no[i]);
		BM_ITER_ELEM(e, &eiter, v, BM_EDGES_OF_VERT) {
			vid = BM_elem_index_get(e->v1);
			if (vid == i) {
				vid = BM_elem_index_get(e->v2);
				BLI_array_append(vidn, vid);
			}
			else {
				BLI_array_append(vidn, vid);
			}
		}
		BLI_array_append(vidn, i);
		ln = BLI_array_count(vidn);
		minj = 1000000.0f;
		for (j = 0; j < (ln-1); j++) {
			vid = vidn[j];
			copy_v3_v3(qj, sa->co[vid]);// vn[j]->co;
			sub_v3_v3v3(vj, qj, sa->co[i]); //sub_v3_v3v3(vj, qj, v->co);
			normalize_v3(vj);
			mjt = fabs(dot_v3v3(vj, sa->no[i])); //mjt = fabs(dot_v3v3(vj, v->no));
			if (mjt < minj) {
				minj = mjt;
				sys->list_uverts[i] = vidn[j];
			}
		}
		
		BLI_array_free(vidn);
		BLI_array_empty(vidn);
		vidn = NULL;
	}
}

static void rotate_differential_coordinates(SystemCustomData * data)
{
	BMFace *f;
	BMVert *v, *v2;
	BMIter fiter;
	BMIter viter, viter2;
	float alpha, beta, gamma,
		pj[3], ni[3], di[3],
		uij[3], dun[3], e2[3], pi[3], fni[3], vn[4][3];
	int i, j, vin[4], lvin, num_fni, k;
	LaplacianSystem * sys = data->sys;
	StaticAnchors * sa = data->sa;


	BM_ITER_MESH (v, &viter, sys->bm, BM_VERTS_OF_MESH) {
		i = BM_elem_index_get(v);
		copy_v3_v3(pi, sa->co[i]); //copy_v3_v3(pi, v->co);
		copy_v3_v3(ni, sa->no[i]); //copy_v3_v3(ni, v->no);
		k = sys->list_uverts[i];
		copy_v3_v3(pj, sa->co[k]); //copy_v3_v3(pj, sys->uverts[i]->co);
		sub_v3_v3v3(uij, pj, pi);
		mul_v3_v3fl(dun, ni, dot_v3v3(uij, ni));
		sub_v3_v3(uij, dun);
		normalize_v3(uij);
		cross_v3_v3v3(e2, ni, uij);
		copy_v3_v3(di, sys->delta[i]);
		alpha = dot_v3v3(ni, di);
        beta = dot_v3v3(uij, di);
        gamma = dot_v3v3(e2, di);

		pi[0] = nlGetVariable(0, i);
		pi[1] = nlGetVariable(1, i);
		pi[2] = nlGetVariable(2, i);
		ni[0] = 0.0f;	ni[1] = 0.0f;	ni[2] = 0.0f;
		num_fni = 0;
		BM_ITER_ELEM_INDEX(f, &fiter, v, BM_FACES_OF_VERT, num_fni) {
			BM_ITER_ELEM_INDEX(v2, &viter2, f, BM_VERTS_OF_FACE, j) {
				vin[j] = BM_elem_index_get(v2);
			}
			lvin = j;

			
			for (j=0; j<lvin; j++ ) {
				vn[j][0] = nlGetVariable(0, vin[j]);
				vn[j][1] = nlGetVariable(1, vin[j]);
				vn[j][2] = nlGetVariable(2, vin[j]);
				if (vin[j] == sys->list_uverts[i]) {
					copy_v3_v3(pj, vn[j]);
				}
			}

			if (lvin == 3) {
				normal_tri_v3(fni, vn[0], vn[1], vn[2]);
			} 
			else if(lvin == 4) {
				normal_quad_v3(fni, vn[0], vn[1], vn[2], vn[3]);
			} 

			add_v3_v3(ni, fni);

		}

		normalize_v3(ni);
		sub_v3_v3v3(uij, pj, pi);
		mul_v3_v3fl(dun, ni, dot_v3v3(uij, ni));
		sub_v3_v3(uij, dun);
		normalize_v3(uij);
		cross_v3_v3v3(e2, ni, uij);
		fni[0] = alpha*ni[0] + beta*uij[0] + gamma*e2[0];
		fni[1] = alpha*ni[1] + beta*uij[1] + gamma*e2[1];
		fni[2] = alpha*ni[2] + beta*uij[2] + gamma*e2[2];

		if (len_v3(fni) > FLT_EPSILON) {
			nlRightHandSideSet(0, i, fni[0]);
			nlRightHandSideSet(1, i, fni[1]);
			nlRightHandSideSet(2, i, fni[2]);
		} 
		else {
			nlRightHandSideSet(0, i, sys->delta[i][0]);
			nlRightHandSideSet(1, i, sys->delta[i][1]);
			nlRightHandSideSet(2, i, sys->delta[i][2]);
		}
	}
}


static void laplacian_deform_preview(SystemCustomData * data, DerivedMesh *dm, float (*vertexCos)[3])
{
	LaplacianSystem * sys;
	StaticAnchors * sa;
	HandlerAnchors * shs;
	struct BMesh *bm;
	int vid, i, n, ns, nh;
	BMIter viter;
	BMVert *v;

	if (data->stateSystem < LAP_STATE_HAS_STATIC_AND_HANDLER) return;


	if (data->stateSystem == LAP_STATE_HAS_STATIC_AND_HANDLER ) {
		if (data->sys) {
			delete_laplacian_system(data->sys);
		}

		data->sys = init_laplacian_system(data->sa->numVerts, data->sa->numStatics, data->shs->numHandlers);
		sys = data->sys;
		sa = data->sa;
		shs = data->shs;
		sys->bm = DM_to_bmesh(dm, false);
		bm = sys->bm;
		n = sys->numVerts;
		ns = sa->numStatics;
		nh = shs->numHandlers;
		nlNewContext();
		sys->context = nlGetCurrent();

		nlSolverParameteri(NL_NB_VARIABLES, n);
		nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
		nlSolverParameteri(NL_NB_ROWS, n + nh + ns);
		nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);

		nlBegin(NL_SYSTEM);
		for (i=0; i<n; i++) {
			nlSetVariable(0, i, sa->co[i][0]);
			nlSetVariable(1, i, sa->co[i][1]);
			nlSetVariable(2, i, sa->co[i][2]);
		}
		for (i=0; i<nh; i++) {
			vid = shs->list_handlers[i];
			nlSetVariable(0, vid, sa->list_verts[vid]->co[0]);
			nlSetVariable(1, vid, sa->list_verts[vid]->co[1]);
			nlSetVariable(2, vid, sa->list_verts[vid]->co[2]);
		}
		for (i=0; i<ns; i++) {
			nlLockVariable(sa->list_index[i]);
		}
		nlBegin(NL_MATRIX);

		init_laplacian_matrix(data);
		compute_implict_rotations(data);

		for (i=0; i<n; i++) {
			nlRightHandSideSet(0, i, sys->delta[i][0]);
			nlRightHandSideSet(1, i, sys->delta[i][1]);
			nlRightHandSideSet(2, i, sys->delta[i][2]);
		}

		for (i=0; i<ns; i++) {
			vid = sa->list_index[i];
			nlRightHandSideSet(0, n + i , sa->co[vid][0]);
			nlRightHandSideSet(1, n + i , sa->co[vid][1]);
			nlRightHandSideSet(2, n + i , sa->co[vid][2]);
			nlMatrixAdd(n + i, vid, 1.0f);
		}

		for (i=0; i<nh; i++)
		{
			vid = shs->list_handlers[i];
			nlRightHandSideSet(0, n + ns + i 	, vertexCos[vid][0]);
			nlRightHandSideSet(1, n + ns + i	, vertexCos[vid][1]);
			nlRightHandSideSet(2, n + ns + i	, vertexCos[vid][2]);
			nlMatrixAdd(n + ns + i 		, vid					, 1.0f);
		}
		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);

		if (nlSolveAdvanced(NULL, NL_TRUE) ) {
			
			nlBegin(NL_SYSTEM);
			nlBegin(NL_MATRIX);
			rotate_differential_coordinates(data);

			for (i=0; i<ns; i++) {
				vid = sa->list_index[i];
				nlRightHandSideSet(0, n + i , sa->co[vid][0]);
				nlRightHandSideSet(1, n + i , sa->co[vid][1]);
				nlRightHandSideSet(2, n + i , sa->co[vid][2]);
			}

			for (i=0; i<nh; i++)
			{
				vid = shs->list_handlers[i];
				nlRightHandSideSet(0, n + ns + i 	, vertexCos[vid][0]);
				nlRightHandSideSet(1, n + ns + i	, vertexCos[vid][1]);
				nlRightHandSideSet(2, n + ns + i	, vertexCos[vid][2]);
			}
			nlEnd(NL_MATRIX);
			nlEnd(NL_SYSTEM);
			if (nlSolveAdvanced(NULL, NL_FALSE) ) {
				for (vid=0; vid<sys->numVerts; vid++) {
					vertexCos[vid][0] = nlGetVariable(0, vid);
					vertexCos[vid][1] = nlGetVariable(1, vid);
					vertexCos[vid][2] = nlGetVariable(2, vid);
				}		
			}
			printf("\nSystem solved");
		}
		else{
			printf("\nNo solution found");
		}
		update_system_state(data, LAP_STATE_HAS_L_COMPUTE);
	} else if (data->stateSystem == LAP_STATE_HAS_L_COMPUTE ) {
		sys = data->sys;
		sa = data->sa;
		shs = data->shs;
		//sys->bm = bm;
		n = sys->numVerts;
		ns = sa->numStatics;
		nh = shs->numHandlers;

		nlBegin(NL_SYSTEM);
		nlBegin(NL_MATRIX);

		for (i=0; i<n; i++) {
			nlRightHandSideSet(0, i  , sys->delta[i][0]);
			nlRightHandSideSet(1, i  , sys->delta[i][1]);
			nlRightHandSideSet(2, i  , sys->delta[i][2]);
		}
		for (i=0; i<ns; i++) {
			vid = sa->list_index[i];
			nlRightHandSideSet(0, n + i , sa->co[vid][0]);
			nlRightHandSideSet(1, n + i , sa->co[vid][1]);
			nlRightHandSideSet(2, n + i , sa->co[vid][2]);
			nlMatrixAdd(n + i, vid, 1.0f);
		}

		for (i=0; i<nh; i++)
		{
			vid = shs->list_handlers[i];
			nlRightHandSideSet(0, n + ns + i 	, vertexCos[vid][0]);
			nlRightHandSideSet(1, n + ns + i	, vertexCos[vid][1]);
			nlRightHandSideSet(2, n + ns + i	, vertexCos[vid][2]);
			nlMatrixAdd(n + ns + i 		, vid					, 1.0f);
		}
		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);

		if (nlSolveAdvanced(NULL, NL_FALSE) ) {
			
			nlBegin(NL_SYSTEM);
			nlBegin(NL_MATRIX);
			rotate_differential_coordinates(data);

			for (i=0; i<ns; i++) {
				vid = sa->list_index[i];
				nlRightHandSideSet(0, n + i , sa->co[vid][0]);
				nlRightHandSideSet(1, n + i , sa->co[vid][1]);
				nlRightHandSideSet(2, n + i , sa->co[vid][2]);
			}

			for (i=0; i<nh; i++)
			{
				vid = shs->list_handlers[i];
				nlRightHandSideSet(0, n + ns + i 	, vertexCos[vid][0]);
				nlRightHandSideSet(1, n + ns + i	, vertexCos[vid][1]);
				nlRightHandSideSet(2, n + ns + i	, vertexCos[vid][2]);
			}
			nlEnd(NL_MATRIX);
			nlEnd(NL_SYSTEM);
			if (nlSolveAdvanced(NULL, NL_FALSE) ) {
				for (vid=0; vid<sys->numVerts; vid++) {
					vertexCos[vid][0] = nlGetVariable(0, vid);
					vertexCos[vid][1] = nlGetVariable(1, vid);
					vertexCos[vid][2] = nlGetVariable(2, vid);
				}			
			}
			printf("\nSystem solved 2");
		}
		else{
			printf("\nNo solution found 2");
		}
		update_system_state(data, LAP_STATE_HAS_L_COMPUTE);

	}
}
 
ModifierTypeInfo modifierType_LaplacianDeform = {
	/* name */              "LaplacianDeform",
	/* structName */        "LaplacianDeformModifierData",
	/* structSize */        sizeof(LaplacianDeformModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode,
 
	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
