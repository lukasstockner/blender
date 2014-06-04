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
	int *index_features;		/* Static vertex index list */
	int *ringf_indices;			/* Indices of faces per vertex */
	int *ringv_indices;			/* Indices of neighbors(vertex) per vertex */
	NLContext *context;			/* System for solve general implicit rotations */
	MeshElemMap *ringf_map;		/* Map of faces per vertex */
	MeshElemMap *ringv_map;		/* Map of vertex per vertex */
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
	sys->index_features = MEM_mallocN(sizeof(int)* (totalFeatures), "QuadRemeshFeatures");
	return sys;
}

static void deleteLaplacianSystem(LaplacianSystem *sys)
{
	MEM_SAFE_FREE(sys->index_features);
	MEM_SAFE_FREE(sys->ringf_indices);
	MEM_SAFE_FREE(sys->ringv_indices);
	MEM_SAFE_FREE(sys->ringf_map);
	MEM_SAFE_FREE(sys->ringv_map);

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

static void initLaplacianMatrix(LaplacianSystem *sys)
{
	
}

static void laplacianDeformPreview(LaplacianSystem *sys, float (*vertexCos)[3])
{
	int vid, i, j, n, na;
	n = sys->total_verts;
	na = sys->total_features;

#ifdef OPENNL_THREADING_HACK
	modifier_opennl_lock();
#endif

	/*if (!sys->is_matrix_computed) {
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
			nlRightHandSideSet(0, n + i, vertexCos[vid][0]);
			nlRightHandSideSet(1, n + i, vertexCos[vid][1]);
			nlRightHandSideSet(2, n + i, vertexCos[vid][2]);
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
		sys->is_matrix_computed = true;

	}
	else if (sys->has_solution) {
		nlMakeCurrent(sys->context);

		nlBegin(NL_SYSTEM);
		nlBegin(NL_MATRIX);

		for (i = 0; i < n; i++) {
			nlRightHandSideSet(0, i, sys->delta[i][0]);
			nlRightHandSideSet(1, i, sys->delta[i][1]);
			nlRightHandSideSet(2, i, sys->delta[i][2]);
		}
		for (i = 0; i < na; i++) {
			vid = sys->index_anchors[i];
			nlRightHandSideSet(0, n + i, vertexCos[vid][0]);
			nlRightHandSideSet(1, n + i, vertexCos[vid][1]);
			nlRightHandSideSet(2, n + i, vertexCos[vid][2]);
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
	*/

#ifdef OPENNL_THREADING_HACK
	modifier_opennl_unlock();
#endif
}



static void initSystem(QuadRemeshModifierData *qmd, Object *ob, DerivedMesh *dm,
	float(*vertexCos)[3], int numVerts, LaplacianSystem *sys)
{
	int i;
	int defgrp_index;
	int total_features;
	float wpaint;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;

	int *index_features = MEM_mallocN(sizeof(int)* numVerts, __func__);  /* over-alloc */
	MFace *tessface;
	STACK_DECLARE(index_features);
	STACK_INIT(index_features);

	modifier_get_vgroup(ob, dm, qmd->anchor_grp_name, &dvert, &defgrp_index);
	BLI_assert(dvert != NULL);
	dv = dvert;
	for (i = 0; i < numVerts; i++) {
		wpaint = defvert_find_weight(dv, defgrp_index);
		dv++;
		if (wpaint > 0.19f && wpaint < 0.89f) {
			STACK_PUSH(index_features, i);
		}
	}
	DM_ensure_tessface(dm);
	total_features = STACK_SIZE(index_features);
	sys = initLaplacianSystem(numVerts, dm->getNumEdges(dm), dm->getNumTessFaces(dm), total_features, qmd->anchor_grp_name);
	memcpy(sys->index_features, index_features, sizeof(int)* total_features);
	MEM_freeN(index_features);
	STACK_FREE(index_features);	
}

static void QuadRemeshModifier_do(
        QuadRemeshModifierData *lmd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	float (*filevertexCos)[3];
	int sysdif;
	LaplacianSystem *sys = NULL;

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
