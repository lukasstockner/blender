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

struct BLaplacianSystem {
	bool is_matrix_computed;
	int total_verts;
	int total_edges;
	int total_faces;
	int total_anchors;
	char defgrp_name[64];		/* Vertex Group name*/
	float (*co)[3];				/* Original vertex coordinates*/
	float (*no)[3];				/* Original vertex normal*/
	float (*delta)[3];			/* Differential Coordinates*/
	int *index_anchors;			/* Static vertex index list*/
	int *unit_verts;			/* Unit vectors of projected edges onto the plane orthogonal to  n*/
	BMVert ** verts;			/* Vertex order by index*/
	BMesh *bm;					/* Bmesh structure pointer*/
	NLContext *context;			/* System for solve general implicit rotations*/
};
typedef struct BLaplacianSystem LaplacianSystem;

static LaplacianSystem * newLaplacianSystem()
{
	LaplacianSystem * sys;
	sys = MEM_callocN(sizeof(LaplacianSystem), "DeformCache");
	if (!sys) {
		return NULL;
	}
	sys->is_matrix_computed = false;
	sys->total_verts = 0;
	sys->total_edges = 0;
	sys->total_faces = 0;
	sys->total_anchors = 0;
	sys->defgrp_name[0] = '\0';
	sys->co = NULL;
	sys->no = NULL;
	sys->delta = NULL;
	sys->index_anchors = NULL;
	sys->unit_verts = NULL;
	sys->verts = NULL;
	sys->bm = NULL;
	sys->context = NULL;
	return sys;
}

static LaplacianSystem * initLaplacianSystem(int totalVerts, int totalEdges, int totalFaces, int totalAnchors, char defgrpName[64])
{
	LaplacianSystem *sys = newLaplacianSystem();
	if (!sys) {
		return NULL;
	}
	sys->is_matrix_computed = false;
	sys->total_verts = totalVerts;
	sys->total_edges = totalEdges;
	sys->total_faces = totalFaces;
	sys->total_anchors = totalAnchors;
	BLI_strncpy(sys->defgrp_name, defgrpName, sizeof(sys->defgrp_name));
	sys->co = (float (*)[3])MEM_callocN(sizeof(float)*(totalVerts*3), "DeformCoordinates");
	sys->no = (float (*)[3])MEM_callocN(sizeof(float)*(totalVerts*3), "DeformNormals");
	sys->delta = (float (*)[3])MEM_callocN(sizeof(float)*totalVerts*3, "DeformDeltas");
	sys->index_anchors = (int *)MEM_callocN(sizeof(int)*(totalAnchors), "DeformAnchors");
	sys->unit_verts = (int *)MEM_callocN(sizeof(int)*totalVerts, "DeformUnitVerts");
	sys->verts = (BMVert**)MEM_callocN(sizeof(BMVert*)*(totalVerts), "DeformVerts");
	memset(sys->no, 0.0, sizeof(float)*totalVerts*3);
	memset(sys->delta, 0.0, sizeof(float)*totalVerts*3);
	return sys;
}

static void deleteVoidPointer(void *data)
{
	if (data) {
		MEM_freeN(data);
		data = NULL;
	}
}

