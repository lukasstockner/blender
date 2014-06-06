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
	int *constraints;			/* Feature points constraints*/
	float *weights;				/* Feature points weights*/
	float *U_field;				/* Initial scalar field*/
	unsigned int(*faces)[4];	/* Copy of MFace (tessface) v1-v4 */
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
	sys->constraints = MEM_mallocN(sizeof(int) * totalVerts, "QuadRemeshConstraints");
	sys->weights = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshWeights");
	sys->U_field = MEM_mallocN(sizeof(float)* (totalVerts), "QuadRemeshUField");
	return sys;
}

static void deleteLaplacianSystem(LaplacianSystem *sys)
{
	MEM_SAFE_FREE(sys->faces);
	MEM_SAFE_FREE(sys->co);
	MEM_SAFE_FREE(sys->constraints);
	MEM_SAFE_FREE(sys->weights);
	MEM_SAFE_FREE(sys->U_field);
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

	printf("initLaplacianMatrix 0 \n");

	for (fi = 0; fi < sys->total_faces; fi++) {
		const unsigned int *vidf = sys->faces[fi];

		idv1 = vidf[0];
		idv2 = vidf[1];
		idv3 = vidf[2];
		idv4 = vidf[3];

		has_4_vert = vidf[3] ? 1 : 0;
		i = has_4_vert ? 4 : 3;
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
	printf("initLaplacianMatrix 1 \n");
	
}

static void laplacianDeformPreview(LaplacianSystem *sys)
{
	int vid, i, j, n, na;
	if (sys) {
		printf("laplacianDeformPreview NOT NULL\n");
	} 
	else {
		printf("laplacianDeformPreview NULL \n");
	}
	printf("laplacianDeformPreview -4\n");
	printf("sys->total_verts test\n");
	if (sys->total_verts) {
		printf("sys->total_verts NOT NULL\n");
	}
	else {
		printf("sys->total_verts NULL\n");
	}


	printf("%f,  %f", sys->total_verts, sys->total_features);
	


	n = sys->total_verts;
	na = sys->total_features;

	printf("laplacianDeformPreview -3\n");

#ifdef OPENNL_THREADING_HACK
	printf("laplacianDeformPreview -2\n");
	modifier_opennl_lock();
	printf("laplacianDeformPreview -1\n");
#endif

	printf("laplacianDeformPreview 0\n");
	if (!sys->is_matrix_computed) {
		printf("laplacianDeformPreview 1\n");
		nlNewContext();
		sys->context = nlGetCurrent();

		nlSolverParameteri(NL_NB_VARIABLES, n);
		nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
		nlSolverParameteri(NL_NB_ROWS, n);
		nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 1);
		nlBegin(NL_SYSTEM);
		printf("laplacianDeformPreview 2\n");
		for (i = 0; i < n; i++) {
			nlSetVariable(0, i, 0);
		}
		
		printf("laplacianDeformPreview 3\n");
		nlBegin(NL_MATRIX);

		initLaplacianMatrix(sys);
		printf("laplacianDeformPreview 4\n");

		for (i = 0; i < n; i++) {
			if (sys->constraints[i] == 1) {
				//printf("i: %d, w:%f \n", i, sys->weights[i]);
				nlRightHandSideSet(0, i, sys->weights[i]);
			}
			else {
				nlRightHandSideSet(0, i, 0);
			}
		}
		printf("laplacianDeformPreview 6\n");
		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);
		if (nlSolveAdvanced(NULL, NL_TRUE)) {
			sys->has_solution = true;

			for (vid = 0; vid < sys->total_verts; vid++) {
				sys->U_field[vid] = nlGetVariable(0, vid);
			}
			printf("Solution found.\n");
			
			
		}
		else {
			printf("Solution not found.\n");
			sys->has_solution = false;
		}
		sys->is_matrix_computed = true;
	
	}
	

#ifdef OPENNL_THREADING_HACK
	modifier_opennl_unlock();
#endif
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

	printf("initSystem\n");

	modifier_get_vgroup(ob, dm, qmd->anchor_grp_name, &dvert, &defgrp_index);
	BLI_assert(dvert != NULL);
	dv = dvert;
	j = 0;
	printf("initSystem 0\n");
	for (i = 0; i < numVerts; i++) {
		wpaint = defvert_find_weight(dv, defgrp_index);
		dv++;

		if (wpaint < 0.19 || wpaint > 0.89) {
			constraints[i] = 1;
			weights[i] = -1.0f + wpaint * 2.0f;
			//printf("\t %f", weights[j]);
			j++;
		}
		else {
			constraints[i] = 0;
		}
	}

	total_features = j;

	printf("initSystem 1\n");

	DM_ensure_tessface(dm);
	sys = initLaplacianSystem(numVerts, dm->getNumEdges(dm), dm->getNumTessFaces(dm), total_features, qmd->anchor_grp_name);

	
	printf("initSystem 1.1\n");
	printf("initSystem 1.2\n");
	memcpy(sys->co, vertexCos, sizeof(float[3]) * numVerts);
	printf("initSystem 1.3\n");
	memcpy(sys->constraints, constraints, sizeof(int) * numVerts);
	printf("initSystem 1.4\n");
	memcpy(sys->weights, weights, sizeof(float)* numVerts);
	printf("initSystem 1.5\n");
	

	printf("initSystem 1.6\n");
	printf("initSystem 1.7\n");
	MEM_freeN(weights);
	printf("initSystem 1.8\n");
	MEM_freeN(constraints);
	printf("initSystem 1.9\n");

	printf("initSystem 2\n");

	tessface = dm->getTessFaceArray(dm);

	for (i = 0; i < sys->total_faces; i++) {
		memcpy(&sys->faces[i], &tessface[i].v1, sizeof(*sys->faces));
	}

	printf("Total vertsss %f\n", sys->total_verts);

	printf("initSystem 3\n");
	return sys;
	
}

static void QuadRemeshModifier_do(
        QuadRemeshModifierData *qmd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	float (*filevertexCos)[3];
	int sysdif, i;
	LaplacianSystem *sys = NULL;
	int defgrp_index;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	float mmin = 1000, mmax = 0;
	float y;

	if (numVerts == 0) return;
	printf("numVerts %d\n", numVerts);

	printf("QuadRemeshModifier_do 0\n");
	if (strlen(qmd->anchor_grp_name) < 3) return;
	printf("QuadRemeshModifier_do 2\n");
	sys = initSystem(qmd, ob, dm, vertexCos, numVerts);
	printf("QuadRemeshModifier_do 3\n");
	laplacianDeformPreview(sys);
	printf("QuadRemeshModifier_do 4\n");

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
			defvert_add_index_notest(dv, defgrp_index, y);
			dv++;
		}

	}
	
	//deleteLaplacianSystem(sys);
	
	
	

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
