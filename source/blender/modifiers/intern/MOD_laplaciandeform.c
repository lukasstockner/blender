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
 * Contributor(s): Alexander Pinzon Fernandez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */
 
/** \file blender/modifiers/intern/MOD_laplaciandeform.c
 *  \ingroup modifiers
 */
 
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "MEM_guardedalloc.h"
#include "BKE_mesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_report.h"
#include "MOD_modifiertypes.h"
#include "MOD_util.h"
#include "ONL_opennl.h"

#define LAPDEFORM_SYSTEM_NOT_CHANGE 0
#define LAPDEFORM_SYSTEM_IS_DIFFERENT 1
#define LAPDEFORM_SYSTEM_ONLY_CHANGE_ANCHORS 2
#define LAPDEFORM_SYSTEM_ONLY_CHANGE_GROUP 3
#define LAPDEFORM_SYSTEM_ONLY_CHANGE_MESH 4

typedef struct LaplacianSystem {
	bool is_matrix_computed;
	bool has_solution;
	int total_verts;
	int total_edges;
	int total_faces;
	int total_anchors;
	int repeat;
	char anchor_grp_name[64];	/* Vertex Group name*/
	float (*co)[3];				/* Original vertex coordinates*/
	float (*no)[3];				/* Original vertex normal*/
	float (*delta)[3];			/* Differential Coordinates*/
	unsigned int (*faces)[4];	/* Copy of MFace (tessface) v1-v4 */
	int *index_anchors;			/* Static vertex index list*/
	int *unit_verts;			/* Unit vectors of projected edges onto the plane orthogonal to n*/
	int *ringf_indices;			/* Indices of faces per vertex*/
	int *ringv_indices;			/* Indices of neighbors(vertex) per vertex*/
	NLContext *context;			/* System for solve general implicit rotations*/
	MeshElemMap *ringf_map;		/* Map of faces per vertex*/
	MeshElemMap *ringv_map;		/* Map of vertex per vertex*/
} LaplacianSystem;

static LaplacianSystem *newLaplacianSystem(void)
{
	LaplacianSystem *sys;
	sys = MEM_callocN(sizeof(LaplacianSystem), "DeformCache");

	sys->is_matrix_computed = false;
	sys->has_solution = false;
	sys->total_verts = 0;
	sys->total_edges = 0;
	sys->total_anchors = 0;
	sys->total_faces = 0;
	sys->repeat = 1;
	sys->anchor_grp_name[0] = '\0';

	return sys;
}

static LaplacianSystem *initLaplacianSystem(int totalVerts, int totalEdges, int totalFaces, int totalAnchors,
                                            const char defgrpName[64], int iterations)
{
	LaplacianSystem *sys = newLaplacianSystem();

	sys->is_matrix_computed = false;
	sys->has_solution = false;
	sys->total_verts = totalVerts;
	sys->total_edges = totalEdges;
	sys->total_faces = totalFaces;
	sys->total_anchors = totalAnchors;
	sys->repeat = iterations;
	BLI_strncpy(sys->anchor_grp_name, defgrpName, sizeof(sys->anchor_grp_name));
	sys->co = MEM_mallocN(sizeof(float[3]) * totalVerts, "DeformCoordinates");
	sys->no = MEM_callocN(sizeof(float[3]) * totalVerts, "DeformNormals");
	sys->delta = MEM_callocN(sizeof(float[3]) * totalVerts, "DeformDeltas");
	sys->faces = MEM_mallocN(sizeof(int[4]) * totalFaces, "DeformFaces");
	sys->index_anchors = MEM_mallocN(sizeof(int) * (totalAnchors), "DeformAnchors");
	sys->unit_verts = MEM_callocN(sizeof(int) * totalVerts, "DeformUnitVerts");
	return sys;
}