static void deleteLaplacianSystem(LaplacianSystem * sys)
{
	if (!sys) return;
	deleteVoidPointer(sys->co);
	deleteVoidPointer(sys->no);
	deleteVoidPointer(sys->delta);
	deleteVoidPointer(sys->index_anchors);
	deleteVoidPointer(sys->unit_verts);
	deleteVoidPointer(sys->verts);
	if (sys->bm) BM_mesh_free(sys->bm);
	if (sys->context) nlDeleteContext(sys->context);
	deleteVoidPointer(sys);
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

static void initLaplacianMatrix( LaplacianSystem * sys)
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
			normal_quad_v3(no, sys->co[idv1], sys->co[idv2], sys->co[idv3], sys->co[idv4]); 
			add_v3_v3(sys->no[idv4], no);
		} 
		else {
			normal_tri_v3(no, sys->co[idv1], sys->co[idv2], sys->co[idv3]); 
		}
		add_v3_v3(sys->no[idv1], no);
		add_v3_v3(sys->no[idv2], no);
		add_v3_v3(sys->no[idv3], no);


		idv[0] = idv1;
		idv[1] = idv2;
		idv[2] = idv3;
		idv[3] = idv4;

		for (j = 0; j < i; j++) {
			idv1 = idv[j];
			idv2 = idv[(j + 1) % i];
			idv3 = idv[(j + 2) % i];
			idv4 = has_4_vert ? idv[(j + 3) % i] : 0;

			copy_v3_v3( v1, sys->co[idv1]);
			copy_v3_v3( v2, sys->co[idv2]);
			copy_v3_v3( v3, sys->co[idv3]);
			if (has_4_vert) copy_v3_v3(v4, sys->co[idv4]);

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

static void computeImplictRotations(LaplacianSystem * sys)
{
	BMEdge *e;
	BMIter eiter;
	BMIter viter;
	BMVert *v;
	int vid, * vidn = NULL;
	float minj, mjt, qj[3], vj[3];
	int i, j, ln;
	BLI_array_declare(vidn);

	BM_ITER_MESH (v, &viter, sys->bm, BM_VERTS_OF_MESH) {
		i = BM_elem_index_get(v);
		normalize_v3( sys->no[i]);
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
			copy_v3_v3(qj, sys->co[vid]);// vn[j]->co;
			sub_v3_v3v3(vj, qj, sys->co[i]); //sub_v3_v3v3(vj, qj, v->co);
			normalize_v3(vj);
			mjt = fabs(dot_v3v3(vj, sys->no[i])); //mjt = fabs(dot_v3v3(vj, v->no));
			if (mjt < minj) {
				minj = mjt;
				sys->unit_verts[i] = vidn[j];
			}
		}
		
		BLI_array_free(vidn);
		BLI_array_empty(vidn);
		vidn = NULL;
	}
}

