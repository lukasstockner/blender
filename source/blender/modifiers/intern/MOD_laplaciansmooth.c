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
 *                 Campbell Barton,
 *                 Alexander Pinzon
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_laplaciansmooth.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_tessmesh.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"

#include "ONL_opennl.h"

struct BModLaplacianSystem {
	float *eweights;		/* Length weights per Edge */
	float (*fweights)[3];   /* Cotangent weights per face */
	float *ring_areas;		/* Total area per ring*/
	float *vlengths;		/* Total sum of lengths(edges) per vertice*/
	float *vweights;		/* Total sum of weights per vertice*/
	int numEdges;			/* Number of edges*/
	int numFaces;			/* Number of faces*/
	int numVerts;			/* Number of verts*/
	short *numNeFa;			/* Number of neighboors faces around vertice*/
	short *numNeEd;			/* Number of neighboors Edges around vertice*/
	short *zerola;			/* Is zero area or length*/
	
	NLContext *context;
};
typedef struct BModLaplacianSystem ModLaplacianSystem;

static float compute_volume(float (*vertexCos)[3], MFace *mfaces, int numFaces);
static float cotan_weight(float *v1, float *v2, float *v3);
static int isDisabled(ModifierData *md, int UNUSED(useRenderParams));
static void copyData(ModifierData *md, ModifierData *target);
static void delete_ModLaplacianSystem(ModLaplacianSystem * sys);
static void delete_void_MLS(void * data);
static void initData(ModifierData *md);
static void memset_ModLaplacianSystem(ModLaplacianSystem *sys, int val);
static void volume_preservation(float (*vertexCos)[3], int numVerts, float vini, float vend, short flag);
static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md);
static ModLaplacianSystem * init_ModLaplacianSystem( int a_numEdges, int a_numFaces, int a_numVerts);

static void delete_void_MLS(void * data)
{
	if (data) {
		MEM_freeN(data);
		data = NULL;
	}
}

static void delete_ModLaplacianSystem(ModLaplacianSystem * sys) 
{
	delete_void_MLS(sys->eweights);
	delete_void_MLS(sys->fweights);
	delete_void_MLS(sys->numNeEd);
	delete_void_MLS(sys->numNeFa);
	delete_void_MLS(sys->ring_areas);
	delete_void_MLS(sys->vlengths);
	delete_void_MLS(sys->vweights);
	delete_void_MLS(sys->zerola);
	if (sys->context) {
		nlDeleteContext(sys->context);
	}
	MEM_freeN(sys);
}

static void memset_ModLaplacianSystem(ModLaplacianSystem *sys, int val)
{
	memset(sys->eweights	, val, sizeof(float) * sys->numEdges);
	memset(sys->fweights	, val, sizeof(float) * sys->numFaces * 3);
	memset(sys->numNeEd		, val, sizeof(short) * sys->numVerts);
	memset(sys->numNeFa		, val, sizeof(short) * sys->numVerts);
	memset(sys->ring_areas	, val, sizeof(float) * sys->numVerts);
	memset(sys->vlengths	, val, sizeof(float) * sys->numVerts);
	memset(sys->vweights	, val, sizeof(float) * sys->numVerts);
	memset(sys->zerola		, val, sizeof(short) * sys->numVerts);
}

static ModLaplacianSystem * init_ModLaplacianSystem( int a_numEdges, int a_numFaces, int a_numVerts) {
	ModLaplacianSystem * sys; 
	sys = MEM_callocN(sizeof(ModLaplacianSystem), "ModLaplSmoothSystem");
	sys->numEdges = a_numEdges;
	sys->numFaces = a_numFaces;
	sys->numVerts = a_numVerts;

	sys->eweights =  MEM_callocN(sizeof(float) * sys->numEdges, "ModLaplSmoothEWeight");
	if (!sys->eweights) {
		delete_ModLaplacianSystem(sys);
		return NULL;
	}
	
	sys->fweights =  MEM_callocN(sizeof(float) * 3 * sys->numFaces, "ModLaplSmoothFWeight");
	if (!sys->fweights) {
		delete_ModLaplacianSystem(sys);
		return NULL;
	}

	sys->numNeEd =  MEM_callocN(sizeof(short) * sys->numVerts, "ModLaplSmoothNumNeEd");
	if (!sys->numNeEd) {
		delete_ModLaplacianSystem(sys);
		return NULL;
	}
	
	sys->numNeFa =  MEM_callocN(sizeof(short) * sys->numVerts, "ModLaplSmoothNumNeFa");
	if (!sys->numNeFa) {
		delete_ModLaplacianSystem(sys);
		return NULL;
	}
	
	sys->ring_areas =  MEM_callocN(sizeof(float) * sys->numVerts, "ModLaplSmoothRingAreas");
	if (!sys->ring_areas) {
		delete_ModLaplacianSystem(sys);
		return NULL;
	}
	
	sys->vlengths =  MEM_callocN(sizeof(float) * sys->numVerts, "ModLaplSmoothVlengths");
	if (!sys->vlengths) {
		delete_ModLaplacianSystem(sys);
		return NULL;
	}

	sys->vweights =  MEM_callocN(sizeof(float) * sys->numVerts, "ModLaplSmoothVweights");
	if (!sys->vweights) {
		delete_ModLaplacianSystem(sys);
		return NULL;
	}

	sys->zerola =  MEM_callocN(sizeof(short) * sys->numVerts, "ModLaplSmoothZeloa");
	if (!sys->zerola) {
		delete_ModLaplacianSystem(sys);
		return NULL;
	}

	return sys;
}