static void deleteLaplacianSystem(LaplacianSystem *sys)
{
	MEM_SAFE_FREE(sys->co);
	MEM_SAFE_FREE(sys->no);
	MEM_SAFE_FREE(sys->delta);
	MEM_SAFE_FREE(sys->faces);
	MEM_SAFE_FREE(sys->index_anchors);
	MEM_SAFE_FREE(sys->unit_verts);
	MEM_SAFE_FREE(sys->ringf_indices);
	MEM_SAFE_FREE(sys->ringv_indices);
	MEM_SAFE_FREE(sys->ringf_map);
	MEM_SAFE_FREE(sys->ringv_map);
	if (sys->context) {
		nlDeleteContext(sys->context);
	}
	MEM_SAFE_FREE(sys);
}

static float cotan_weight(const float v1[3], const float v2[3], const float v3[3])
{
	float a[3], b[3], c[3], clen;

	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v3, v1);
	cross_v3_v3v3(c, a, b);

	clen = len_v3(c);
	if (clen < FLT_EPSILON) {
		return 0.0f;
	}

	return dot_v3v3(a, b) / clen;
}

static void createFaceRingMap(
        const int mvert_tot, const MFace *mface, const int mface_tot,
        MeshElemMap **r_map, int **r_indices)
{
	int i, j, totalr = 0;
	int *indices, *index_iter;
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * mvert_tot, "DeformRingMap");
	const MFace *mf;

	for (i = 0, mf = mface; i < mface_tot; i++, mf++) {
		bool has_4_vert;

		has_4_vert = mf->v4 ? 1 : 0;

		for (j = 0; j < (has_4_vert ? 4 : 3); j++) {
			const unsigned int v_index = (*(&mf->v1 + j));
			map[v_index].count++;
			totalr++;
		}
	}
	indices = MEM_callocN(sizeof(int) * totalr, "DeformRingIndex");
	index_iter = indices;
	for (i = 0; i < mvert_tot; i++) {
		map[i].indices = index_iter;
		index_iter += map[i].count;
		map[i].count = 0;
	}
	for (i = 0, mf = mface; i < mface_tot; i++, mf++) {
		bool has_4_vert;

		has_4_vert = mf->v4 ? 1 : 0;

		for (j = 0; j < (has_4_vert ? 4 : 3); j++) {
			const unsigned int v_index = (*(&mf->v1 + j));
			map[v_index].indices[map[v_index].count] = i;
			map[v_index].count++;
		}
	}
	*r_map = map;
	*r_indices = indices;
}

static void createVertRingMap(
        const int mvert_tot, const MEdge *medge, const int medge_tot,
        MeshElemMap **r_map, int **r_indices)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * mvert_tot, "DeformNeighborsMap");
	int i, vid[2], totalr = 0;
	int *indices, *index_iter;
	const MEdge *me;

	for (i = 0, me = medge; i < medge_tot; i++, me++) {
		vid[0] = me->v1;
		vid[1] = me->v2;
		map[vid[0]].count++;
		map[vid[1]].count++;
		totalr += 2;
	}
	indices = MEM_callocN(sizeof(int) * totalr, "DeformNeighborsIndex");
	index_iter = indices;
	for (i = 0; i < mvert_tot; i++) {
		map[i].indices = index_iter;
		index_iter += map[i].count;
		map[i].count = 0;
	}
	for (i = 0, me = medge; i < medge_tot; i++, me++) {
		vid[0] = me->v1;
		vid[1] = me->v2;
		map[vid[0]].indices[map[vid[0]].count] = vid[1];
		map[vid[0]].count++;
		map[vid[1]].indices[map[vid[1]].count] = vid[0];
		map[vid[1]].count++;
	}
	*r_map = map;
	*r_indices = indices;
}

