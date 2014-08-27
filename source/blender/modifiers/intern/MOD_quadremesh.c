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
 * Author: Alexander Pinzon Fernandez
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_quadremesh.c
 *  \ingroup modifiers
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_rand.h"

#include "MEM_guardedalloc.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"

#include "MOD_util.h"
#include "MOD_quadremesh_geom.h"


#ifdef WITH_OPENNL


static LaplacianSystem *newLaplacianSystem(void)
{
	LaplacianSystem *sys;
	sys = MEM_callocN(sizeof(LaplacianSystem), "QuadRemeshCache");
	sys->command_compute_flow = false;
	sys->has_solution = false;
	sys->total_verts = 0;
	sys->total_edges = 0;
	sys->total_features = 0;
	sys->total_faces = 0;
	sys->total_gflines = 0;
	sys->total_gfverts = 0;
	sys->features_grp_name[0] = '\0';

	return sys;
}

static LaplacianSystem *initLaplacianSystem(int totalVerts, int totalEdges, int totalFaces, int totalFeatures,
                                            const char defgrpName[64])
{
	LaplacianSystem *sys = newLaplacianSystem();

	sys->command_compute_flow = false;
	sys->has_solution = false;
	sys->total_verts = totalVerts;
	sys->total_edges = totalEdges;
	sys->total_faces = totalFaces;
	sys->total_features = totalFeatures;
	BLI_strncpy(sys->features_grp_name, defgrpName, sizeof(sys->features_grp_name));
	sys->faces = MEM_mallocN(sizeof(int[4]) * totalFaces, "QuadRemeshFaces");
	sys->edges = MEM_mallocN(sizeof(int[2]) * totalEdges, "QuadRemeshEdges");
	sys->faces_edge = MEM_mallocN(sizeof(int[2]) * totalEdges, "QuadRemeshFacesEdge");
	sys->co = MEM_mallocN(sizeof(float[3]) * totalVerts, "QuadRemeshCoordinates");
	sys->no = MEM_callocN(sizeof(float[3]) * totalFaces, "QuadRemeshNormals");
	sys->gf1 = MEM_mallocN(sizeof(float[3]) * totalFaces, "QuadRemeshGradientField1");
	sys->gf2 = MEM_mallocN(sizeof(float[3]) * totalFaces, "QuadRemeshGradientField2");
	sys->constraints = MEM_mallocN(sizeof(int) * totalVerts, "QuadRemeshConstraints");
	sys->weights = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshWeights");
	sys->U_field = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshUField");
	sys->h1 = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshH1");
	sys->h2 = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshH2");
	sys->gfsys = NULL;
	return sys;
}

static void deleteLaplacianSystem(LaplacianSystem *sys)
{
	MEM_SAFE_FREE(sys->faces);
	MEM_SAFE_FREE(sys->edges);
	MEM_SAFE_FREE(sys->faces_edge);
	MEM_SAFE_FREE(sys->co);
	MEM_SAFE_FREE(sys->cogfl);
	MEM_SAFE_FREE(sys->no);
	MEM_SAFE_FREE(sys->constraints);
	MEM_SAFE_FREE(sys->weights);
	MEM_SAFE_FREE(sys->U_field);
	MEM_SAFE_FREE(sys->h1);
	MEM_SAFE_FREE(sys->h2);
	MEM_SAFE_FREE(sys->gf1);
	MEM_SAFE_FREE(sys->gf2);
	MEM_SAFE_FREE(sys->ringf_indices);
	MEM_SAFE_FREE(sys->ringv_indices);
	MEM_SAFE_FREE(sys->ringe_indices);
	MEM_SAFE_FREE(sys->ringf_map);
	MEM_SAFE_FREE(sys->ringv_map);
	MEM_SAFE_FREE(sys->ringe_map);
	if (sys->context) {
		nlDeleteContext(sys->context);
	}
	MEM_SAFE_FREE(sys);
}

