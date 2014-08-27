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

/** \file blender/modifiers/intern/MOD_quadremesh_geom.h
 *  \ingroup modifiers
 */

#ifndef __MOD_QUADREMESH_GEOM_H__
#define __MOD_QUADREMESH_GEOM_H__

#define MOD_QUADREMESH_ALLOC_BLOCK 256
#define MOD_QUADREMESH_MIN_LEN 0.000001f

#ifdef WITH_OPENNL

#include "ONL_opennl.h"

#include "BKE_mesh_mapping.h"
#include "BLI_heap.h"

typedef struct GFList {
	int index;		/*Index pointer to face or edge on original mesh*/
	struct GFList *next;
} GFList;

typedef struct GradientFlowVert {
	float co[3];		/*Vert coordinates*/
	int ori_e;	/*Edge index on original Mesh -1 if not inside a edge*/
} GradientFlowVert;

typedef struct GradientFlowEdge {
	int v1, v2;	/*Vert indices on gradient flow mesh*/
	int ori_f;		/*Face index on original Mesh -1 if not inside a face*/
} GradientFlowEdge;

typedef struct GradientFlowMesh {
	int totvert, allocvert;
	int totedge, allocedge;
	GradientFlowVert *mvert;	/* array of verts */
	GradientFlowEdge *medge;	/* array of edges */
} GradientFlowMesh;

/*GradientFlowSysten, one gfsys for every gradient field*/
typedef struct GradientFlowSystem {
	GradientFlowMesh *mesh;			/* Mesh pointer*/
	GFList **ringf_list;			/* Array list of of GradientFlowEdge per original face*/
	GFList **ringe_list;			/* Array list of of GradientFlowVert per original edge*/
	struct Heap *heap_seeds;
	int totalf;
	int totale;
	float *hfunction;
	float(*gfield)[3];				/* Gradient Field g1 */
} GradientFlowSystem;

typedef struct LaplacianSystem {
	bool command_compute_flow;
	bool has_solution;
	bool command_remesh;
	int total_verts;
	int total_edges;
	int total_faces;
	int total_features;
	int total_gflines;
	int total_gfverts;
	char features_grp_name[64];		/* Vertex Group name */
	float(*co)[3];					/* Original vertex coordinates */
	float(*cogfl)[3];				/* Vertex coordinate Gradient flow line */
	float(*no)[3];					/* Original face normal */
	float(*gf1)[3];					/* Gradient Field g1 */
	float(*gf2)[3];					/* Gradient Field g2 */
	float *weights;					/* Feature points weights*/
	float *U_field;					/* Initial scalar field*/
	float *h1;						/* Sampling distance function h1*/
	float *h2;						/* Sampling distance function h2*/
	float h;
	int *constraints;				/* Feature points constraints*/
	int *ringf_indices;				/* Indices of faces per vertex */
	int *ringv_indices;				/* Indices of neighbors(vertex) per vertex */
	int *ringe_indices;				/* Indices of edges per vertex */
	unsigned int(*faces)[4];		/* Copy of MFace (tessface) v1-v4 */
	unsigned int(*edges)[2];		/* Copy of edges v1-v2 */
	unsigned int(*faces_edge)[2];	/* Faces by edges  */
	MeshElemMap *ringf_map;			/* Map of faces per vertex */
	MeshElemMap *ringv_map;			/* Map of vertex per vertex */
	MeshElemMap *ringe_map;			/* Map of edges per vertex */
	NLContext *context;				/* System for solve general implicit rotations */
	GradientFlowSystem *gfsys;
} LaplacianSystem;

GFList *newGFList(int value);
void deleteGFList(GFList *l);
void addNodeGFList(GFList *l, int value);
int getSizeGFList(GFList *l);

/*
* alpha is degree of anisotropic curvature sensitivity
* h is the desired distance
* return ve[0] number of vertices
* return ve[1] number of edges
*/
void estimateNumberGFVerticesEdges(int ve[2], LaplacianSystem *sys, float h);