/**
* This method computes the Laplacian Matrix and Differential Coordinates for all vertex in the mesh.
* The Linear system is LV = d
* Where L is Laplacian Matrix, V as the vertexes in Mesh, d is the differential coordinates
* The Laplacian Matrix is computes as a
* Lij = sum(Wij) (if i == j)
* Lij = Wij (if i != j)
* Wij is weight between vertex Vi and vertex Vj, we use cotangent weight 
* 
* The Differential Coordinate is computes as a 
* di = Vi * sum(Wij) - sum(Wij * Vj)
* Where :
* di is the Differential Coordinate i
* sum (Wij) is the sum of all weights between vertex Vi and its vertexes neighbors (Vj)
* sum (Wij * Vj) is the sum of the product between vertex neighbor Vj and weight Wij for all neighborhood.
* 
* This Laplacian Matrix is described in the paper:
* Desbrun M. et.al, Implicit fairing of irregular meshes using diffusion and curvature flow, SIGGRAPH '99, pag 317-324, 
* New York, USA
* 
* The computation of Laplace Beltrami operator on Hybrid Triangle/Quad Meshes is described in the paper:
* Pinzon A., Romero E., Shape Inflation With an Adapted Laplacian Operator For Hybrid Quad/Triangle Meshes,
* Conference on Graphics Patterns and Images, SIBGRAPI, 2013
* 
* The computation of Differential Coordinates is described in the paper:
* Sorkine, O. Laplacian Surface Editing. Proceedings of the EUROGRAPHICS/ACM SIGGRAPH Symposium on Geometry Processing, 
* 2004. p. 179-188.
*/
static void initLaplacianMatrix(LaplacianSystem *sys)
{
	float v1[3], v2[3], v3[3], v4[3], no[3];
	float w2, w3, w4;
	int i, j, fi;
	bool has_4_vert;
	unsigned int idv1, idv2, idv3, idv4;

	for (fi = 0; fi < sys->total_faces; fi++) {
		const unsigned int *vidf = sys->faces[fi];

		idv1 = vidf[0];
		idv2 = vidf[1];
		idv3 = vidf[2];
		idv4 = vidf[3];

		has_4_vert = vidf[3] ? 1 : 0;
		if (has_4_vert) {
			normal_quad_v3(no, sys->co[idv1], sys->co[idv2], sys->co[idv3], sys->co[idv4]); 
			add_v3_v3(sys->no[idv4], no);
			i = 4;
		} 
		else {
			normal_tri_v3(no, sys->co[idv1], sys->co[idv2], sys->co[idv3]); 
			i = 3;
		}
		add_v3_v3(sys->no[idv1], no);
		add_v3_v3(sys->no[idv2], no);
		add_v3_v3(sys->no[idv3], no);

		for (j = 0; j < i; j++) {
			idv1 = vidf[j];
			idv2 = vidf[(j + 1) % i];
			idv3 = vidf[(j + 2) % i];
			idv4 = has_4_vert ? vidf[(j + 3) % i] : 0;

			copy_v3_v3(v1, sys->co[idv1]);
			copy_v3_v3(v2, sys->co[idv2]);
			copy_v3_v3(v3, sys->co[idv3]);
			if (has_4_vert) {
				copy_v3_v3(v4, sys->co[idv4]);
			}

			if (has_4_vert) {

				w2 = (cotan_weight(v4, v1, v2) + cotan_weight(v3, v1, v2)) / 2.0f;
				w3 = (cotan_weight(v2, v3, v1) + cotan_weight(v4, v1, v3)) / 2.0f;
				w4 = (cotan_weight(v2, v4, v1) + cotan_weight(v3, v4, v1)) / 2.0f;

				sys->delta[idv1][0] -= v4[0] * w4;
				sys->delta[idv1][1] -= v4[1] * w4;
				sys->delta[idv1][2] -= v4[2] * w4;

				nlRightHandSideAdd(0, idv1, -v4[0] * w4);
				nlRightHandSideAdd(1, idv1, -v4[1] * w4);
				nlRightHandSideAdd(2, idv1, -v4[2] * w4);

				nlMatrixAdd(idv1, idv4, -w4);
			}
			else {
				w2 = cotan_weight(v3, v1, v2);
				w3 = cotan_weight(v2, v3, v1);
				w4 = 0.0f;
			}

			sys->delta[idv1][0] += v1[0] * (w2 + w3 + w4);
			sys->delta[idv1][1] += v1[1] * (w2 + w3 + w4);
			sys->delta[idv1][2] += v1[2] * (w2 + w3 + w4);

			sys->delta[idv1][0] -= v2[0] * w2;
			sys->delta[idv1][1] -= v2[1] * w2;
			sys->delta[idv1][2] -= v2[2] * w2;

			sys->delta[idv1][0] -= v3[0] * w3;
			sys->delta[idv1][1] -= v3[1] * w3;
			sys->delta[idv1][2] -= v3[2] * w3;

			nlMatrixAdd(idv1, idv2, -w2);
			nlMatrixAdd(idv1, idv3, -w3);
			nlMatrixAdd(idv1, idv1, w2 + w3 + w4);
			
		}
	}
}

