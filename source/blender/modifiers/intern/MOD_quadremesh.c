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

typedef struct LaplacianSystem {
	bool is_matrix_computed;
	bool has_solution;
	int total_verts;
	int total_edges;
	int total_faces;
	int total_features;
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
	unsigned int(*faces)[4];	/* Copy of MFace (tessface) v1-v4 */
	MeshElemMap *ringf_map;		/* Map of faces per vertex */
	MeshElemMap *ringv_map;		/* Map of vertex per vertex */
	NLContext *context;			/* System for solve general implicit rotations */
} LaplacianSystem;

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
	sys->co = MEM_mallocN(sizeof(float[3]) * totalVerts, "QuadRemeshCoordinates");
	sys->no = MEM_callocN(sizeof(float[3]) * totalFaces, "QuadRemeshNormals");
	sys->gf1 = MEM_mallocN(sizeof(float[3]) * totalFaces, "QuadRemeshGradientField1");
	sys->gf2 = MEM_mallocN(sizeof(float[3]) * totalFaces, "QuadRemeshGradientField2");
	sys->constraints = MEM_mallocN(sizeof(int) * totalVerts, "QuadRemeshConstraints");
	sys->weights = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshWeights");
	sys->U_field = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshUField");
	return sys;
}

static void deleteLaplacianSystem(LaplacianSystem *sys)
{
	MEM_SAFE_FREE(sys->faces);
	MEM_SAFE_FREE(sys->co);
	MEM_SAFE_FREE(sys->no);
	MEM_SAFE_FREE(sys->constraints);
	MEM_SAFE_FREE(sys->weights);
	MEM_SAFE_FREE(sys->U_field);
	MEM_SAFE_FREE(sys->gf1);
	MEM_SAFE_FREE(sys->gf2);
	MEM_SAFE_FREE(sys->ringf_indices);
	MEM_SAFE_FREE(sys->ringv_indices);
	MEM_SAFE_FREE(sys->ringf_map);
	MEM_SAFE_FREE(sys->ringv_map);
	if (sys->context) {
		nlDeleteContext(sys->context);
	}
	MEM_SAFE_FREE(sys);
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
	int vid, i, n, na;
	n = sys->total_verts;
	na = sys->total_features;

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

/** 
 * Compute the gradiente fields
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
		u[0] = sys->U_field[j] - sys->U_field[i];
		u[1] = sys->U_field[k] - sys->U_field[j];
		u[2] = 0;
		invert_m3_m3(inv_a, a);
		mul_v3_m3v3(sys->gf1[fi], inv_a, u);
		cross_v3_v3v3(sys->gf2[fi], sys->no[fi], sys->gf1[fi]);
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
	tessface = dm->getTessFaceArray(dm);

	for (i = 0; i < sys->total_faces; i++) {
		memcpy(&sys->faces[i], &tessface[i].v1, sizeof(*sys->faces));
	}
	return sys;

}

static float RGBtoH(float r, float g, float b)
{
	float mmin, mmax, delta, h;
	mmin = min(r,min( g, b));
	mmax = max(r, max( g, b));

	delta = mmax - mmin;
	if (r == mmax)
		h = (g - b) / delta;		// between yellow & magenta
	else if (g == mmax)
		h = 2 + (b - r) / delta;	// between cyan & yellow
	else
		h = 4 + (r - g) / delta;	// between magenta & cyan
	h *= 60;				// degrees
	if (h < 0)
		h += 360;
	return h / 360.0;
}

static void QuadRemeshModifier_do(
	QuadRemeshModifierData *qmd, Object *ob, DerivedMesh *dm,
	float(*vertexCos)[3], int numVerts)
{
	float(*filevertexCos)[3];
	int sysdif, i, fi;
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
			mmin = min(mmin, sys->U_field[i]);
			mmax = max(mmax, sys->U_field[i]);
		}

		for (i = 0; i < numVerts; i++) {
			y = (sys->U_field[i] - mmin) / (mmax - mmin);
			x = y * 30;
			y = (x % 2 == 0 ? 0.1 : 0.9);
			defvert_add_index_notest(dv, defgrp_index, y);
			dv++;
		}

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
	QuadRemeshModifierData *lmd = (QuadRemeshModifierData *)md;
	QuadRemeshModifierData *tlmd = (QuadRemeshModifierData *)target;

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

static void freeData(ModifierData *md)
{
	QuadRemeshModifierData *lmd = (QuadRemeshModifierData *)md;
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