static void createFaceRingMap(
	const int mvert_tot, const MFace *mface, const int mface_tot,
	MeshElemMap **r_map, int **r_indices)
{
	int i, j, totalr = 0;
	int *indices, *index_iter;
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap)* mvert_tot, "DeformRingMap");
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
	indices = MEM_callocN(sizeof(int)* totalr, "DeformRingIndex");
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
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap)* mvert_tot, "DeformNeighborsMap");
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
	indices = MEM_callocN(sizeof(int)* totalr, "DeformNeighborsIndex");
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

static void createEdgeRingMap(
	const int mvert_tot, const MEdge *medge, const int medge_tot,
	MeshElemMap **r_map, int **r_indices)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap)* mvert_tot, "DeformNeighborsMap");
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
	indices = MEM_callocN(sizeof(int)* totalr, "DeformNeighborsIndex");
	index_iter = indices;
	for (i = 0; i < mvert_tot; i++) {
		map[i].indices = index_iter;
		index_iter += map[i].count;
		map[i].count = 0;
	}
	for (i = 0, me = medge; i < medge_tot; i++, me++) {
		vid[0] = me->v1;
		vid[1] = me->v2;
		map[vid[0]].indices[map[vid[0]].count] = i;
		map[vid[0]].count++;
		map[vid[1]].indices[map[vid[1]].count] = i;
		map[vid[1]].count++;
	}
	*r_map = map;
	*r_indices = indices;
}

static void computeFacesAdjacentToEdge(int fs[2], LaplacianSystem *sys, int indexe)
{
	int i, v1, v2, counter;
	int *fidn, numf;
	int *vin;
	v1 = sys->edges[indexe][0];
	v2 = sys->edges[indexe][1];
	numf = sys->ringf_map[v1].count;
	fidn = sys->ringf_map[v1].indices;
	counter = 0;
	fs[0] = -1;
	fs[1] = -1;

	for (i = 0; i < numf && counter < 2; i++) {
		vin = sys->faces[fidn[i]];
		if (vin[0] == v2 || vin[1] == v2 || vin[2] == v2) {
			fs[counter++] = fidn[i];
		}
	}
}

static void createFacesByEdge(LaplacianSystem *sys){
	int ei;
	for (ei = 0; ei < sys->total_edges; ei++) {
		computeFacesAdjacentToEdge(sys->faces_edge[ei], sys, ei);
	}
}

/*
* Compute the normal curvature
* k = dot(2*no, (pi - pj)) / (|pi - pj|)^2
* no = normal on vertex pi
* pi - pj is a vector direction on this case the gradient field direction
* the gradient field direction on some vertex is computed how the average of the faces around vertex
*/
static void computeSampleDistanceFunctions(LaplacianSystem *sys, float user_h, float user_alpha) {
	int i, j, *fin, lin;
	float avg1[3], avg2[3], no[3], k1, k2, h1, h2;
	for (i = 0; i < sys->total_verts; i++) {
		zero_v3(avg1);
		zero_v3(avg2);
		fin = sys->ringf_map[i].indices;
		lin = sys->ringf_map[i].count;
		for (j = 0; j < lin; j++) {
			add_v3_v3(avg1, sys->gf1[j]);
			add_v3_v3(avg2, sys->gf2[j]);
		}
		mul_v3_fl(avg1, 1.0f / ((float)lin));
		mul_v3_fl(avg2, 1.0f / ((float)lin));

		copy_v3_v3(no, sys->no[i]);
		mul_v3_fl(no, 2.0f);
		k1 = dot_v3v3(no, avg1) / dot_v3v3(avg1, avg1);
		k2 = dot_v3v3(no, avg2) / dot_v3v3(avg2, avg2);
		
		h1 = user_h / (1.0f + user_alpha * (logf(1.0f + k1*k1)));
		h2 = user_h / (1.0f + user_alpha * (logf(1.0f + k2*k2)));

		sys->h1[i] = h1;
		sys->h2[i] = h2;
	}
}

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
			i = 4;
		}
		else {
			normal_tri_v3(no, sys->co[idv1], sys->co[idv2], sys->co[idv3]);
			i = 3;
		}
		copy_v3_v3(sys->no[fi], no);

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

				w2 = (cotangent_tri_weight_v3(v4, v1, v2) + cotangent_tri_weight_v3(v3, v1, v2)) / 2.0f;
				w3 = (cotangent_tri_weight_v3(v2, v3, v1) + cotangent_tri_weight_v3(v4, v1, v3)) / 2.0f;
				w4 = (cotangent_tri_weight_v3(v2, v4, v1) + cotangent_tri_weight_v3(v3, v4, v1)) / 2.0f;

				if (sys->constraints[idv1] == 0) {
					nlMatrixAdd(idv1, idv4, -w4);
				}
			}
			else {
				w2 = cotangent_tri_weight_v3(v3, v1, v2);
				w3 = cotangent_tri_weight_v3(v2, v3, v1);
				w4 = 0.0f;
			}

			if (sys->constraints[idv1] == 1) {
				nlMatrixAdd(idv1, idv1, w2 + w3 + w4);
			}
			else  {
				nlMatrixAdd(idv1, idv2, -w2);
				nlMatrixAdd(idv1, idv3, -w3);
				nlMatrixAdd(idv1, idv1, w2 + w3 + w4);
			}

		}
	}
	
}

