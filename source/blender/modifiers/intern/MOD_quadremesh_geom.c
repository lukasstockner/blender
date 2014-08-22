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

static bool isOnSegmentLine(float p1[3], float p2[3], float q[3]){
	if (fabsf(len_v3v3(p1, q) + len_v3v3(p2, q) - len_v3v3(p1, p2)) < 0.000001f) {
		return true;
	}
	return false;
}

/*
* Return 1 if the intersections exist
* Return -1 if the intersections does not exist
*/
static bool intersecionLineSegmentWithVector(float r[3], float p1[3], float p2[3], float ori[3], float dir[3])
{
	float v[3], i1[3], i2[3];
	int i;

	add_v3_v3v3(v, ori, dir);
	i = isect_line_line_v3(p1, p2, ori, v, i1, i2);
	if (i == 0) {
		sub_v3_v3v3(i1, p1, ori);
		normalize_v3(i1);
		if (equals_v3v3(i1, dir)) {
			copy_v3_v3(r, p1);
		}
		else {
			copy_v3_v3(r, p2);
		}
	}
	else {
		sub_v3_v3v3(v, i1, ori);
		normalize_v3(v);
		if (equals_v3v3(v, dir)) {
			if (isOnSegmentLine(p1, p2, i1)) {
				copy_v3_v3(r, i1);
			}
			else{
				return false;
			}
		}
		else {
			return false;
		}
	}
	return true;
}