static void initData(ModifierData *md)
{
	LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *) md;
	smd->lambda = 0.00001f;
	smd->lambda_border = 0.00005f;
	smd->min_area = 0.00001f;
	smd->repeat = 1;
	smd->flag = MOD_LAPLACIANSMOOTH_X | MOD_LAPLACIANSMOOTH_Y | MOD_LAPLACIANSMOOTH_Z;
	smd->defgrp_name[0] = '\0';
}

static void copyData(ModifierData *md, ModifierData *target)
{
	LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *) md;
	LaplacianSmoothModifierData *tsmd = (LaplacianSmoothModifierData *) target;

	tsmd->lambda = smd->lambda;
	tsmd->lambda_border = smd->lambda_border;
	tsmd->min_area = smd->min_area;
	tsmd->repeat = smd->repeat;
	tsmd->flag = smd->flag;
	BLI_strncpy(tsmd->defgrp_name, smd->defgrp_name, sizeof(tsmd->defgrp_name));
}

static int isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *) md;
	short flag;

	flag = smd->flag & (MOD_LAPLACIANSMOOTH_X | MOD_LAPLACIANSMOOTH_Y | MOD_LAPLACIANSMOOTH_Z);

	/* disable if modifier is off for X, Y and Z or if factor is 0 */
	if ( flag == 0) return 1;

	return 0;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (smd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static float cotan_weight(float *v1, float *v2, float *v3)
{
	float a[3], b[3], c[3], clen;

	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v3, v1);
	cross_v3_v3v3(c, a, b);

	clen = len_v3(c);

	if (clen == 0.0f)
		return 0.0f;
	
	return dot_v3v3(a, b) / clen;
}

static float compute_volume(float (*vertexCos)[3], MFace *mfaces, int numFaces)
{
	float vol = 0.0f;
	float x1, y1, z1, x2, y2, z2, x3, y3, z3;
	int i;
	float *vn;
	float *vf[3];
	for (i = 0; i<numFaces; i++){
		vf[0] = vertexCos[mfaces[i].v1];
		vf[1] = vertexCos[mfaces[i].v2];
		vf[2] = vertexCos[mfaces[i].v3];

		x1 = vf[0][0];
		y1 = vf[0][1];
		z1 = vf[0][2];

		x2 = vf[1][0];
		y2 = vf[1][1];
		z2 = vf[1][2];

		x3 = vf[2][0];
		y3 = vf[2][1];
		z3 = vf[2][2];

		vol = vol + (1.0 / 6.0) * (0.0 - x3*y2*z1 + x2*y3*z1 + x3*y1*z2 - x1*y3*z2 - x2*y1*z3 + x1*y2*z3);
	}
	return fabs(vol);
}

static void volume_preservation(float (*vertexCos)[3], int numVerts, float vini, float vend, short flag)
{
	float beta;
	int i;

	if (vend != 0.0f) {	
		beta  = pow (vini / vend, 1.0f / 3.0f);
		for (i = 0; i<numVerts; i++) {
			if (flag & MOD_LAPLACIANSMOOTH_X) {
				vertexCos[i][0] = vertexCos[i][0] * beta;
			}
			if (flag & MOD_LAPLACIANSMOOTH_Y) {
				vertexCos[i][1] = vertexCos[i][1] * beta;
			}
			if (flag & MOD_LAPLACIANSMOOTH_Z) {
				vertexCos[i][2] = vertexCos[i][2] * beta;
			}
			
		}
	}
}