static void computeImplictRotations(LaplacianSystem *sys)
{
	int vid, * vidn = NULL;
	float minj, mjt, qj[3], vj[3];
	int i, j, ln;

	for (i = 0; i < sys->total_verts; i++) {
		normalize_v3(sys->no[i]);
		vidn = sys->ringv_map[i].indices;
		ln = sys->ringv_map[i].count;
		minj = 1000000.0f;
		for (j = 0; j < ln; j++) {
			vid = vidn[j];
			copy_v3_v3(qj, sys->co[vid]);
			sub_v3_v3v3(vj, qj, sys->co[i]);
			normalize_v3(vj);
			mjt = fabsf(dot_v3v3(vj, sys->no[i]));
			if (mjt < minj) {
				minj = mjt;
				sys->unit_verts[i] = vidn[j];
			}
		}
	}
}

static void rotateDifferentialCoordinates(LaplacianSystem *sys)
{
	float alpha, beta, gamma;
	float pj[3], ni[3], di[3];
	float uij[3], dun[3], e2[3], pi[3], fni[3], vn[4][3];
	int i, j, lvin, num_fni, k, fi;
	int *fidn;

	for (i = 0; i < sys->total_verts; i++) {
		copy_v3_v3(pi, sys->co[i]); 
		copy_v3_v3(ni, sys->no[i]); 
		k = sys->unit_verts[i];
		copy_v3_v3(pj, sys->co[k]); 
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
		num_fni = sys->ringf_map[i].count;
		for (fi = 0; fi < num_fni; fi++) {
			const unsigned int *vin;
			fidn = sys->ringf_map[i].indices;
			vin = sys->faces[fidn[fi]];
			lvin = vin[3] ? 4 : 3;
			for (j = 0; j < lvin; j++) {
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
			else if (lvin == 4) {
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
		fni[0] = alpha * ni[0] + beta * uij[0] + gamma * e2[0];
		fni[1] = alpha * ni[1] + beta * uij[1] + gamma * e2[1];
		fni[2] = alpha * ni[2] + beta * uij[2] + gamma * e2[2];

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

static void laplacianDeformPreview(LaplacianSystem *sys, float (*vertexCos)[3])
{
	int vid, i, j, n, na;
	n = sys->total_verts;
	na = sys->total_anchors;

	if (!sys->is_matrix_computed) {
		nlNewContext();
		sys->context = nlGetCurrent();

		nlSolverParameteri(NL_NB_VARIABLES, n);
		nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
		nlSolverParameteri(NL_NB_ROWS, n + na);
		nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);
		nlBegin(NL_SYSTEM);
		for (i = 0; i < n; i++) {
			nlSetVariable(0, i, sys->co[i][0]);
			nlSetVariable(1, i, sys->co[i][1]);
			nlSetVariable(2, i, sys->co[i][2]);
		}
		for (i = 0; i < na; i++) {
			vid = sys->index_anchors[i];
			nlSetVariable(0, vid, vertexCos[vid][0]);
			nlSetVariable(1, vid, vertexCos[vid][1]);
			nlSetVariable(2, vid, vertexCos[vid][2]);
		}
		nlBegin(NL_MATRIX);

		initLaplacianMatrix(sys);
		computeImplictRotations(sys);

		for (i = 0; i < n; i++) {
			nlRightHandSideSet(0, i, sys->delta[i][0]);
			nlRightHandSideSet(1, i, sys->delta[i][1]);
			nlRightHandSideSet(2, i, sys->delta[i][2]);
		}
		for (i = 0; i < na; i++) {
			vid = sys->index_anchors[i];
			nlRightHandSideSet(0, n + i, sys->co[vid][0]);
			nlRightHandSideSet(1, n + i, sys->co[vid][1]);
			nlRightHandSideSet(2, n + i, sys->co[vid][2]);
			nlMatrixAdd(n + i, vid, 1.0f);
		}
		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);
		if (nlSolveAdvanced(NULL, NL_TRUE)) {
			sys->has_solution = true;
			
			for (j = 1; j <= sys->repeat; j++) {
				nlBegin(NL_SYSTEM);
				nlBegin(NL_MATRIX);
				rotateDifferentialCoordinates(sys);

				for (i = 0; i < na; i++) {
					vid = sys->index_anchors[i];
					nlRightHandSideSet(0, n + i, sys->co[vid][0]);
					nlRightHandSideSet(1, n + i, sys->co[vid][1]);
					nlRightHandSideSet(2, n + i, sys->co[vid][2]);
				}

				nlEnd(NL_MATRIX);
				nlEnd(NL_SYSTEM);
				if (!nlSolveAdvanced(NULL, NL_FALSE)) {
					sys->has_solution = false;
					break;
				}
			}
			if (sys->has_solution) {
				for (vid = 0; vid < sys->total_verts; vid++) {
					vertexCos[vid][0] = nlGetVariable(0, vid);
					vertexCos[vid][1] = nlGetVariable(1, vid);
					vertexCos[vid][2] = nlGetVariable(2, vid);
				}		
			} 
			else {
				sys->has_solution = false;
			}
			
		} 
		else {
			sys->has_solution = false;
		}
		sys->is_matrix_computed = true;
	} 
	else {
		if (!sys->has_solution) {
			return;
		}

		nlBegin(NL_SYSTEM);
		nlBegin(NL_MATRIX);

		for (i = 0; i < n; i++) {
			nlRightHandSideSet(0, i, sys->delta[i][0]);
			nlRightHandSideSet(1, i, sys->delta[i][1]);
			nlRightHandSideSet(2, i, sys->delta[i][2]);
		}
		for (i = 0; i < na; i++) {
			vid = sys->index_anchors[i];
			nlRightHandSideSet(0, n + i	, vertexCos[vid][0]);
			nlRightHandSideSet(1, n + i	, vertexCos[vid][1]);
			nlRightHandSideSet(2, n + i	, vertexCos[vid][2]);
			nlMatrixAdd(n + i, vid, 1.0f);
		}

		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);
		if (nlSolveAdvanced(NULL, NL_FALSE)) {
			sys->has_solution = true;
			for (j = 1; j <= sys->repeat; j++) {
				nlBegin(NL_SYSTEM);
				nlBegin(NL_MATRIX);
				rotateDifferentialCoordinates(sys);

				for (i = 0; i < na; i++) {
					vid = sys->index_anchors[i];
					nlRightHandSideSet(0, n + i, vertexCos[vid][0]);
					nlRightHandSideSet(1, n + i, vertexCos[vid][1]);
					nlRightHandSideSet(2, n + i, vertexCos[vid][2]);
				}
				nlEnd(NL_MATRIX);
				nlEnd(NL_SYSTEM);
				if (!nlSolveAdvanced(NULL, NL_FALSE)) {
					sys->has_solution = false;
					break;
				}
			}
			if (sys->has_solution) {
				for (vid = 0; vid < sys->total_verts; vid++) {
					vertexCos[vid][0] = nlGetVariable(0, vid);
					vertexCos[vid][1] = nlGetVariable(1, vid);
					vertexCos[vid][2] = nlGetVariable(2, vid);
				}			
			} 
			else {
				sys->has_solution = false;
			}
		} 
		else {
			sys->has_solution = false;
		}
	}
}

static bool isValidVertexGroup(LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm)
{
	int defgrp_index;
	MDeformVert *dvert = NULL;

	modifier_get_vgroup(ob, dm, smd->anchor_grp_name, &dvert, &defgrp_index);

	return  (dvert != NULL);
}

static void initSystem(LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm,
                       float (*vertexCos)[3], int numVerts)
{
	int i;
	int defgrp_index;
	int total_anchors;
	float wpaint;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	LaplacianSystem *sys;
	if (isValidVertexGroup(smd, ob, dm)) {
		int *index_anchors = MEM_mallocN(sizeof(int) * numVerts, __func__);  /* over-alloc */
		MFace *tessface;
		STACK_DECLARE(index_anchors);
		STACK_INIT(index_anchors);

		modifier_get_vgroup(ob, dm, smd->anchor_grp_name, &dvert, &defgrp_index);
		BLI_assert(dvert != NULL);
		dv = dvert;
		for (i = 0; i < numVerts; i++) {
			wpaint = defvert_find_weight(dv, defgrp_index);
			dv++;
			if (wpaint > 0.0f) {
				STACK_PUSH(index_anchors, i);
			}
		}
		DM_ensure_tessface(dm);
		total_anchors = STACK_SIZE(index_anchors);
		smd->cacheSystem = initLaplacianSystem(numVerts, dm->getNumEdges(dm), dm->getNumTessFaces(dm),
		                                       total_anchors, smd->anchor_grp_name, smd->repeat);
		sys = (LaplacianSystem *)smd->cacheSystem;
		memcpy(sys->index_anchors, index_anchors, sizeof(int) * total_anchors);
		memcpy(sys->co, vertexCos, sizeof(float[3]) * numVerts);
		MEM_freeN(index_anchors);
		STACK_FREE(index_anchors);
		smd->vertexco = MEM_mallocN(sizeof(float[3]) * numVerts, "ModDeformCoordinates");
		memcpy(smd->vertexco, vertexCos, sizeof(float[3]) * numVerts);
		smd->total_verts = numVerts;

		createFaceRingMap(
		            dm->getNumVerts(dm), dm->getTessFaceArray(dm), dm->getNumTessFaces(dm),
		            &sys->ringf_map, &sys->ringf_indices);
		createVertRingMap(
		            dm->getNumVerts(dm), dm->getEdgeArray(dm), dm->getNumEdges(dm),
		            &sys->ringv_map, &sys->ringv_indices);


		tessface = dm->getTessFaceArray(dm);

		for (i = 0; i < sys->total_faces; i++) {
			memcpy(&sys->faces[i], &tessface[i].v1, sizeof(*sys->faces));
		}
	}
}

static int isSystemDifferent(LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm, int numVerts)
{
	int i;
	int defgrp_index;
	int total_anchors = 0;
	float wpaint;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	LaplacianSystem *sys = (LaplacianSystem *)smd->cacheSystem;

	if (sys->total_verts != numVerts) {
		return LAPDEFORM_SYSTEM_IS_DIFFERENT;
	}
	if (sys->total_edges != dm->getNumEdges(dm)) {
		return LAPDEFORM_SYSTEM_IS_DIFFERENT;
	}
	if (!STREQ(smd->anchor_grp_name, sys->anchor_grp_name)) {
		return LAPDEFORM_SYSTEM_ONLY_CHANGE_GROUP; 
	}
	modifier_get_vgroup(ob, dm, smd->anchor_grp_name, &dvert, &defgrp_index);
	if (!dvert) {
		return LAPDEFORM_SYSTEM_IS_DIFFERENT;
	}
	dv = dvert;
	for (i = 0; i < numVerts; i++) {
		wpaint = defvert_find_weight(dv, defgrp_index);
		dv++;
		if (wpaint > 0.0f) {
			total_anchors++;
		}
	}
	if (sys->total_anchors != total_anchors) {
		return LAPDEFORM_SYSTEM_ONLY_CHANGE_ANCHORS;
	}

	return LAPDEFORM_SYSTEM_NOT_CHANGE;
}

static void LaplacianDeformModifier_do(
        LaplacianDeformModifierData *smd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	float (*filevertexCos)[3];
	int sysdif;
	LaplacianSystem *sys = NULL;
	filevertexCos = NULL;
	if (smd->cacheSystem) {
		sysdif = isSystemDifferent(smd, ob, dm, numVerts);
		sys = smd->cacheSystem;
		if (sysdif) {
			if (sysdif == LAPDEFORM_SYSTEM_ONLY_CHANGE_ANCHORS || sysdif == LAPDEFORM_SYSTEM_ONLY_CHANGE_GROUP) {
				filevertexCos = MEM_mallocN(sizeof(float[3]) * numVerts, "TempModDeformCoordinates");
				memcpy(filevertexCos, smd->vertexco, sizeof(float[3]) * numVerts);
				MEM_SAFE_FREE(smd->vertexco);
				smd->total_verts = 0;
				deleteLaplacianSystem(sys);
				initSystem(smd, ob, dm, filevertexCos, numVerts);
				sys = smd->cacheSystem; /* may have been reallocated */
				MEM_SAFE_FREE(filevertexCos);
				laplacianDeformPreview(sys, vertexCos);
			}
			else {
				deleteLaplacianSystem(sys);
				MEM_SAFE_FREE(smd->vertexco);
				smd->total_verts = 0;
				initSystem(smd, ob, dm, vertexCos, numVerts);
				sys = smd->cacheSystem; /* may have been reallocated */
				laplacianDeformPreview(sys, vertexCos);
			}
		} 
		else {
			sys->repeat = smd->repeat;
			laplacianDeformPreview(sys, vertexCos);
		}
	}
	else {
		if (smd->total_verts > 0 && smd->total_verts == numVerts) {
			if (isValidVertexGroup(smd, ob, dm)) {
				filevertexCos = MEM_mallocN(sizeof(float[3]) * numVerts, "TempDeformCoordinates");
				memcpy(filevertexCos, smd->vertexco, sizeof(float[3]) * numVerts);
				MEM_SAFE_FREE(smd->vertexco);
				smd->total_verts = 0;
				initSystem(smd, ob, dm, filevertexCos, numVerts);
				sys = smd->cacheSystem;
				MEM_SAFE_FREE(filevertexCos);
				laplacianDeformPreview(sys, vertexCos);
			}
		} 
		else {
			if (isValidVertexGroup(smd, ob, dm)) {
				initSystem(smd, ob, dm, vertexCos, numVerts);
				sys = smd->cacheSystem;
				laplacianDeformPreview(sys, vertexCos);
			}
		}
	}
}


static void initData(ModifierData *md)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	smd->anchor_grp_name[0] = '\0';
	smd->total_verts = 0;
	smd->repeat = 1;
	smd->vertexco = NULL;
	smd->cacheSystem = NULL;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	LaplacianDeformModifierData *tsmd = (LaplacianDeformModifierData *) target;
	tsmd->total_verts = smd->total_verts;
	tsmd->repeat = smd->repeat;
	BLI_strncpy(tsmd->anchor_grp_name, smd->anchor_grp_name, sizeof(tsmd->anchor_grp_name));
	tsmd->vertexco = MEM_dupallocN(smd->vertexco);
	tsmd->cacheSystem = MEM_dupallocN(smd->cacheSystem);
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *)md;
	if (smd->anchor_grp_name[0]) return 0;
	return 1;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *)md;
	CustomDataMask dataMask = 0;
	if (smd->anchor_grp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;
	return dataMask;
}