static void computeScalarField(LaplacianSystem *sys)
{
	int vid, i, n;
	n = sys->total_verts;
	printf("computeScalarField 0 \n");

#ifdef OPENNL_THREADING_HACK
	modifier_opennl_lock();
#endif
	printf("computeScalarField 1 \n");
	nlNewContext();
	sys->context = nlGetCurrent();

	printf("computeScalarField 2 \n");

	nlSolverParameteri(NL_NB_VARIABLES, n);
	nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
	nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
	nlSolverParameteri(NL_NB_ROWS, n);
	nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 1);
	nlBegin(NL_SYSTEM);
	printf("computeScalarField 3 \n");
	for (i = 0; i < n; i++) {
		nlSetVariable(0, i, 0);
	}
		
	nlBegin(NL_MATRIX);
	printf("computeScalarField 4 \n");
	initLaplacianMatrix(sys);
	printf("computeScalarField 5 \n");

	for (i = 0; i < n; i++) {
		if (sys->constraints[i] == 1) {
			nlRightHandSideSet(0, i, sys->weights[i]);
		}
		else {
			nlRightHandSideSet(0, i, 0);
		}
	}
	nlEnd(NL_MATRIX);
	nlEnd(NL_SYSTEM);
	printf("computeScalarField 6 \n");
	if (nlSolveAdvanced(NULL, NL_TRUE)) {
		printf("computeScalarField 7 \n");
		sys->has_solution = true;

		for (vid = 0; vid < sys->total_verts; vid++) {
			sys->U_field[vid] = nlGetVariable(0, vid);
		}	
	}
	else {
		sys->has_solution = false;
	}
#ifdef OPENNL_THREADING_HACK
	modifier_opennl_unlock();
#endif
}

/**
* Compute the gradient fields
*
* xi, xj, xk, are the vertices of the face
* ui, uj, uk, are the values of scalar fields for every vertex of the face
* n is the normal of the face.
* gf1 is the unknown field gradient 1.
* gf2 is the unknown field gradient 2.
*
* |xj - xi|         |uj - ui|
* |xk - xj| * gf1 = |uk - uj|
* |   nf  |         |   0   |
*
* gf2 = cross(n, gf1)
*/
static void computeGradientFields(LaplacianSystem * sys)
{
	int fi, i, j, k;
	float val, a[3][3], u[3], inv_a[3][3], gf1[3], g[3], w[3];

	for (fi = 0; fi < sys->total_faces; fi++) {
		const unsigned int *vidf = sys->faces[fi];
		i = vidf[0];
		j = vidf[1];
		k = vidf[2];
		sub_v3_v3v3(a[0], sys->co[j], sys->co[i]);
		sub_v3_v3v3(a[1], sys->co[k], sys->co[j]);
		copy_v3_v3(a[2], sys->no[fi]);

		/* Correct way*/
		transpose_m3(a);
		u[0] = sys->U_field[j] - sys->U_field[i];
		u[1] = sys->U_field[k] - sys->U_field[j];
		u[2] = 0;
		invert_m3_m3(inv_a, a);
		//mul_v3_m3v3(sys->gf1[fi], inv_a, u);
		mul_v3_m3v3(gf1, inv_a, u);

		/* Project Gradient fields on face*/
		normalize_v3_v3(g, gf1);
		val = dot_v3v3(g, sys->no[fi]);
		mul_v3_v3fl(u, sys->no[fi], val);
		sub_v3_v3v3(w, g, u);
		normalize_v3_v3(sys->gf1[fi], w);
		

		cross_v3_v3v3(g, sys->no[fi], sys->gf1[fi]);
		normalize_v3_v3(sys->gf2[fi], g);
		//cross_v3_v3v3(sys->gf2[fi], sys->no[fi], sys->gf1[fi]);
	}
}