static void laplaciansmoothModifier_do(
        LaplacianSmoothModifierData *smd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	ModLaplacianSystem *sys;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	float *v1, *v2, *v3;
	float w1, w2, w3, wpaint;
	float areaf;
	int i, iter;
	int defgrp_index;
	unsigned int idv1, idv2, idv3;
	MFace *mfaces = NULL;
	MEdge *medges = NULL;

	DM_ensure_tessface(dm);

	sys = init_ModLaplacianSystem(dm->getNumEdges(dm), dm->getNumTessFaces(dm), numVerts);
	if(!sys) return;

	mfaces = dm->getTessFaceArray(dm);
	medges = dm->getEdgeArray(dm);
	modifier_get_vgroup(ob, dm, smd->defgrp_name, &dvert, &defgrp_index);

	
	for (iter = 0; iter < smd->repeat; iter++) {
		memset_ModLaplacianSystem(sys, 0);
		nlNewContext();
		sys->context = nlGetCurrent();
		nlSolverParameteri(NL_NB_VARIABLES, numVerts);
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
		nlSolverParameteri(NL_NB_ROWS, numVerts);
		nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);

		for ( i = 0; i < sys->numEdges; i++) {
			idv1 = medges[i].v1;
			idv2 = medges[i].v2;

			v1 = vertexCos[idv1];
			v2 = vertexCos[idv2];

			sys->numNeEd[idv1] = sys->numNeEd[idv1] + 1;
			sys->numNeEd[idv2] = sys->numNeEd[idv2] + 1;

			w1 = len_v3v3(v1, v2);

			if (fabs(w1) < smd->min_area) {
				sys->zerola[idv1] = 1;
				sys->zerola[idv2] = 1;
			} else {
				w1 = 1.0f / w1;
			}

			sys->eweights[i] = w1;
		}

		for ( i = 0; i < sys->numFaces; i++) {

			idv1 = mfaces[i].v1;
			idv2 = mfaces[i].v2;
			idv3 = mfaces[i].v3;

			sys->numNeFa[idv1] = sys->numNeFa[idv1] + 1;
			sys->numNeFa[idv2] = sys->numNeFa[idv2] + 1;
			sys->numNeFa[idv3] = sys->numNeFa[idv3] + 1;

			v1 = vertexCos[idv1];
			v2 = vertexCos[idv2];
			v3 = vertexCos[idv3];

			w1 = cotan_weight(v1, v2, v3);
			w2 = cotan_weight(v2, v3, v1);
			w3 = cotan_weight(v3, v1, v2);

			areaf = area_tri_v3(v1, v2, v3);
			if (fabs(areaf) < smd->min_area) { 
				sys->zerola[idv1] = 1;
				sys->zerola[idv2] = 1;
				sys->zerola[idv3] = 1;
			}

			sys->fweights[i][0] = sys->fweights[i][0] + w1;
			sys->fweights[i][1] = sys->fweights[i][1] + w2;
			sys->fweights[i][2] = sys->fweights[i][2] + w3;

			sys->vweights[idv1] = sys->vweights[idv1] + w2 + w3;
			sys->vweights[idv2] = sys->vweights[idv2] + w1 + w3;
			sys->vweights[idv3] = sys->vweights[idv3] + w1 + w2;

			sys->ring_areas[idv1] = sys->ring_areas[idv1] + areaf;
			sys->ring_areas[idv2] = sys->ring_areas[idv2] + areaf;
			sys->ring_areas[idv3] = sys->ring_areas[idv3] + areaf;

		}

		for ( i = 0; i < sys->numEdges; i++) {
			idv1 = medges[i].v1;
			idv2 = medges[i].v2;
			/* Is boundary if number of faces != number of edges around vertice */
			if (sys->numNeEd[idv1] != sys->numNeFa[idv1] && sys->numNeEd[idv2] != sys->numNeFa[idv2]) { 
				sys->vlengths[idv1] = sys->vlengths[idv1] + sys->eweights[i];
				sys->vlengths[idv2] = sys->vlengths[idv2] + sys->eweights[i];
			}
		}

		nlBegin(NL_SYSTEM);	
		for (i = 0; i < numVerts; i++) {
			nlSetVariable(0, i, vertexCos[i][0]);
			nlSetVariable(1, i, vertexCos[i][1]);
			nlSetVariable(2, i, vertexCos[i][2]);
		}

		nlBegin(NL_MATRIX);

		dv = dvert;
		for (i = 0; i < numVerts; i++) {
			nlRightHandSideAdd(0, i, vertexCos[i][0]);
			nlRightHandSideAdd(1, i, vertexCos[i][1]);
			nlRightHandSideAdd(2, i, vertexCos[i][2]);

			if (dv) {
				wpaint = defvert_find_weight(dv, defgrp_index);
				dv++;
			} else {
				wpaint = 1.0f;
			}
		
			if (sys->zerola[i] == 0) {
				if (sys->vweights[i] * sys->ring_areas[i] != 0.0f) {
					sys->vweights[i] = - smd->lambda * wpaint / (4.0f * sys->vweights[i] * sys->ring_areas[i]);
				}
				if (sys->vlengths[i] != 0.0f) {
					sys->vlengths[i] = - smd->lambda_border * wpaint * 2.0f / sys->vlengths[i];
				}
				if (sys->numNeEd[i] == sys->numNeFa[i]) { 
					nlMatrixAdd(i, i,  1.0f + smd->lambda * wpaint / (4.0f * sys->ring_areas[i]));
				} else { 
					nlMatrixAdd(i, i,  1.0f + smd->lambda_border * wpaint * 2.0f);
				}
			} else {
				nlMatrixAdd(i, i, 1.0f);
			}
		}

		for ( i = 0; i < sys->numFaces; i++) {
			idv1 = mfaces[i].v1;
			idv2 = mfaces[i].v2;
			idv3 = mfaces[i].v3;

			/* Is ring if number of faces == number of edges around vertice*/
			if (sys->numNeEd[idv1] == sys->numNeFa[idv1] && sys->zerola[idv1] == 0) { 
				nlMatrixAdd(idv1, idv2, sys->fweights[i][2] * sys->vweights[idv1]);
				nlMatrixAdd(idv1, idv3, sys->fweights[i][1] * sys->vweights[idv1]);
			}
			if (sys->numNeEd[idv2] == sys->numNeFa[idv2] && sys->zerola[idv2] == 0) { 
				nlMatrixAdd(idv2, idv1, sys->fweights[i][2] * sys->vweights[idv2]);
				nlMatrixAdd(idv2, idv3, sys->fweights[i][0] * sys->vweights[idv2]);
			}
			if (sys->numNeEd[idv3] == sys->numNeFa[idv3] && sys->zerola[idv3] == 0) { 
				nlMatrixAdd(idv3, idv1, sys->fweights[i][1] * sys->vweights[idv3]);
				nlMatrixAdd(idv3, idv2, sys->fweights[i][0] * sys->vweights[idv3]);
			}

		}

		for ( i = 0; i < sys->numEdges; i++) {
			idv1 = medges[i].v1;
			idv2 = medges[i].v2;
			/* Is boundary if number of faces != number of edges around vertice */
			if (sys->numNeEd[idv1] != sys->numNeFa[idv1] && sys->numNeEd[idv2] != sys->numNeFa[idv2] && sys->zerola[idv1] == 0 && sys->zerola[idv2] == 0) {
				nlMatrixAdd(idv1, idv2, sys->eweights[i] * sys->vlengths[idv1]);
				nlMatrixAdd(idv2, idv1, sys->eweights[i] * sys->vlengths[idv2]);
			}
		}

		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);

		if (nlSolveAdvanced(NULL, NL_TRUE) ) {
			float vini, vend;
			vini = compute_volume(vertexCos, mfaces, sys->numFaces);
			for (i = 0; i < numVerts; i++) {
				if (smd->flag & MOD_LAPLACIANSMOOTH_X){
					vertexCos[i][0] = nlGetVariable(0, i);
				}
				if (smd->flag & MOD_LAPLACIANSMOOTH_Y){
					vertexCos[i][1] = nlGetVariable(1, i);
				}
				if (smd->flag & MOD_LAPLACIANSMOOTH_Z){
					vertexCos[i][2] = nlGetVariable(2, i);
				}
			}
			vend = compute_volume(vertexCos, mfaces, sys->numFaces);
			volume_preservation(vertexCos, numVerts, vini, vend, smd->flag);
		}
		nlDeleteContext(sys->context);
		sys->context = NULL;
	}
	delete_ModLaplacianSystem(sys);

}

static void deformVerts(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                        float (*vertexCos)[3], int numVerts, ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = get_dm(ob, NULL, derivedData, NULL, 0);

	laplaciansmoothModifier_do((LaplacianSmoothModifierData *)md, ob, dm,
	                  vertexCos, numVerts);

	if (dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(
        ModifierData *md, Object *ob, struct BMEditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = get_dm(ob, editData, derivedData, NULL, 0);

	laplaciansmoothModifier_do((LaplacianSmoothModifierData *)md, ob, dm,
	                  vertexCos, numVerts);

	if (dm != derivedData)
		dm->release(dm);
}


ModifierTypeInfo modifierType_LaplacianSmooth = {
	/* name */              "Laplacian Smooth",
	/* structName */        "LaplacianSmoothModifierData",
	/* structSize */        sizeof(LaplacianSmoothModifierData),
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