static void rotateDifferentialCoordinates(LaplacianSystem * sys)
{
	BMFace *f;
	BMVert *v, *v2;
	BMIter fiter;
	BMIter viter, viter2;
	float alpha, beta, gamma,
		pj[3], ni[3], di[3],
		uij[3], dun[3], e2[3], pi[3], fni[3], vn[4][3];
	int i, j, vin[4], lvin, num_fni, k;


	BM_ITER_MESH (v, &viter, sys->bm, BM_VERTS_OF_MESH) {
		i = BM_elem_index_get(v);
		copy_v3_v3(pi, sys->co[i]); //copy_v3_v3(pi, v->co);
		copy_v3_v3(ni, sys->no[i]); //copy_v3_v3(ni, v->no);
		k = sys->unit_verts[i];
		copy_v3_v3(pj, sys->co[k]); //copy_v3_v3(pj, sys->uverts[i]->co);
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
				if (vin[j] == sys->unit_verts[i]) {
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

static void laplacianDeformPreview(LaplacianSystem * sys, float (*vertexCos)[3])
{
	struct BMesh *bm;
	int vid, i, n, na;
	bm = sys->bm;
	n = sys->total_verts;
	na = sys->total_anchors;

	if (!sys->is_matrix_computed){
		
		nlNewContext();
		sys->context = nlGetCurrent();

		nlSolverParameteri(NL_NB_VARIABLES, n);
		nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
		nlSolverParameteri(NL_NB_ROWS, n + na);
		nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);

		nlBegin(NL_SYSTEM);
		for (i=0; i<n; i++) {
			nlSetVariable(0, i, sys->co[i][0]);
			nlSetVariable(1, i, sys->co[i][1]);
			nlSetVariable(2, i, sys->co[i][2]);
		}
		for (i=0; i<na; i++) {
			vid = sys->index_anchors[i];
			nlSetVariable(0, vid, sys->verts[vid]->co[0]);
			nlSetVariable(1, vid, sys->verts[vid]->co[1]);
			nlSetVariable(2, vid, sys->verts[vid]->co[2]);
		}

		nlBegin(NL_MATRIX);

		initLaplacianMatrix(sys);
		computeImplictRotations(sys);

		for (i=0; i<n; i++) {
			nlRightHandSideSet(0, i, sys->delta[i][0]);
			nlRightHandSideSet(1, i, sys->delta[i][1]);
			nlRightHandSideSet(2, i, sys->delta[i][2]);
		}

		for (i=0; i<na; i++) {
			vid = sys->index_anchors[i];
			nlRightHandSideSet(0, n + i , sys->co[vid][0]);
			nlRightHandSideSet(1, n + i , sys->co[vid][1]);
			nlRightHandSideSet(2, n + i , sys->co[vid][2]);
			nlMatrixAdd(n + i, vid, 1.0f);
		}

		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);

		if (nlSolveAdvanced(NULL, NL_TRUE) ) {
			
			nlBegin(NL_SYSTEM);
			nlBegin(NL_MATRIX);
			rotateDifferentialCoordinates(sys);

			for (i=0; i<na; i++) {
				vid = sys->index_anchors[i];
				nlRightHandSideSet(0, n + i , sys->co[vid][0]);
				nlRightHandSideSet(1, n + i , sys->co[vid][1]);
				nlRightHandSideSet(2, n + i , sys->co[vid][2]);
			}

			nlEnd(NL_MATRIX);
			nlEnd(NL_SYSTEM);
			if (nlSolveAdvanced(NULL, NL_FALSE) ) {
				for (vid=0; vid<sys->total_verts; vid++) {
					vertexCos[vid][0] = nlGetVariable(0, vid);
					vertexCos[vid][1] = nlGetVariable(1, vid);
					vertexCos[vid][2] = nlGetVariable(2, vid);
				}		
			}
		}
		sys->is_matrix_computed = true;
	} else {

		nlBegin(NL_SYSTEM);
		nlBegin(NL_MATRIX);

		for (i=0; i<n; i++) {
			nlRightHandSideSet(0, i  , sys->delta[i][0]);
			nlRightHandSideSet(1, i  , sys->delta[i][1]);
			nlRightHandSideSet(2, i  , sys->delta[i][2]);
		}
		for (i=0; i<na; i++) {
			vid = sys->index_anchors[i];
			nlRightHandSideSet(0, n + i , sys->co[vid][0]);
			nlRightHandSideSet(1, n + i , sys->co[vid][1]);
			nlRightHandSideSet(2, n + i , sys->co[vid][2]);
			nlMatrixAdd(n + i, vid, 1.0f);
		}

		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);

		if (nlSolveAdvanced(NULL, NL_FALSE) ) {
			
			nlBegin(NL_SYSTEM);
			nlBegin(NL_MATRIX);
			rotateDifferentialCoordinates(sys);

			for (i=0; i<na; i++)
			{
				vid = sys->index_anchors[i];
				nlRightHandSideSet(0, n + i	, vertexCos[vid][0]);
				nlRightHandSideSet(1, n + i	, vertexCos[vid][1]);
				nlRightHandSideSet(2, n + i	, vertexCos[vid][2]);
			}
			nlEnd(NL_MATRIX);
			nlEnd(NL_SYSTEM);
			if (nlSolveAdvanced(NULL, NL_FALSE) ) {
				for (vid=0; vid<sys->total_verts; vid++) {
					vertexCos[vid][0] = nlGetVariable(0, vid);
					vertexCos[vid][1] = nlGetVariable(1, vid);
					vertexCos[vid][2] = nlGetVariable(2, vid);
				}			
			}
		}
	}
}

static bool isValidVertexGroup(LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm)
{
	int defgrp_index;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	modifier_get_vgroup(ob, dm, smd->defgrp_name, &dvert, &defgrp_index);
	if (!dvert) return false;
	dvert = NULL;
	return true;
}