/**
* Random point, P, uniformly from within triangle ABC, method given by 
* Robert Osada, Thomas Funkhouser, Bernard Chazelle, and David Dobkin. 2002. Shape distributions. ACM Trans. Graph. 21,
* 4 (October 2002), 807-832. DOI=10.1145/571647.571648 http://doi.acm.org/10.1145/571647.571648
* a,b,c are the triangle points
* r1, r2, are the randon numbers betwen [0, 1]
* P = (1 − sqrt(r1)) A + sqrt(r1)(1 − r2) B + sqrt(r1) * r2 C
*/
static void uniformRandomPointWithinTriangle(float r[3], float a[3], float b[3], float c[3])
{
	float va, vb, vc;
	float pa[3], pb[3], pc[3];
	float r1, r2;
	r1 = BLI_frand();
	r2 = BLI_frand();
	va = 1.0f - sqrtf(r1);
	vb = sqrtf(r1) * ( 1.0f - r2);
	vc = sqrtf(r1) * r2;
	mul_v3_v3fl(pa, a, va);
	mul_v3_v3fl(pb, b, vb);
	mul_v3_v3fl(pc, c, vc);
}

static void uniformRandomPointWithinFace(float r[3], LaplacianSystem *sys, int indexf){
	int *vin;
	vin = sys->faces[indexf];
	uniformRandomPointWithinTriangle(r, sys->co[vin[0]], sys->co[vin[1]], sys->co[vin[2]]);
}

static LaplacianSystem * initSystem(QuadRemeshModifierData *qmd, Object *ob, DerivedMesh *dm,
	float(*vertexCos)[3], int numVerts)
{
	int i, j;
	int defgrp_index;
	int total_features;
	float wpaint;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	LaplacianSystem *sys = NULL;


	int *constraints = MEM_mallocN(sizeof(int)* numVerts, __func__);
	float *weights = MEM_mallocN(sizeof(float)* numVerts, __func__);
	MFace *tessface;
	MEdge *arrayedge;

	printf("initSystem 0\n");
	modifier_get_vgroup(ob, dm, qmd->anchor_grp_name, &dvert, &defgrp_index);
	printf("initSystem 1\n");
	BLI_assert(dvert != NULL);
	printf("initSystem 2\n");
	dv = dvert;
	j = 0;
	for (i = 0; i < numVerts; i++) {
		wpaint = defvert_find_weight(dv, defgrp_index);
		dv++;

		if (wpaint < 0.19 || wpaint > 0.89) {
			constraints[i] = 1;
			weights[i] = -1.0f + wpaint * 2.0f;
			j++;
		}
		else {
			constraints[i] = 0;
		}
	}
	printf("initSystem 3\n");
	total_features = j;
	DM_ensure_tessface(dm);
	sys = initLaplacianSystem(numVerts, dm->getNumEdges(dm), dm->getNumTessFaces(dm), total_features, qmd->anchor_grp_name);
	printf("initSystem 4\n");

	memcpy(sys->co, vertexCos, sizeof(float[3]) * numVerts);
	memcpy(sys->constraints, constraints, sizeof(int)* numVerts);
	memcpy(sys->weights, weights, sizeof(float)* numVerts);
	MEM_freeN(weights);
	MEM_freeN(constraints);
	printf("initSystem 5\n");
	createFaceRingMap(
		dm->getNumVerts(dm), dm->getTessFaceArray(dm), dm->getNumTessFaces(dm),
		&sys->ringf_map, &sys->ringf_indices);
	createVertRingMap(
		dm->getNumVerts(dm), dm->getEdgeArray(dm), dm->getNumEdges(dm),
		&sys->ringv_map, &sys->ringv_indices);
	createEdgeRingMap(
		dm->getNumVerts(dm), dm->getEdgeArray(dm), dm->getNumEdges(dm),
		&sys->ringe_map, &sys->ringe_indices);

	tessface = dm->getTessFaceArray(dm);

	for (i = 0; i < sys->total_faces; i++) {
		memcpy(&sys->faces[i], &tessface[i].v1, sizeof(*sys->faces));
	}

	arrayedge = dm->getEdgeArray(dm);
	for (i = 0; i < sys->total_edges; i++) {
		memcpy(&sys->edges[i], &arrayedge[i].v1, sizeof(*sys->edges));
	}
	createFacesByEdge(sys);

	computeSampleDistanceFunctions(sys, 2.0, 10.0f);
	printf("initSystem 6\n");

	return sys;

}

