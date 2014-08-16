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

/** \file blender/modifiers/intern/MOD_quadremesh_geom.c
 *  \ingroup modifiers
 */

#include "MEM_guardedalloc.h"
#include "MOD_quadremesh_geom.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_rand.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"

#include "MOD_util.h"

static GFList *newGFList(int value)
{
	GFList *mnewGFList = MEM_mallocN(sizeof(GFList), __func__);
	mnewGFList->index = value;
	mnewGFList->next = NULL;
	return mnewGFList;
}

static void deleteGFList(GFList *l)
{
	if (l) {
		if (l->next) {
			deleteGFList(l->next);
		}
		MEM_SAFE_FREE(l);
	}
}

static void addNodeGFList(GFList *l, int value)
{
	if (l) {
		if (l->next) {
			addNodeGFList(l->next, value);
		}
		else{
			l->next = newGFList(value);
		}
	}
}

static int getSizeGFList(GFList *l) {
	if (l) {
		return 1 + getSizeGFList(l->next);
	}
	else{
		return 0;
	}
}

static void estimateNumberGFVerticesEdges(int ve[2], LaplacianSystem *sys, float h)
{
	int i, totalv, totale;
	float area = 0.0f;
	float sqrtarea;
	for (i = 0; i < sys->total_faces; i++) {
		area += area_tri_v3(sys->co[sys->faces[i][0]], sys->co[sys->faces[i][1]], sys->co[sys->faces[i][2]]);
	}
	sqrtarea = sqrtf(area);
	if (h > 0.0f) {
		totalv = ((sqrtarea / h) + 1.0f);
		totale = totalv * sqrtarea * 2.0f;
		totalv = totalv * totalv;
	}
	else{
		totalv = sqrtarea + 1.0f;
		totale = totalv * sqrtarea * 2.0f;
		totalv = totalv * totalv;
	}
	ve[0] = totalv;
	ve[1] = totale;
}

static GradientFlowMesh *newGradientFlowMesh(int totalvert, int totaledge)
{
	GradientFlowMesh *gfmesh = MEM_mallocN(sizeof(GradientFlowMesh), "GradientFlowMesh");
	gfmesh->mvert = MEM_mallocN(sizeof(GradientFlowVert)* totalvert, "GradientFlowVert");
	gfmesh->medge = MEM_mallocN(sizeof(GradientFlowEdge)* totalvert, "GradientFlowEdge");
	gfmesh->totvert = 0;
	gfmesh->totedge = 0;
	gfmesh->allocvert = totalvert;
	gfmesh->allocedge = totaledge;
}

static void deleteGradientFlowMesh(GradientFlowMesh * gfmesh) 
{
	if (gfmesh) {
		MEM_SAFE_FREE(gfmesh->mvert);
		MEM_SAFE_FREE(gfmesh->medge);
		MEM_SAFE_FREE(gfmesh);
	}
}

static int addGFVertGFMesh(GradientFlowMesh *gfmesh, GradientFlowVert gfvert)
{
	return addVertGFMesh(gfmesh, gfvert.co, gfvert.ori_e);
}

static int addVertGFMesh(GradientFlowMesh *gfmesh, float co[3], int index_edge)
{
	if (gfmesh->totvert == gfmesh->allocvert) {
		gfmesh->mvert = MEM_reallocN(gfmesh->mvert, sizeof(GradientFlowVert)* (gfmesh->allocvert + MOD_QUADREMESH_ALLOC_BLOCK));
		gfmesh->allocvert += MOD_QUADREMESH_ALLOC_BLOCK;
	}
	copy_v3_v3(gfmesh->mvert[gfmesh->totvert].co, co);
	gfmesh->mvert[gfmesh->totvert].ori_e = index_edge;
	gfmesh->totvert++;
	return gfmesh->totvert - 1;
}

static int addGFEdgeGFMesh(GradientFlowMesh *gfmesh, GradientFlowEdge gfedge)
{
	return addEdgeGFMesh(gfmesh, gfedge.v1, gfedge.v2, gfedge.ori_f);
}

static int addEdgeGFMesh(GradientFlowMesh *gfmesh, int index_v1, int index_v2, int index_face)
{
	if (gfmesh->totedge == gfmesh->allocedge) {
		gfmesh->medge = MEM_reallocN(gfmesh->medge, sizeof(GradientFlowEdge)* (gfmesh->allocedge + MOD_QUADREMESH_ALLOC_BLOCK));
		gfmesh->allocedge += MOD_QUADREMESH_ALLOC_BLOCK;
	}
	gfmesh->medge[gfmesh->totedge].ori_f = index_face;
	gfmesh->medge[gfmesh->totedge].v1 = index_v1;
	gfmesh->medge[gfmesh->totedge].v2 = index_v2;
	gfmesh->totedge++;
	return gfmesh->totedge - 1;
}

