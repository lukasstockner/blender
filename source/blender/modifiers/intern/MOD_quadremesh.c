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

#include "MEM_guardedalloc.h"

#include "BKE_mesh_mapping.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"

#include "MOD_util.h"


#ifdef WITH_OPENNL

#include "ONL_opennl.h"

typedef struct GradientFlowLine {
	float(*co)[3];			/* Vertex coordinate */
	int *index;				/* Pointer to a edge */
	int total_verts;		/* Total number of points in a flow line */
	int total_allocated;	/* Total number of elements allocated */
} GradientFlowLine;

typedef struct LaplacianSystem {
	bool is_matrix_computed;
	bool has_solution;
	int total_verts;
	int total_edges;
	int total_faces;
	int total_features;
	int total_gflines;
	char features_grp_name[64];	/* Vertex Group name */
	float(*co)[3];				/* Original vertex coordinates */
	float(*no)[3];				/* Original face normal */
	float(*gf1)[3];				/* Gradient Field g1 */
	float(*gf2)[3];				/* Gradient Field g2 */
	float *weights;				/* Feature points weights*/
	float *U_field;				/* Initial scalar field*/
	int *constraints;			/* Feature points constraints*/
	int *ringf_indices;			/* Indices of faces per vertex */
	int *ringv_indices;			/* Indices of neighbors(vertex) per vertex */
	int *ringe_indices;			/* Indices of edges per vertex */
	unsigned int(*faces)[4];	/* Copy of MFace (tessface) v1-v4 */
	unsigned int(*edges)[2];	/* Copy of edges v1-v2 */
	GradientFlowLine *gflines;  /* Gradien flow lines of field g1*/
	MeshElemMap *ringf_map;		/* Map of faces per vertex */
	MeshElemMap *ringv_map;		/* Map of vertex per vertex */
	MeshElemMap *ringe_map;		/* Map of edges per vertex */
	NLContext *context;			/* System for solve general implicit rotations */
} LaplacianSystem;

static GradientFlowLine *initGradientFlowLine(GradientFlowLine *gfl, int expected_size){
	gfl->co = MEM_mallocN(sizeof(float[3]) * expected_size, __func__);  /* over-alloc */
	gfl->index = MEM_mallocN(sizeof(int)* expected_size, __func__);  /* over-alloc */
	gfl->total_allocated = expected_size;
	gfl->total_verts = 0;
	return gfl;
}

static void addPointToGradientFlowLine(GradientFlowLine *gfl, float p[3], int index)
{
	if (index >= 0 ) {

		if (index >= gfl->total_allocated){
			gfl->co = MEM_reallocN(gfl->co, sizeof(float[3]) * (gfl->total_allocated + 1));
			gfl->index = MEM_reallocN(gfl->index, sizeof(int) * (gfl->total_allocated + 1));
			gfl->total_allocated++;
		}

		copy_v3_v3(gfl->co[gfl->total_verts], p);
		gfl->index[gfl->total_verts] = index;
		gfl->total_verts++;
	}
}

static LaplacianSystem *newLaplacianSystem(void)
{
	LaplacianSystem *sys;
	sys = MEM_callocN(sizeof(LaplacianSystem), "QuadRemeshCache");
	sys->is_matrix_computed = false;
	sys->has_solution = false;
	sys->total_verts = 0;
	sys->total_edges = 0;
	sys->total_features = 0;
	sys->total_faces = 0;
	sys->total_gflines = 0;
	sys->features_grp_name[0] = '\0';

	return sys;
}

static LaplacianSystem *initLaplacianSystem(int totalVerts, int totalEdges, int totalFaces, int totalFeatures,
                                            const char defgrpName[64])
{
	LaplacianSystem *sys = newLaplacianSystem();

	sys->is_matrix_computed = false;
	sys->has_solution = false;
	sys->total_verts = totalVerts;
	sys->total_edges = totalEdges;
	sys->total_faces = totalFaces;
	sys->total_features = totalFeatures;
	BLI_strncpy(sys->features_grp_name, defgrpName, sizeof(sys->features_grp_name));
	sys->faces = MEM_mallocN(sizeof(int[4]) * totalFaces, "QuadRemeshFaces");
	sys->edges = MEM_mallocN(sizeof(int[2]) * totalEdges, "QuadRemeshEdges");
	sys->co = MEM_mallocN(sizeof(float[3]) * totalVerts, "QuadRemeshCoordinates");
	sys->no = MEM_callocN(sizeof(float[3]) * totalFaces, "QuadRemeshNormals");
	sys->gf1 = MEM_mallocN(sizeof(float[3]) * totalFaces, "QuadRemeshGradientField1");
	sys->gf2 = MEM_mallocN(sizeof(float[3]) * totalFaces, "QuadRemeshGradientField2");
	sys->constraints = MEM_mallocN(sizeof(int) * totalVerts, "QuadRemeshConstraints");
	sys->weights = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshWeights");
	sys->U_field = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshUField");
	return sys;
}