static GradientFlowSystem *QuadRemeshModifier_do(
	QuadRemeshModifierData *qmd, Object *ob, DerivedMesh *dm,
	float(*vertexCos)[3], int numVerts)
{
	int i;
	LaplacianSystem *sys = NULL;
	
	int defgrp_index;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	float mmin = 1000, mmax = 0;
	float y;
	int x;
	GradientFlowSystem *gfsys = NULL;

	if (qmd->flag & MOD_QUADREMESH_COMPUTE_FLOW) {
		if (strlen(qmd->anchor_grp_name) >= 1) {
			printf("QuadRemeshModifier_do 0.1 \n");
			if (qmd->cache_system) {
				sys = qmd->cache_system;
				deleteLaplacianSystem(sys);
			}
			qmd->cache_system = initSystem(qmd, ob, dm, vertexCos, numVerts);
			sys = qmd->cache_system;
			computeScalarField(sys);
			if (sys->has_solution) {
				computeGradientFields(sys);
				printf("QuadRemeshModifier_do 0 \n");
				if (!defgroup_find_name(ob, "QuadRemeshFlow")) {
					printf("QuadRemeshModifier_do 1 \n");
					BKE_defgroup_new(ob, "QuadRemeshFlow");
					modifier_get_vgroup(ob, dm, "QuadRemeshFlow", &dvert, &defgrp_index);
					BLI_assert(dvert != NULL);
					dv = dvert;
					for (i = 0; i < numVerts; i++) {
						mmin = min_ff(mmin, sys->U_field[i]);
						mmax = max_ff(mmax, sys->U_field[i]);
					}

					for (i = 0; i < numVerts; i++) {
						y = (sys->U_field[i] - mmin) / (mmax - mmin);
						x = y * 60;
						y = (x % 2 == 0 ? 0.1 : 0.9);
						defvert_add_index_notest(dv, defgrp_index, y);
						dv++;
					}
				}
			}
		}
		printf("QuadRemeshModifier_do 2 \n");
		qmd->flag &= ~MOD_QUADREMESH_COMPUTE_FLOW;
	}

	if (qmd->flag & MOD_QUADREMESH_REMESH && qmd->cache_system) {
		sys = qmd->cache_system;
		if (sys->has_solution) {
			sys->h = 2.0f;
			computeFlowLines(sys);
			gfsys = sys->gfsys;
		}
		qmd->flag &= ~MOD_QUADREMESH_REMESH;
	}

	if (qmd->cache_system) {
		sys = qmd->cache_system;
		if (sys->has_solution) {
			if (sys->gfsys) {
				gfsys = sys->gfsys;
			}
		}
	}


	return gfsys;
}


#else  /* WITH_OPENNL */
static void QuadRemeshModifier_do(
        QuadRemeshModifierData *lmd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	(void)lmd, (void)ob, (void)dm, (void)vertexCos, (void)numVerts;
}
#endif  /* WITH_OPENNL */

