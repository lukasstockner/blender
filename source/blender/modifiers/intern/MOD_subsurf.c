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
 * Contributor(s): Daniel Dunbar
 *                 Ton Roosendaal,
 *                 Ben Batt,
 *                 Brecht Van Lommel,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_subsurf.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"

#include "MOD_modifiertypes.h"

#include "intern/CCGSubSurf.h"

/* This is just for quick tests. */
#include "../../../intern/opensubdiv/opensubdiv_capi.h"

static void initData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;

	smd->levels = 1;
	smd->renderLevels = 2;
	smd->flags |= eSubsurfModifierFlag_SubsurfUv;
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
#endif
	SubsurfModifierData *tsmd = (SubsurfModifierData *) target;

	modifier_copyData_generic(md, target);

	tsmd->emCache = tsmd->mCache = NULL;

}

static void freeData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;

	if (smd->mCache) {
		ccgSubSurf_free(smd->mCache);
	}
	if (smd->emCache) {
		ccgSubSurf_free(smd->emCache);
	}
}

static bool isDisabled(ModifierData *md, int useRenderParams)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	int levels = (useRenderParams) ? smd->renderLevels : smd->levels;

	return get_render_subsurf_level(&md->scene->r, levels) == 0;
}

#if 1
static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag flag)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	SubsurfFlags subsurf_flags = 0;
	DerivedMesh *result;
	const int useRenderParams = flag & MOD_APPLY_RENDER;
	const int isFinalCalc = flag & MOD_APPLY_USECACHE;

	if (useRenderParams)
		subsurf_flags |= SUBSURF_USE_RENDER_PARAMS;
	if (isFinalCalc)
		subsurf_flags |= SUBSURF_IS_FINAL_CALC;
	if (ob->mode & OB_MODE_EDIT)
		subsurf_flags |= SUBSURF_IN_EDIT_MODE;
	
	result = subsurf_make_derived_from_derived(derivedData, smd, NULL, subsurf_flags);
	result->cd_flag = derivedData->cd_flag;
	
	if (useRenderParams || !isFinalCalc) {
		DerivedMesh *cddm = CDDM_copy(result);
		result->release(result);
		result = cddm;
	}

	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *UNUSED(ob),
                                    struct BMEditMesh *UNUSED(editData),
                                    DerivedMesh *derivedData,
                                    ModifierApplyFlag flag)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	DerivedMesh *result;
	/* 'orco' using editmode flags would cause cache to be used twice in editbmesh_calc_modifiers */
	SubsurfFlags ss_flags = (flag & MOD_APPLY_ORCO) ? 0 : (SUBSURF_FOR_EDIT_MODE | SUBSURF_IN_EDIT_MODE);

	result = subsurf_make_derived_from_derived(derivedData, smd, NULL, ss_flags);

	return result;
}
#else
static DerivedMesh *applyModifier(ModifierData *md, Object *UNUSED(ob),
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag flag)
{
#define MAX_STATIC_VERTS 64
	const bool useRenderParams = flag & MOD_APPLY_RENDER;

	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	struct OpenSubdiv_MeshDescr *mesh_descr;
	struct OpenSubdiv_EvaluationDescr *evaluation_descr;
	MVert *mvert;
	MPoly *mpoly;
	MLoop *mloop;
	DerivedMesh *result;
	int i, totvert, totpoly;
	int level = useRenderParams ? smd->renderLevels : smd->levels;
	int grid_size = (1 << (level - 1)) + 1;

	mvert = derivedData->getVertArray(derivedData);
	mpoly = derivedData->getPolyArray(derivedData);
	mloop = derivedData->getLoopArray(derivedData);
	totvert = derivedData->getNumVerts(derivedData);
	totpoly = derivedData->getNumPolys(derivedData);

	mesh_descr = openSubdiv_createMeshDescr();

	/* Create basis vertices of the mesh. */
	for (i = 0; i < totvert; i++) {
		openSubdiv_createMeshDescrVertex(mesh_descr, mvert[i].co);
	}

	/* Create basis faces of HBR mesh. */
	for (i = 0; i < totpoly; i++) {
		int loop, totloop = mpoly[i].totloop;
		int *indices;
		int indices_static[MAX_STATIC_VERTS];

		/* If number of vertices per face is low, we use sttaic array,
		 * this is so because of performance issues -- in most cases
		 * we'll just use sttaic array and wouldn't streess memory
		 * allocator at all.
		 */
		if (totloop <= MAX_STATIC_VERTS) {
			indices = indices_static;
		}
		else {
			indices = MEM_mallocN(sizeof(int) * totloop, "subsurf hbr tmp vertices");
		}

		/* Fill in vertex indices array. */
		for (loop = 0; loop < totloop; loop++) {
			int vertex = mloop[loop + mpoly[i].loopstart].v;
			indices[loop] = vertex;
		}

		openSubdiv_createMeshDescrFace(mesh_descr, totloop, indices);

		if (indices != indices_static) {
			MEM_freeN(indices);
		}
	}

	/* Finish mesh creation. */
	openSubdiv_finishMeshDescr(mesh_descr);

	evaluation_descr = openSubdiv_createEvaluationDescr(mesh_descr);

	result = CDDM_new(totpoly * (grid_size + 1) * (grid_size + 1), 0, 0, 0, 0);
	mvert = result->getVertArray(result);

	for (i = 0; i < totpoly; i++) {
		int x, y;
		for (x = 0; x <= grid_size; x++) {
			for (y = 0; y <= grid_size; y++) {
				float P[3];
				float u = (float) x / grid_size, v = (float) y / grid_size;
				openSubdiv_evaluateDescr(evaluation_descr, i, u, v, P, NULL, NULL);
				copy_v3_v3(mvert->co, P);
				mvert++;
			}
		}
	}

	/* Clean-up.. */
	openSubdiv_deleteEvaluationDescr(evaluation_descr);
	openSubdiv_deleteMeshDescr(mesh_descr);

	return result;
#undef MAX_STATIC_VERTS
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
                                    struct BMEditMesh *editData,
                                    DerivedMesh *derivedData,
                                    ModifierApplyFlag flag)
{
	(void) md;  /* Ignored. */
	(void) ob;  /* Ignored. */
	(void) editData;  /* Ignored. */
	(void) flag;  /* Ignored. */

	return derivedData;
}
#endif


ModifierTypeInfo modifierType_Subsurf = {
	/* name */              "Subsurf",
	/* structName */        "SubsurfModifierData",
	/* structSize */        sizeof(SubsurfModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode |
	                        eModifierTypeFlag_AcceptsCVs,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};