static bool intersectionVectorWithTriangle(float r[3], float p1[3], float p2[3], float p3[3], float ori[3], float dir[3])
{
	if (intersecionLineSegmentWithVector(r, p1, p2, ori, dir) == 1) {
		return true;
	}
	else if (intersecionLineSegmentWithVector(r, p2, p3, ori, dir) == 1) {
		return true;
	}
	else if (intersecionLineSegmentWithVector(r, p3, p1, ori, dir) == 1) {
		return true;
	}
	return false;

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

static int getOtherFaceAdjacentToEdge(LaplacianSystem *sys, int oldface, int inde)
{
	if (sys->faces_edge[inde][0] == oldface) {
		return sys->faces_edge[inde][1];
	}

	return sys->faces_edge[inde][0];
}

/* Project Gradient fields on face*/
static void projectVectorOnFace(float r[3], float no[3], float dir[3])
{
	float g[3], val, u[3], w[3];
	normalize_v3_v3(g, dir);
	val = dot_v3v3(g, no);
	mul_v3_v3fl(u, no, val);
	sub_v3_v3v3(w, g, u);
	normalize_v3_v3(r, w);
}

static int getDifferentVertexFaceEdge(LaplacianSystem *sys, int oldface, int inde)
{
	int i1, i2, i3;
	i1 = sys->edges[inde][0];
	i2 = sys->edges[inde][1];

	if (i1 == sys->faces[oldface][0]) {
		if (i2 == sys->faces[oldface][1]) {
			i3 = sys->faces[oldface][2];
		}
		else {
			i3 = sys->faces[oldface][1];
		}
	}
	else if (i1 == sys->faces[oldface][1]) {
		if (i2 == sys->faces[oldface][2]) {
			i3 = sys->faces[oldface][0];
		}
		else {
			i3 = sys->faces[oldface][2];
		}
	}
	else {
		if (i2 == sys->faces[oldface][0]) {
			i3 = sys->faces[oldface][1];
		}
		else {
			i3 = sys->faces[oldface][0];
		}
	}

	return i3;
}

/*
* ori coordinate of origin point
* dir direction to made query
* indexface Face in original mesh
* maxradius lenght to made query on dir direction
*/

static int nearGFEdgeInGFMesh(LaplacianSystem *sys, GradientFlowSystem *gfsys, float ori[3], float dir[3], int indexface, float maxradius)
{
	GFList *gfl;
	int index_gfedge, iv1, iv2, eid, newface;
	int res;
	float i1[3], i2[3], v[3], r[3];
	add_v3_v3v3(v, ori, dir);
	/* Query on flow lines inside face[indexface]*/
	if (gfsys->ringf_list[indexface]) {
		gfl = gfsys->ringf_list[indexface];
		while (gfl) {
			index_gfedge = gfl->index;
			if (index_gfedge >= 0 && index_gfedge < gfsys->mesh->totedge) {
				iv1 = gfsys->mesh->medge[index_gfedge].v1;
				iv2 = gfsys->mesh->medge[index_gfedge].v2;
				

				if (intersecionLineSegmentWithVector(i1, gfsys->mesh->mvert[iv1].co, gfsys->mesh->mvert[iv2].co, ori, v)) {
					if (len_v3v3(i1, ori) < maxradius){
						return index_gfedge;
					}
				}
			}
			gfl = gfl->next;
		}
	}
	else {
		/*Do not flow lines found, then search on adjacent faces*/
		if (intersecionLineSegmentWithVector(i1, sys->co[sys->faces[indexface][0]], sys->co[sys->faces[indexface][1]],
			ori, v)) {
			res = len_v3v3(i1, ori);
			if (res > maxradius) {
				return -1;
			}
			else {
				eid = getEdgeFromVerts(sys, sys->faces[indexface][0], sys->faces[indexface][1]);
				newface = getOtherFaceAdjacentToEdge(sys, indexface, eid);
				projectVectorOnFace(r, sys->no[indexface], dir);
				return nearGFEdgeInGFMesh(sys, gfsys, i1, r, newface, maxradius - res);
			}			
		}
		else if (intersecionLineSegmentWithVector(i1, sys->co[sys->faces[indexface][1]], sys->co[sys->faces[indexface][2]],
			ori, v)) {
			res = len_v3v3(i1, ori);
			if (res > maxradius) {
				return -1;
			}
			else {
				eid = getEdgeFromVerts(sys, sys->faces[indexface][1], sys->faces[indexface][2]);
				newface = getOtherFaceAdjacentToEdge(sys, indexface, eid);
				projectVectorOnFace(r, sys->no[indexface], dir);
				return nearGFEdgeInGFMesh(sys, gfsys, i1, r, newface, maxradius - res);
			}
		}
		else if (intersecionLineSegmentWithVector(i1, sys->co[sys->faces[indexface][2]], sys->co[sys->faces[indexface][0]],
			ori, v)) {
			res = len_v3v3(i1, ori);
			if (res > maxradius) {
				return -1;
			}
			else {
				eid = getEdgeFromVerts(sys, sys->faces[indexface][2], sys->faces[indexface][0]);
				newface = getOtherFaceAdjacentToEdge(sys, indexface, eid);
				projectVectorOnFace(r, sys->no[indexface], dir);
				return nearGFEdgeInGFMesh(sys, gfsys, i1, r, newface, maxradius - res);
			}
		}
	}
	return -1;
}

/**
* return -1 if max U was found
* float r[3] vertex with next point on flow line
* float q[3] actual point on flow line.
* int olface index of old face
* int inde edge from origin of actual point
*/
static int nextPointFlowLine(float r[3], LaplacianSystem *sys, float q[3], int oldface, int inde)
{
	float v1[3], v2[3], dir[3], dq[3], res[3], r2[3];
	float u1, u2, u3, u4, maxu;
	int i1, i2, i3, i4, ix;
	int newface, fs[2];
	int numv, *vidn;
	int i, iu, ie, isect;
	i1 = sys->edges[inde][0];
	i2 = sys->edges[inde][1];
	copy_v3_v3(v1, sys->co[i1]);
	copy_v3_v3(v2, sys->co[i2]);
	i3 = getDifferentVertexFaceEdge(sys, oldface, inde);
	u1 = sys->U_field[i1];
	u2 = sys->U_field[i2];
	u3 = sys->U_field[i3];
	copy_v2_v2_int(fs, sys->faces_edge[inde]);
	//getFacesAdjacentToEdge(fs, sys, inde);
	newface = fs[0] == oldface ? fs[1] : fs[0];
	i4 = getDifferentVertexFaceEdge(sys, newface, inde);
	u4 = sys->U_field[i4];

	/* The actual point on flow line correspond to a vertex in a mesh */
	if (equals_v3v3(q, v1) || equals_v3v3(q, v2)) {
		ix = equals_v3v3(q, v1) ? i1 : i2;
		numv = sys->ringv_map[ix].count;
		vidn = sys->ringf_map[ix].indices;
		iu = -1;
		maxu = -1000000;
		for (i = 0; i < numv; i++) {
			if (vidn[i] != ix) {
				if (sys->U_field[ix] < sys->U_field[vidn[i]]) {
					if (maxu < sys->U_field[vidn[i]]){
						iu = vidn[i];
						maxu = sys->U_field[vidn[i]];
					}

				}
			}
		}
		/*Max U found*/
		if (iu == -1) {
			printf("/*Max U found*/\n");
			return -1;
		}

		ie = getEdgeFromVerts(sys, ix, iu);

		//getFacesAdjacentToEdge(fs, sys, ie);
		copy_v2_v2_int(fs, sys->faces_edge[ie]);
		i1 = ix;
		i2 = iu;
		i3 = getDifferentVertexFaceEdge(sys, fs[0], ie);
		i4 = getDifferentVertexFaceEdge(sys, fs[1], ie);
		u1 = sys->U_field[i1];
		u2 = sys->U_field[i2];
		u3 = sys->U_field[i3];
		u3 = sys->U_field[i4];

		/* the next point is the opposite vertex in the edge*/
		if (u2 >= u3 && u2 >= u4 && u1 >= u3 && u1 >= u4) {
			copy_v3_v3(r, sys->co[iu]);
			return ie;

			/* the next point is on face fs[0]*/
		}
		else if (u3 >= u4) {
			copy_v3_v3(dir, sys->gf1[fs[0]]);
			//projectGradientOnFace(dir, sys, sys->gf1, fs[0]);
			mul_v3_fl(dir, 100);
			add_v3_v3v3(dq, q, dir);
			isect = isect_line_line_v3(sys->co[i3], sys->co[i2], q, dq, res, r2);
			copy_v3_v3(r, res);
			return ie;
			/* the next point is on face fs[1]*/
		}
		else {
			copy_v3_v3(dir, sys->gf1[fs[1]]);
			//projectGradientOnFace(dir, sys, sys->gf1, fs[1]);
			mul_v3_fl(dir, 100);
			add_v3_v3v3(dq, q, dir);
			isect = isect_line_line_v3(sys->co[i4], sys->co[i2], q, dq, res, r2);
			copy_v3_v3(r, res);
			return ie;
		}

		/* There is simple intersection on new face adjacent to inde */
	}
	else if (u1 <= u3 && u2 <= u3) {
		copy_v3_v3(dir, sys->gf1[newface]);
		//projectGradientOnFace(dir, sys, sys->gf1, newface);
		mul_v3_fl(dir, 100);
		add_v3_v3v3(dq, q, dir);
		if (u1 >= u2) {
			isect = isect_line_line_v3(sys->co[i3], sys->co[i1], q, dq, res, r2);
			copy_v3_v3(r, res);
			return getEdgeFromVerts(sys, i3, i1);
		}
		else {
			isect = isect_line_line_v3(sys->co[i3], sys->co[i2], q, dq, res, r2);
			copy_v3_v3(r, res);
			return getEdgeFromVerts(sys, i3, i2);
		}

		/* The new point is on the same edge in the u1 direction*/
	}
	else if (u1 >= u2 && u1 >= u3 && u1 >= u4) {
		copy_v3_v3(r, sys->co[i1]);
		return inde;
		/* The new point is on the same edge in the u2 direction*/
	}
	else if (u2 >= u1 && u2 >= u3 && u2 >= u4) {
		copy_v3_v3(r, sys->co[i2]);
		return inde;
	}
	return -2;
}

static int nextPointFlowLineInverse(float r[3], LaplacianSystem *sys, float q[3], int oldface, int inde)
{
	float v1[3], v2[3], dir[3], dq[3], res[3], r2[3];
	float u1, u2, u3, u4, minu;
	int i1, i2, i3, i4, ix;
	int newface, fs[2];
	int numv, *vidn;
	int i, iu, ie, isect;
	i1 = sys->edges[inde][0];
	i2 = sys->edges[inde][1];
	copy_v3_v3(v1, sys->co[i1]);
	copy_v3_v3(v2, sys->co[i2]);
	i3 = getDifferentVertexFaceEdge(sys, oldface, inde);
	u1 = sys->U_field[i1];
	u2 = sys->U_field[i2];
	u3 = sys->U_field[i3];
	copy_v2_v2_int(fs, sys->faces_edge[inde]);
	//getFacesAdjacentToEdge(fs, sys, inde);
	newface = fs[0] == oldface ? fs[1] : fs[0];
	i4 = getDifferentVertexFaceEdge(sys, newface, inde);
	u4 = sys->U_field[i4];

	/* The actual point on flow line correspond to a vertex in a mesh */
	if (equals_v3v3(q, v1) || equals_v3v3(q, v2)) {
		ix = equals_v3v3(q, v1) ? i1 : i2;
		numv = sys->ringv_map[ix].count;
		vidn = sys->ringf_map[ix].indices;
		iu = -1;
		minu = 1000000;
		for (i = 0; i < numv; i++) {
			if (vidn[i] != ix) {
				if (sys->U_field[ix] < sys->U_field[vidn[i]]) {
					if (minu > sys->U_field[vidn[i]]){
						iu = vidn[i];
						minu = sys->U_field[vidn[i]];
					}
				}
			}
		}
		/*Min U found*/
		if (iu == -1) {
			printf("/*Min U found*/\n");
			return -1;
		}

		ie = getEdgeFromVerts(sys, ix, iu);

		//getFacesAdjacentToEdge(fs, sys, ie);
		copy_v2_v2_int(fs, sys->faces_edge[ie]);
		i1 = ix;
		i2 = iu;
		i3 = getDifferentVertexFaceEdge(sys, fs[0], ie);
		i4 = getDifferentVertexFaceEdge(sys, fs[1], ie);
		u1 = sys->U_field[i1];
		u2 = sys->U_field[i2];
		u3 = sys->U_field[i3];
		u3 = sys->U_field[i4];

		/* the next point is the opposite vertex in the edge*/
		if (u2 <= u3 && u2 <= u4 && u1 <= u3 && u1 <= u4) {
			copy_v3_v3(r, sys->co[iu]);
			return ie;

			/* the next point is on face fs[0]*/
		}
		else if (u3 <= u4) {
			copy_v3_v3(dir, sys->gf1[fs[0]]);
			//projectGradientOnFace(dir, sys, sys->gf1, fs[0]);
			mul_v3_fl(dir, -100);
			add_v3_v3v3(dq, q, dir);
			isect = isect_line_line_v3(sys->co[i3], sys->co[i2], q, dq, res, r2);
			copy_v3_v3(r, res);
			return ie;
			/* the next point is on face fs[1]*/
		}
		else {
			copy_v3_v3(dir, sys->gf1[fs[1]]);
			//projectGradientOnFace(dir, sys, sys->gf1, fs[1]);
			mul_v3_fl(dir, -100);
			add_v3_v3v3(dq, q, dir);
			isect = isect_line_line_v3(sys->co[i4], sys->co[i2], q, dq, res, r2);
			copy_v3_v3(r, res);
			return ie;
		}

		/* There is simple intersection on new face adjacent to inde */
	}
	else if (u1 >= u3 && u2 >= u3) {
		//projectGradientOnFace(dir, sys, sys->gf1, newface);
		copy_v3_v3(dir, sys->gf1[newface]);
		mul_v3_fl(dir, -100);
		add_v3_v3v3(dq, q, dir);
		if (u1 <= u2) {
			isect = isect_line_line_v3(sys->co[i3], sys->co[i1], q, dq, res, r2);
			copy_v3_v3(r, res);
			return getEdgeFromVerts(sys, i3, i1);
		}
		else {
			isect = isect_line_line_v3(sys->co[i3], sys->co[i2], q, dq, res, r2);
			copy_v3_v3(r, res);
			return getEdgeFromVerts(sys, i3, i2);
		}

		/* The new point is on the same edge in the u1 direction*/
	}
	else if (u1 <= u2 && u1 <= u3 && u1 <= u4) {
		copy_v3_v3(r, sys->co[i1]);
		return inde;
		/* The new point is on the same edge in the u2 direction*/
	}
	else if (u2 <= u1 && u2 <= u3 && u2 <= u4) {
		copy_v3_v3(r, sys->co[i2]);
		return inde;
	}
	return -2;
}


static void computeGFLine(LaplacianSystem *sys, GradientFlowSystem *gfsys, GradientFlowVert *gfvert_seed)
{
	float seed[3], r[3], q[3];
	int idv = -1, fs[2], ied, oldface, oldedge;
	bool can_next;

	/* is valid vert*/
	if (gfvert_seed->ori_e == -1) return;

	/*Determine if is some vertice on otiginal mesh*/
	if (equals_v3v3(sys->co[sys->edges[gfvert_seed->ori_e][0]], gfvert_seed->co)){
		idv = sys->edges[gfvert_seed->ori_e][0];
	}
	else if (equals_v3v3(sys->co[sys->edges[gfvert_seed->ori_e][1]], gfvert_seed->co)){
		idv = sys->edges[gfvert_seed->ori_e][1];

	}
	
	/*Is a vert on segment of line*/
	if (idv == -1) {
		copy_v2_v2(fs, sys->faces_edge[gfvert_seed->ori_e]);
		can_next = true;
		copy_v3_v3(q, gfvert_seed->co);
		oldface = fs[0];
		oldedge = gfvert_seed->ori_e;
		while (can_next) {
			ied = nextPointFlowLine(r, sys, q, oldface, oldedge);
			if (ied >= 0) {
				/* */
			}
			else{
				can_next = false;
			}
		}
	}
	else{
		/*The vert is a vertice on original mesh*/

	}
	
	
	


	

}