GradientFlowMesh *newGradientFlowMesh(int totalvert, int totaledge);
void deleteGradientFlowMesh(GradientFlowMesh * gfmesh);
int addGFVertGFMesh(GradientFlowMesh *gfmesh, GradientFlowVert gfvert);
int addVertGFMesh(GradientFlowMesh *gfmesh, float co[3], int index_edge);
int addGFEdgeGFMesh(GradientFlowMesh *gfmesh, GradientFlowEdge gfedge);
int addEdgeGFMesh(GradientFlowMesh *gfmesh, int index_v1, int index_v2, int index_face);

GradientFlowSystem *newGradientFlowSystem(LaplacianSystem *sys, float *mhfunction, float(*mgfield)[3]);
void deleteGradientFlowSystem(GradientFlowSystem *gfsys);
int addGFVertGFSystem(GradientFlowSystem *gfsys, GradientFlowVert gfvert);
int addVertGFSystem(GradientFlowSystem *gfsys, float co[3], int index_edge);
int addGFEdgeGFSystem(GradientFlowSystem *gfsys, GradientFlowEdge gfedge);
int addEdgeGFSystem(GradientFlowSystem *gfsys, int index_v1, int index_v2, int index_face);
int addEdgeTwoFacesGFSystem(GradientFlowSystem *gfsys, int index_v1, int index_v2, int index_face1, int index_face2);

int *findFeaturesOnMesh(int size[2], LaplacianSystem *sys);
void addSeedToQueue(struct Heap *aheap, float value, GradientFlowVert *vert);
GradientFlowVert *getTopSeedFromQueue(struct Heap *aheap);

bool isOnSegmentLine(float p1[3], float p2[3], float q[3]);
bool intersecionLineSegmentWithVector(float r[3], float p1[3], float p2[3], float ori[3], float dir[3]);
int getEdgeFromVerts(LaplacianSystem *sys, int v1, int v2);
int getOtherFaceAdjacentToEdge(LaplacianSystem *sys, int oldface, int inde);
void projectVectorOnFace(float r[3], float no[3], float dir[3]);
int getDifferentVertexFaceEdge(LaplacianSystem *sys, int oldface, int inde);
#define GRA_DIR_ON_NONE 0
#define GRA_DIR_ON_FACE 1
#define GRA_DIR_ON_EDGE 2
void computeGradientDirectionOnVert(int rind[2], float r[3], LaplacianSystem *sys, GradientFlowSystem *gfsys, int indexvert);
void computeGradientDirectionOnEdgeInverse(int rind[2], float r[3], LaplacianSystem *sys, GradientFlowSystem *gfsys, int indexedge);
int nearGFEdgeInGFMesh(LaplacianSystem *sys, GradientFlowSystem *gfsys, float ori[3], float dir[3], int indexface, float maxradius);
int nearGFEdgeInGFMeshFromEdge(LaplacianSystem *sys, GradientFlowSystem *gfsys, float ori[3], float dir[3], int indexedge, float maxradius);
int nextPointFlowLine(float r[3], LaplacianSystem *sys, float q[3], int oldface, int inde);
int nextPointFlowLineInverse(float r[3], LaplacianSystem *sys, float q[3], int oldface, int inde);
float getSamplingDistanceFunctionOnFace(LaplacianSystem *sys, GradientFlowSystem *gfsys, int indexface);
float getMaxSamplingDistanceFunctionOnFace(LaplacianSystem *sys, GradientFlowSystem *gfsys, int indexface);
void computeGFLine(LaplacianSystem *sys, GradientFlowSystem *gfsys, GradientFlowVert *gfvert_seed);

int computeNewSeed(float r[3], LaplacianSystem *sys, GradientFlowSystem *gfsys, int indexf, float ori[3], float dir[3], float mh);

void computeFlowLines(LaplacianSystem *sys);

#endif /*openNl*/
#endif /*__MOD_QUADREMESH_GEOM_H__*/