static void initData(ModifierData *md)
{
	QuadRemeshModifierData *lmd = (QuadRemeshModifierData *)md;
	lmd->anchor_grp_name[0] = '\0';
	lmd->flag = 0;
	lmd->cache_system = NULL;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	QuadRemeshModifierData *qmd = (QuadRemeshModifierData *)md;
	QuadRemeshModifierData *tqmd = (QuadRemeshModifierData *)target;
	modifier_copyData_generic(md, target);
	tqmd->cache_system = NULL;

}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	QuadRemeshModifierData *lmd = (QuadRemeshModifierData *)md;
	//if (lmd->anchor_grp_name[0]) return 0;
	//return 1;
	return 0;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	QuadRemeshModifierData *lmd = (QuadRemeshModifierData *)md;
	CustomDataMask dataMask = 0;
	if (lmd->anchor_grp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;
	return dataMask;
}

static void deformVerts(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                        float (*vertexCos)[3], int numVerts, ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = get_dm(ob, NULL, derivedData, NULL, false, false);

	QuadRemeshModifier_do((QuadRemeshModifierData *)md, ob, dm, vertexCos, numVerts, NULL);
	if (dm != derivedData) {
		dm->release(dm);
	}
}

static DerivedMesh *applyModifier(ModifierData *md,
	Object *ob,
	DerivedMesh *dm,
	ModifierApplyFlag UNUSED(flag))
{
	//DerivedMesh *dm2 = get_dm(ob, NULL, dm, NULL, false, false);
	//QuadRemeshModifier_do((QuadRemeshModifierData *)md, ob, dm, (void *)dm->getVertArray(dm), dm->getNumVerts(dm));

	float (*myco)[3];	
	MVert *arrayvect;
	MEdge *arrayedge;
	int i;
	float(*vertexCos)[3];
	GradientFlowSystem *gfsys = NULL;
	DerivedMesh *result;
	
	vertexCos = MEM_mallocN(sizeof(float[3]) * dm->getNumVerts(dm), __func__);
	
	arrayvect = dm->getVertArray(dm);
	for (i = 0; i < dm->getNumVerts(dm); i++) {
		copy_v3_v3(vertexCos[i], arrayvect[i].co);
	}
	if (!gfsys) {
		gfsys = QuadRemeshModifier_do((QuadRemeshModifierData *)md, ob, dm, vertexCos, dm->getNumVerts(dm), gfsys);
	}
	MEM_SAFE_FREE(vertexCos);
	
	if (gfsys) {
		result = CDDM_new(gfsys->mesh->totvert, gfsys->mesh->totedge, 0, gfsys->mesh->totedge, 0);
		arrayvect = result->getVertArray(result);
		for (i = 0; i < gfsys->mesh->totvert; i++) {
			copy_v3_v3(arrayvect[i].co, gfsys->mesh->mvert[i].co);
		}
		arrayedge = result->getEdgeArray(result);
		for (i = 0; i < gfsys->mesh->totedge; i++) {
			arrayedge[i].v1 = gfsys->mesh->medge[i].v1;
			arrayedge[i].v2 = gfsys->mesh->medge[i].v2;
		}
		dm = result;
	}
	else{
		result = dm;
	}
	

	
	//CDDM_calc_edges_tessface(result);

	return result;
}

static void freeData(ModifierData *md)
{
#ifdef WITH_OPENNL
	QuadRemeshModifierData *qmd = (QuadRemeshModifierData *)md;
	LaplacianSystem *sys = (LaplacianSystem *)qmd->cache_system;
	if (sys) {
		deleteLaplacianSystem(sys);
	}
#endif
}

ModifierTypeInfo modifierType_QuadRemesh = {
	/* name */              "QuadRemesh",
	/* structName */        "QuadRemeshModifierData",
	/* structSize */        sizeof(QuadRemeshModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
							eModifierTypeFlag_AcceptsCVs |
							eModifierTypeFlag_SupportsEditmode,
	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
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