static GradientFlowSystem *newGradientFlowSystem(LaplacianSystem *sys, float *mhfunction)
{
	int ve[2], i;
	int *lverts, sizeverts;
	estimateNumberGFVerticesEdges(ve, sys, sys->h);
	GradientFlowSystem * gfsys = MEM_mallocN(sizeof(GradientFlowSystem), "GradientFlowSystem");
	gfsys->mesh = newGradientFlowMesh(ve[0], ve[1]);
	gfsys->ringf_list = MEM_mallocN(sizeof(GFList *) * sys->total_faces, "GFListFaces");
	gfsys->ringe_list = MEM_mallocN(sizeof(GFList *) * sys->total_edges, "GFListEdges");
	gfsys->heap_seeds = BLI_heap_new();
	gfsys->totalf = sys->total_faces;
	gfsys->totale = sys->total_edges;
	for (i = 0; i < sys->total_faces; i++) {
		gfsys->ringf_list[i] = NULL;
	}
	for (i = 0; i < sys->total_edges; i++) {
		gfsys->ringe_list[i] = NULL;
	}
	sizeverts = findFeaturesOnMesh(lverts, sys);
	GradientFlowVert *mv;
	for (i = 0; i < sizeverts; i++) {
		mv = MEM_mallocN(sizeof(GradientFlowVert), __func__);
		copy_v3_v3(mv->co, sys->co[lverts[i]]);
		mv->ori_e = sys->ringe_map[lverts[i]].indices[0];
		addSeedToQueue(gfsys->heap_seeds, 0.0f, mv);
	}
	gfsys->hfunction = mhfunction;
}

static void deleteGradientFlowSystem(GradientFlowSystem *gfsys) 
{
	int i;
	if (gfsys) {
		for (i = 0; i < gfsys->totalf; i++) {
			MEM_SAFE_FREE(gfsys->ringf_list[i]);
		}
		for (i = 0; i < gfsys->totale; i++) {
			MEM_SAFE_FREE(gfsys->ringe_list[i]);
		}
		deleteGradientFlowMesh(gfsys->mesh);
		BLI_heap_free(gfsys->heap_seeds, MEM_freeN);
		MEM_SAFE_FREE(gfsys);
	}
}

static int addGFVertGFSystem(GradientFlowSystem *gfsys, GradientFlowVert gfvert)
{
	return addVertGFSystem(gfsys, gfvert.co, gfvert.ori_e);
}

static int addVertGFSystem(GradientFlowSystem *gfsys, float co[3], int index_edge)
{
	int pos = addVertGFMesh(gfsys->mesh, co, index_edge);

	if (index_edge >= 0) {
		if (gfsys->ringe_list[index_edge]) {
			addNodeGFList(gfsys->ringe_list[index_edge], pos);
		}
		else{
			gfsys->ringe_list[index_edge] = newGFList(pos);
		}
	}

	return pos;
}

static int addGFEdgeGFSystem(GradientFlowSystem *gfsys, GradientFlowEdge gfedge)
{
	return addEdgeGFSystem(gfsys, gfedge.v1, gfedge.v2, gfedge.ori_f);
}

static int addEdgeGFSystem(GradientFlowSystem *gfsys, int index_v1, int index_v2, int index_face)
{
	int pos = addEdgeGFMesh(gfsys->mesh, index_v1, index_v2, index_face);
	if (index_face >= 0) {
		if (gfsys->ringf_list[index_face]) {
			addNodeGFList(gfsys->ringf_list[index_face], pos);
		}
		else{
			gfsys->ringf_list[index_face] = newGFList(pos);
		}
	}
	return pos;
}

/*
* List of vertices from original mesh with special features (edge dihedral angle less that 90) to be preserves
* return the size of array
*/
static int findFeaturesOnMesh(int * lverts, LaplacianSystem *sys)
{
	int i, f1, f2, total;
	float angle;
	int *listverts = MEM_callocN(sizeof(int) * sys->total_verts, __func__);
	int *listdest = NULL;
	total = 0;

	for (i = 0; i < sys->total_edges; i++) {
		f1 = sys->faces_edge[i][0];
		f2 = sys->faces_edge[i][1];
		angle = angle_normalized_v3v3(sys->no[f1], sys->no[f2]);
		if (angle <= M_PI_2) {
			listverts[sys->edges[i][0]] = 1;
			listverts[sys->edges[i][1]] = 1;
		}
	}

	for (i = 0; i < sys->total_verts; i++) {
		if (sys->constraints[i] == 1) {
			listverts[i] = 1;
		}
	}

	for (i = 0; i<sys->total_verts; i++) {
		if (listverts[i] == 1) {
			total++;
		}
	}
	if (total > 0) {
		listdest = MEM_mallocN(sizeof(int)* total, __func__);
	}
	total = 0;
	for (i = 0; i<sys->total_verts; i++) {
		if (listverts[i] == 1) {
			listdest[total++] = i;
		}
	}
	lverts = listdest;
	MEM_SAFE_FREE(listverts);
	return total;
}

static void addSeedToQueue(Heap *aheap, float value, GradientFlowVert *vert)
{
	BLI_heap_insert(aheap, value, vert);
}

static GradientFlowVert *getTopSeedFromQueue(struct Heap *aheap)
{
	GradientFlowVert *vert = BLI_heap_popmin(aheap);
	return vert;

}