static void initSystem(LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm,
				float (*vertexCos)[3], int numVerts)
{
	int i, vertID;
	int defgrp_index;
	int total_anchors;
	int * index_anchors = NULL;
	float wpaint;
	BMIter viter;
	BMVert *v;
	BMesh * bm;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	LaplacianSystem * sys;
	BLI_array_declare(index_anchors);
	
	
	if (isValidVertexGroup(smd, ob, dm)){
		modifier_get_vgroup(ob, dm, smd->defgrp_name, &dvert, &defgrp_index);
		if (!dvert) return;
		dv = dvert;
		bm = DM_to_bmesh(dm, false);
		for (i=0; i<numVerts; i++) {
			if (dv) {
				wpaint = defvert_find_weight(dv, defgrp_index);
				dv++;
				if (wpaint > 0.0f) {
					BLI_array_append(index_anchors, i);
				}
			}
		}
		total_anchors = BLI_array_count(index_anchors);
		smd->cacheSystem = initLaplacianSystem(numVerts, bm->totedge, bm->totface, total_anchors, smd->defgrp_name);
		sys = (LaplacianSystem *)smd->cacheSystem;
		sys->bm = bm;
		for (i=0; i<total_anchors; i++) {
			sys->index_anchors[i] = index_anchors[i];
		}
		for (i=0; i<numVerts; i++) {
			copy_v3_v3(sys->co[i], vertexCos[i]);
		}

		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			vertID = BM_elem_index_get(v);
			sys->verts[vertID] = v;
		}
		BLI_array_free(index_anchors);
		smd->vertexco = (float *)MEM_callocN(sizeof(float)*(numVerts*3), "ModDeformCoordinates");
		memcpy(smd->vertexco, vertexCos, sizeof(float) * numVerts*3);
		smd->total_verts = numVerts;
	}
}

static	bool isSystemDifferent(LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm, int numVerts)
{
	int i;
	int defgrp_index;
	int total_anchors;
	int * index_anchors = NULL;
	float wpaint;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	LaplacianSystem * sys = (LaplacianSystem *)smd->cacheSystem;
	BLI_array_declare(index_anchors);

	if (sys->total_verts != numVerts) return true;
	if (sys->total_edges != dm->getNumEdges(dm)) return true;
	if(BLI_strcasecmp(smd->defgrp_name, sys->defgrp_name) != 0) return true; 
	modifier_get_vgroup(ob, dm, smd->defgrp_name, &dvert, &defgrp_index);
	if (!dvert) return false;
	dv = dvert;
	for (i=0; i<numVerts; i++) {
		if (dv) {
			wpaint = defvert_find_weight(dv, defgrp_index);
			dv++;
			if (wpaint > 0.0f) {
				BLI_array_append(index_anchors, i);
			}
		}
	}
	total_anchors = BLI_array_count(index_anchors);
	BLI_array_free(index_anchors);
	if(sys->total_anchors != total_anchors) return true;
	
		
	return false;
}

static	bool onlyChangueAnchors(LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm, int numVerts)
{
	int i;
	int defgrp_index;
	int total_anchors;
	int * index_anchors = NULL;
	float wpaint;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	LaplacianSystem * sys = (LaplacianSystem *)smd->cacheSystem;
	BLI_array_declare(index_anchors);

	if (sys->total_verts != numVerts) return false;
	if (sys->total_edges != dm->getNumEdges(dm)) return false;
	if(BLI_strcasecmp(smd->defgrp_name, sys->defgrp_name) != 0) return false; 
	modifier_get_vgroup(ob, dm, smd->defgrp_name, &dvert, &defgrp_index);
	if (!dvert) return false;
	dv = dvert;
	for (i=0; i<numVerts; i++) {
		if (dv) {
			wpaint = defvert_find_weight(dv, defgrp_index);
			dv++;
			if (wpaint > 0.0f) {
				BLI_array_append(index_anchors, i);
			}
		}
	}
	total_anchors = BLI_array_count(index_anchors);
	BLI_array_free(index_anchors);
	if(sys->total_anchors != total_anchors) return true;
		
	return false;
}