static void UNUSED_FUNCTION(deleteLaplacianSystem)(LaplacianSystem *sys)
{
	MEM_SAFE_FREE(sys->faces);
	MEM_SAFE_FREE(sys->edges);
	MEM_SAFE_FREE(sys->co);
	MEM_SAFE_FREE(sys->no);
	MEM_SAFE_FREE(sys->constraints);
	MEM_SAFE_FREE(sys->weights);
	MEM_SAFE_FREE(sys->U_field);
	MEM_SAFE_FREE(sys->gf1);
	MEM_SAFE_FREE(sys->gf2);
	MEM_SAFE_FREE(sys->ringf_indices);
	MEM_SAFE_FREE(sys->ringv_indices);
	MEM_SAFE_FREE(sys->ringe_indices);
	MEM_SAFE_FREE(sys->ringf_map);
	MEM_SAFE_FREE(sys->ringv_map);
	MEM_SAFE_FREE(sys->ringe_map);
	for (int i = 0; i < sys->total_gflines; i++) {
		MEM_SAFE_FREE(sys->gflines[i].co);
		MEM_SAFE_FREE(sys->gflines[i].index);
	}
	MEM_SAFE_FREE(sys->gflines);
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

#ifdef OPENNL_THREADING_HACK
	modifier_opennl_lock();
#endif
	if (!sys->is_matrix_computed) {
		nlNewContext();
		sys->context = nlGetCurrent();

		nlSolverParameteri(NL_NB_VARIABLES, n);
		nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
		nlSolverParameteri(NL_NB_ROWS, n);
		nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 1);
		nlBegin(NL_SYSTEM);
		for (i = 0; i < n; i++) {
			nlSetVariable(0, i, 0);
		}
		
		nlBegin(NL_MATRIX);

		initLaplacianMatrix(sys);

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
		if (nlSolveAdvanced(NULL, NL_TRUE)) {
			sys->has_solution = true;

			for (vid = 0; vid < sys->total_verts; vid++) {
				sys->U_field[vid] = nlGetVariable(0, vid);
			}	
		}
		else {
			sys->has_solution = false;
		}
		sys->is_matrix_computed = true;
	
	}
	

#ifdef OPENNL_THREADING_HACK
	modifier_opennl_unlock();
#endif
}

static void print_face(LaplacianSystem *sys, int indexf)
{
	int *vin;
	vin = sys->faces[indexf];
	print_v3_id(sys->co[vin[0]]);
	print_v3_id(sys->co[vin[1]]);
	print_v3_id(sys->co[vin[2]]);

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
	float a[3][3], u[3], inv_a[3][3];

	for (fi = 0; fi < sys->total_faces; fi++) {
		const unsigned int *vidf = sys->faces[fi];
		i = vidf[0];
		j = vidf[1];
		k = vidf[2];
		sub_v3_v3v3(a[0], sys->co[j], sys->co[i]);
		sub_v3_v3v3(a[1], sys->co[k], sys->co[j]);
		copy_v3_v3 (a[2], sys->no[fi]);

		/* Correct way*/
		transpose_m3(a);
		u[0] = sys->U_field[j] - sys->U_field[i];
		u[1] = sys->U_field[k] - sys->U_field[j];
		u[2] = 0;
		invert_m3_m3(inv_a, a);
		mul_v3_m3v3(sys->gf1[fi], inv_a, u);
		cross_v3_v3v3(sys->gf2[fi], sys->no[fi], sys->gf1[fi]);
	}
}

void printff_v3(const char *str, const float v[3])
{
	printf("%s: %.7f %.7f %.7f\n", str, v[0], v[1], v[2]);
}

/**
* Project vector of gradient field on face, 
*/
static void projectGradientOnFace(float dir[3], LaplacianSystem * sys, float(*gf)[3], int indexf)
{
	int i;
	float  g[3], u[3], w[3], val;
	normalize_v3_v3(g, gf[indexf]);
	val = dot_v3v3(g, sys->no[indexf]);
	mul_v3_v3fl(u, sys->no[indexf], val);
	sub_v3_v3v3(w, g, u);
	normalize_v3_v3(dir, w);
}