static void deformVerts(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                        float (*vertexCos)[3], int numVerts, ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = get_dm(ob, NULL, derivedData, NULL, false, false);

	LaplacianDeformModifier_do((LaplacianDeformModifierData *) md, ob, dm,
	                  vertexCos, numVerts);
	if (dm != derivedData){
		dm->release(dm);
	}
}
 
static void deformVertsEM(
        ModifierData *md, Object *ob, struct BMEditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = get_dm(ob, editData, derivedData, NULL, false, false);
	LaplacianDeformModifier_do((LaplacianDeformModifierData *) md, ob, dm,
	                  vertexCos, numVerts);
	if (dm != derivedData) {
		dm->release(dm);
	}
}

static void freeData(ModifierData *md)
{
	LaplacianDeformModifierData *smd = (LaplacianDeformModifierData *) md;
	LaplacianSystem *sys = (LaplacianSystem *)(smd->cacheSystem);

	if (sys) {
		deleteLaplacianSystem(sys);
	}
	MEM_SAFE_FREE(smd->vertexco);
	smd->total_verts = 0;
}

ModifierTypeInfo modifierType_LaplacianDeform = {
	/* name */              "LaplacianDeform",
	/* structName */        "LaplacianDeformModifierData",
	/* structSize */        sizeof(LaplacianDeformModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
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