static	bool onlyChangueGroup(LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm, int numVerts)
{
	int i;
	LaplacianSystem * sys = (LaplacianSystem *)smd->cacheSystem;

	if (sys->total_verts != numVerts) return false;
	if (sys->total_edges != dm->getNumEdges(dm)) return false;
	if(BLI_strcasecmp(smd->defgrp_name, sys->defgrp_name) != 0) return true; 
		
	return false;
}

static void LaplacianDeformModifier_do(
        LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	float (*filevertexCos)[3];
	filevertexCos = NULL;
	if (smd->cacheSystem){
		if (isSystemDifferent(smd, ob, dm,numVerts)){
			if (onlyChangueAnchors(smd, ob, dm,numVerts) || onlyChangueGroup(smd, ob, dm,numVerts)){
				filevertexCos = (float (*)[3])MEM_callocN(sizeof(float)*(numVerts*3), "TempModDeformCoordinates");
				memcpy(filevertexCos, smd->vertexco, sizeof(float)*numVerts*3);
				deleteVoidPointer(smd->vertexco);
				smd->total_verts = 0;
				deleteLaplacianSystem((LaplacianSystem *)smd->cacheSystem);
				initSystem(smd, ob, dm, filevertexCos, numVerts);
				deleteVoidPointer(filevertexCos);
				laplacianDeformPreview((LaplacianSystem *)smd->cacheSystem, vertexCos);
			}else{
				deleteLaplacianSystem((LaplacianSystem *)smd->cacheSystem);
				if(smd->vertexco) {
					MEM_freeN(smd->vertexco);
				}
				smd->total_verts = 0;
				initSystem(smd, ob, dm, vertexCos, numVerts);
				laplacianDeformPreview((LaplacianSystem *)smd->cacheSystem, vertexCos);
			}
		} else {
			laplacianDeformPreview((LaplacianSystem *)smd->cacheSystem, vertexCos);
		}
	}else {
		if (smd->total_verts > 0 && smd->total_verts == numVerts){
			if (isValidVertexGroup(smd, ob, dm)){
				filevertexCos = (float (*)[3])MEM_callocN(sizeof(float)*(numVerts*3), "TempModDeformCoordinates");
				memcpy(filevertexCos, smd->vertexco, sizeof(float)*numVerts*3);
				deleteVoidPointer(smd->vertexco);
				smd->total_verts = 0;
				initSystem(smd, ob, dm, filevertexCos, numVerts);
				deleteVoidPointer(filevertexCos);
				laplacianDeformPreview((LaplacianSystem *)smd->cacheSystem, vertexCos);
				
			}
		} else {
			if (isValidVertexGroup(smd, ob, dm)){
				initSystem(smd, ob, dm, vertexCos, numVerts);
				laplacianDeformPreview((LaplacianSystem *)smd->cacheSystem, vertexCos);
			}
		}
	}
}


static void initData(ModifierData *md)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	smd->defgrp_name[0] = '\0';
	smd->total_verts = 0;
	smd->vertexco = NULL;
	smd->cacheSystem = NULL;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	LaplacianDeformModifierData *tsmd = (LaplacianDeformModifierData *) target;
	tsmd->total_verts = smd->total_verts;
	BLI_strncpy(tsmd->defgrp_name, smd->defgrp_name, sizeof(tsmd->defgrp_name));
	tsmd->vertexco = MEM_dupallocN(smd->vertexco);
	tsmd->cacheSystem = MEM_dupallocN(smd->cacheSystem);
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	//if (!smd->cacheSystem) return 1;
	return 0;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *)md;
	CustomDataMask dataMask = 0;
	return dataMask;
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

static void freeData(ModifierData *md)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	LaplacianSystem * sys = (LaplacianSystem *)(smd->cacheSystem);

	if (sys) {
		deleteLaplacianSystem(sys);
	}
	if (smd->vertexco){
		deleteVoidPointer(smd->vertexco);
	}
	smd->total_verts = 0;
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
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