static void getFacesAdjacentToEdge(int fs[2], LaplacianSystem *sys, int indexe)
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

static int getEdgeFromVerts(LaplacianSystem *sys, int v1, int v2)
{
	int *eidn, nume, i;
	nume = sys->ringe_map[v1].count;
	eidn = sys->ringe_map[v1].indices;
	for (i = 0; i < nume; i++) {
		if (sys->edges[eidn[i]][0] == v2 || sys->edges[eidn[i]][1] == v2){
			return eidn[i];
		}
	}
	return -1;
}

static bool isBetweenLine(float p1[3], float p2[3], float q[3]){
	if (   (q[0] >= min_ff(p1[0], p2[0]))
		&& (q[1] >= min_ff(p1[1], p2[1]))
		&& (q[2] >= min_ff(p1[2], p2[2]))
		&& (q[0] <= max_ff(p1[0], p2[0]))
		&& (q[1] <= max_ff(p1[1], p2[1]))
		&& (q[2] <= max_ff(p1[2], p2[2]))
		) {
		return true;
	}
	return false;
}

static int isectLineToEdges(float r[3], LaplacianSystem *sys, float ori[3], int indexf, int indexe)
{
	int *vin, ev1, ev2, ev3;
	float p1[3], p2[3], p3[3], dir[3], q[3], i1[3], i2[3];
	float l;
	
	ev1 = sys->edges[indexe][0];
	ev2 = sys->edges[indexe][1];

	vin = sys->faces[indexf];
	projectGradientOnFace(dir, sys, sys->gf1, indexf);

	mul_v3_fl(dir, 100);

	add_v3_v3v3(q, ori, dir);

	if ((vin[0] == ev1 && vin[1] == ev2) || (vin[1] == ev1 && vin[0] == ev2)) {
		ev3 = vin[2];
	}
	else if ((vin[0] == ev1 && vin[2] == ev2) || (vin[2] == ev1 && vin[0] == ev2)) {
		ev3 = vin[1];
	}
	else if ((vin[1] == ev1 && vin[2] == ev2) || (vin[2] == ev1 && vin[1] == ev2)) {
		ev3 = vin[0];
	}

	copy_v3_v3(p1, sys->co[ev1]);
	copy_v3_v3(p2, sys->co[ev2]);
	copy_v3_v3(p3, sys->co[ev3]);

	if (isect_line_line_v3(p2, p3, ori, q, r, i2) == ISECT_LINE_LINE_EXACT) {
		return getEdgeFromVerts(sys, ev2, ev3);
	}
	else if (isect_line_line_v3(p1, p3, ori, q, r, i2) == ISECT_LINE_LINE_EXACT) {
		return getEdgeFromVerts(sys, ev1, ev3);
	}
	else if (isect_line_line_v3(p2, p3, ori, q, r, i2) == ISECT_LINE_LINE_CROSS) {
		l = len_v3v3(r, i2);
		if (l < 0.000001f) {
			if (isBetweenLine(p2, p3, r)) {
				return getEdgeFromVerts(sys, ev2, ev3);
			}
			else{
				printf("No between \n");
			}
		}
	}
	else if (isect_line_line_v3(p1, p3, ori, q, r, i2) == ISECT_LINE_LINE_CROSS) {
		l = len_v3v3(r, i2);
		if (l < 0.000001f) {
			if (isBetweenLine(p1, p3, r)) {
				return getEdgeFromVerts(sys, ev1, ev3);
			}
			else {
				printf("No between \n");
			}

		}
	}
	return -1;

	
}

/** 
* int ifs; Index of inde, this edge is the seed for trace this flow line
*/
static void computeGradientFlowLine(LaplacianSystem * sys, int inde, float dis, float(*vertexCos)[3])
{
	int fs[2], indegde, res, oldface, newface;
	float seed[3], v1[3], v2[3], p1[3], dir[3], r[3];
	float(*mive)[3] = MEM_mallocN(sizeof(float) * 600, __func__ );
	int i, total = 0;
	//GradientFlowLine *gfline = MEM_mallocN(sizeof(GradientFlowLine), __func__);



	copy_v3_v3(v1, sys->co[sys->edges[inde][0]]);
	copy_v3_v3(v2, sys->co[sys->edges[inde][1]]);
	
	add_v3_v3v3(seed, v1, v2);
	mul_v3_fl(seed, 0.5f);
	
	getFacesAdjacentToEdge(fs, sys, inde);
	

	indegde = inde;
	oldface = fs[0];
	newface = fs[1];
	while (indegde >= 0) {

		indegde = isectLineToEdges(r, sys, seed, newface, indegde);
		print_v3_id(r);
		
		if (indegde >= 0) {
			copy_v3_v3(mive[total++], r);
			getFacesAdjacentToEdge(fs, sys, indegde);
			if (fs[0] == newface){
				oldface = newface;
				newface = fs[1];
			}
			else{
				oldface = newface;
				newface = fs[0];
			}
		}
		else{
			print_face(sys, newface);
			printff_v3("seed: \n", seed);
			projectGradientOnFace(dir, sys, sys->gf1, newface);
			printff_v3("dir: \n", dir);

		}
		copy_v3_v3(seed, r);
	}

	for (i = 0; i < total; i++) {
		copy_v3_v3(vertexCos[i], mive[i]);
	}

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

	modifier_get_vgroup(ob, dm, qmd->anchor_grp_name, &dvert, &defgrp_index);
	BLI_assert(dvert != NULL);
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

	total_features = j;
	DM_ensure_tessface(dm);
	sys = initLaplacianSystem(numVerts, dm->getNumEdges(dm), dm->getNumTessFaces(dm), total_features, qmd->anchor_grp_name);

	memcpy(sys->co, vertexCos, sizeof(float[3]) * numVerts);
	memcpy(sys->constraints, constraints, sizeof(int)* numVerts);
	memcpy(sys->weights, weights, sizeof(float)* numVerts);
	MEM_freeN(weights);
	MEM_freeN(constraints);

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
	return sys;

}

static void QuadRemeshModifier_do(
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

	if (numVerts == 0) return;
	if (strlen(qmd->anchor_grp_name) < 3) return;
	sys = initSystem(qmd, ob, dm, vertexCos, numVerts);
	computeScalarField(sys);
	computeGradientFields(sys);

	if (!defgroup_find_name(ob, "QuadRemeshGroup")) {
		BKE_defgroup_new(ob, "QuadRemeshGroup");
		modifier_get_vgroup(ob, dm, "QuadRemeshGroup", &dvert, &defgrp_index);
		BLI_assert(dvert != NULL);
		dv = dvert;
		for (i = 0; i < numVerts; i++) {
			mmin = min_ff(mmin, sys->U_field[i]);
			mmax = max_ff(mmax, sys->U_field[i]);
		}

		for (i = 0; i < numVerts; i++) {
			y = (sys->U_field[i] - mmin) / (mmax - mmin);
			x = y * 30;
			y = (x % 2 == 0 ? 0.1 : 0.9);
			defvert_add_index_notest(dv, defgrp_index, y);
			dv++;
		}
		computeGradientFlowLine(sys, 100, 0.5, vertexCos);
	}
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
}

static void copyData(ModifierData *md, ModifierData *target)
{

	modifier_copyData_generic(md, target);

}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	QuadRemeshModifierData *lmd = (QuadRemeshModifierData *)md;
	if (lmd->anchor_grp_name[0]) return 0;
	return 1;
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

	QuadRemeshModifier_do((QuadRemeshModifierData *)md, ob, dm, vertexCos, numVerts);
	if (dm != derivedData) {
		dm->release(dm);
	}
}

static void deformVertsEM(
        ModifierData *md, Object *ob, struct BMEditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = get_dm(ob, editData, derivedData, NULL, false, false);
	QuadRemeshModifier_do((QuadRemeshModifierData *)md, ob, dm,
	                           vertexCos, numVerts);
	if (dm != derivedData) {
		dm->release(dm);
	}
}

static void freeData(ModifierData *UNUSED(md))
{
#ifdef WITH_OPENNL
	/*LaplacianSystem *sys = (LaplacianSystem *)lmd->cache_system;
	if (sys) {
		deleteLaplacianSystem(sys);
	}*/
#endif
	//MEM_SAFE_FREE(lmd->vertexco);
	//lmd->total_verts = 0;
}

ModifierTypeInfo modifierType_QuadRemesh = {
	/* name */              "QuadRemesh",
	/* structName */        "QuadRemeshModifierData",
	/* structSize */        sizeof(QuadRemeshModifierData),
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
