/**
 * A BVH for high poly meshes.
 * 
 * $Id$
 *
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLI_PBVH_H
#define BLI_PBVH_H

#include <assert.h>

struct BoundBox;
struct CustomData;
struct MFace;
struct MVert;
struct GridKey;
struct GridToFace;
struct DMGridAdjacency;
struct DMGridData;
struct PBVH;
struct PBVHNode;
struct ListBase;

typedef struct PBVH PBVH;
typedef struct PBVHNode PBVHNode;

typedef struct PBVHHiddenArea {
	struct PBVHHiddenArea *next, *prev;
	float clip_planes[4][4];
	int hide_inside;
} HiddenArea;

typedef struct {
	float (*co)[3];
} PBVHProxyNode;

/* Callbacks */

/* returns 1 if the search should continue from this node, 0 otherwise */
typedef int (*BLI_pbvh_SearchCallback)(PBVHNode *node, void *data);

typedef void (*BLI_pbvh_HitCallback)(PBVHNode *node, void *data);
typedef void (*BLI_pbvh_HitOccludedCallback)(PBVHNode *node, void *data, float* tmin);

/* test AABB against sphere */
typedef struct {
	float *center;
	float radius_squared;
	int original;
} PBVHSearchSphereData;
int BLI_pbvh_search_sphere_cb(PBVHNode *node, void *data);

/* Building */
#define PBVH_DEFAULT_LEAF_LIMIT 10000
PBVH *BLI_pbvh_new(int leaf_limit);
void BLI_pbvh_build_mesh(PBVH *bvh, struct MFace *faces, struct MVert *verts,
			 struct CustomData *vdata, struct CustomData *fdata,
			 int totface, int totvert, ListBase *hidden_areas);
void BLI_pbvh_build_grids(PBVH *bvh, struct DMGridData **grids,
			  struct DMGridAdjacency *gridadj, int totgrid,
			  int gridsize, struct GridKey *gridkey, void **gridfaces,
			  struct GridToFace *grid_face_map,
			  struct CustomData *vdata, struct CustomData *fdata,
			  ListBase *hidden_areas);
void BLI_pbvh_free(PBVH *bvh);

/* Hierarchical Search in the BVH, two methods:
   * for each hit calling a callback
   * gather nodes in an array (easy to multithread) */

void BLI_pbvh_search_callback(PBVH *bvh,
	BLI_pbvh_SearchCallback scb, void *search_data,
	BLI_pbvh_HitCallback hcb, void *hit_data);

void BLI_pbvh_search_gather(PBVH *bvh,
	BLI_pbvh_SearchCallback scb, void *search_data,
	PBVHNode ***array, int *tot);

/* Raycast
   the hit callback is called for all leaf nodes intersecting the ray;
   it's up to the callback to find the primitive within the leaves that is
   hit first */

void BLI_pbvh_raycast(PBVH *bvh, BLI_pbvh_HitOccludedCallback cb, void *data,
			  float ray_start[3], float ray_normal[3], int original);
int BLI_pbvh_node_raycast(PBVH *bvh, PBVHNode *node, float (*origco)[3],
	float ray_start[3], float ray_normal[3], float *dist,
	int *hit_index, int *grid_hit_index);

/* Drawing */

void BLI_pbvh_node_draw(PBVHNode *node, void *data);
int BLI_pbvh_node_planes_contain_AABB(PBVHNode *node, void *data);
void BLI_pbvh_draw(PBVH *bvh, float (*planes)[4], float (*face_nors)[3], int flags);

/* Node Access */

typedef enum {
	PBVH_Leaf = 1,

	PBVH_UpdateNormals = 2,
	PBVH_UpdateBB = 4,
	PBVH_UpdateOriginalBB = 8,

	/* Update vertex data (coord + normal */
	PBVH_UpdateVertBuffers = 16,
	
	/* Update color data (used for masks) */
	PBVH_UpdateColorBuffers = 32,

	PBVH_UpdateRedraw = 64,

	PBVH_UpdateAll = PBVH_UpdateNormals |
	                 PBVH_UpdateBB |
	                 PBVH_UpdateOriginalBB |
	                 PBVH_UpdateVertBuffers |
	                 PBVH_UpdateColorBuffers |
	                 PBVH_UpdateRedraw,

	PBVH_NeedsColorStitch = 128
} PBVHNodeFlags;

void BLI_pbvh_node_mark_update(PBVHNode *node);
void BLI_pbvh_node_set_flags(PBVHNode *node, void *data);

/* returns true if the pbvh is using grids rather than faces */
int BLI_pbvh_uses_grids(PBVH *bvh);

void BLI_pbvh_get_customdata(PBVH *pbvh, struct CustomData **vdata, struct CustomData **fdata);
struct GridToFace *BLI_pbvh_get_grid_face_map(PBVH *pbvh);

void BLI_pbvh_node_get_faces(PBVH *bvh, PBVHNode *node,
			     struct MFace **faces,
			     int **face_indices, int **face_vert_indices,
			     int *totface);
void BLI_pbvh_node_get_grids(PBVH *bvh, PBVHNode *node,
	int **grid_indices, int *totgrid, int *maxgrid, int *gridsize,
	struct DMGridData ***griddata, struct DMGridAdjacency **gridadj,
	struct GridKey **gridkey);
void BLI_pbvh_node_num_verts(PBVH *bvh, PBVHNode *node,
	int *uniquevert, int *totvert);
void BLI_pbvh_node_get_verts(PBVH *bvh, PBVHNode *node,
			     int **vert_indices, struct MVert **verts);

void BLI_pbvh_node_get_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);
void BLI_pbvh_node_get_original_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);

float BLI_pbvh_node_get_tmin(PBVHNode* node);

/* Update Normals/Bounding Box/Draw Buffers/Redraw and clear flags */

void BLI_pbvh_update(PBVH *bvh, int flags, float (*face_nors)[3]);
void BLI_pbvh_redraw_BB(PBVH *bvh, float bb_min[3], float bb_max[3]);
void BLI_pbvh_get_grid_updates(PBVH *bvh, int clear, void ***gridfaces, int *totface);
void BLI_pbvh_grids_update(PBVH *bvh, struct DMGridData **grids,
			   struct DMGridAdjacency *gridadj, void **gridfaces, struct GridKey *gridkey);

/* vertex deformer */
float (*BLI_pbvh_get_vertCos(struct PBVH *pbvh))[3];
void BLI_pbvh_apply_vertCos(struct PBVH *pbvh, float (*vertCos)[3]);
int BLI_pbvh_isDeformed(struct PBVH *pbvh);


/* Vertex Iterator */

/* this iterator has quite a lot of code, but it's designed to:
   - allow the compiler to eliminate dead code and variables
   - spend most of the time in the relatively simple inner loop */

#define PBVH_ITER_ALL		0
#define PBVH_ITER_UNIQUE	1

typedef struct PBVHVertexIter {
	/* iteration */
	int g;
	int width;
	int height;
	int skip;
	int gx;
	int gy;
	int i;

	/* grid */
	struct DMGridData **grids;
	struct DMGridData *grid, *elem;
	int *grid_indices;
	int totgrid;
	int gridsize;
	struct GridKey *gridkey;

	/* mesh */
	struct MVert *mverts;
	int totvert;
	int *vert_indices;

	/* mask layers */
	struct CustomData *vdata;
	int pmask_first_layer, pmask_layer_count, pmask_active_layer;

	/* result: these are all computed in the macro, but we assume
	   that compiler optimizations will skip the ones we don't use */
	struct MVert *mvert;
	float *co;
	short *no;
	float *fno;
	float *mask_active;

	float mask_combined; /* not editable */
} PBVHVertexIter;

#ifdef _MSC_VER
#pragma warning (disable:4127) // conditional expression is constant
#endif

#define BLI_pbvh_vertex_iter_begin(bvh, node, vi, mode) \
	{ \
		struct DMGridData **grids; \
		struct MVert *verts; \
		int *grid_indices, totgrid, gridsize, *vert_indices, uniq_verts, totvert; \
		struct GridKey *gridkey; \
		\
		memset(&vi, 0, sizeof(PBVHVertexIter)); \
		\
		BLI_pbvh_node_get_grids(bvh, node, &grid_indices, &totgrid, NULL, &gridsize, &grids, NULL, &gridkey); \
		BLI_pbvh_node_num_verts(bvh, node, &uniq_verts, &totvert); \
		BLI_pbvh_node_get_verts(bvh, node, &vert_indices, &verts); \
		BLI_pbvh_get_customdata(bvh, &vi.vdata, NULL); \
		\
		vi.grids= grids; \
		vi.grid_indices= grid_indices; \
		vi.totgrid= (grids)? totgrid: 1; \
		vi.gridsize= gridsize; \
		vi.gridkey= gridkey; \
		\
		if(mode == PBVH_ITER_ALL) \
			vi.totvert = totvert; \
		else \
			vi.totvert= uniq_verts; \
		vi.vert_indices= vert_indices; \
		vi.mverts= verts; \
		vi.mask_active= NULL; \
		assert(!gridkey || gridkey->mask == 0 || vi.vdata); \
		vi.pmask_layer_count = CustomData_number_of_layers(vi.vdata, CD_PAINTMASK); \
		assert(!gridkey || gridkey->mask == 0 || gridkey->mask == vi.pmask_layer_count); \
		if(vi.pmask_layer_count) { \
			vi.pmask_first_layer = CustomData_get_layer_index(vi.vdata, CD_PAINTMASK); \
			vi.pmask_active_layer = CustomData_get_active_layer_index(vi.vdata, CD_PAINTMASK); \
			if(vi.pmask_active_layer != -1 && !(vi.vdata->layers[vi.pmask_active_layer].flag & CD_FLAG_ENABLED)) \
				vi.pmask_active_layer = -1; \
		} \
	}\
	\
	for(vi.i=0, vi.g=0; vi.g<vi.totgrid; vi.g++) { \
		if(vi.grids) { \
			vi.width= vi.gridsize; \
			vi.height= vi.gridsize; \
			vi.grid= vi.grids[vi.grid_indices[vi.g]]; \
			vi.skip= 0; \
			 \
			/*if(mode == PVBH_ITER_UNIQUE) { \
				vi.grid += subm->grid.offset; \
				vi.skip= subm->grid.skip; \
				vi.grid -= skip; \
			}*/ \
		} \
		else { \
			vi.width= vi.totvert; \
			vi.height= 1; \
		} \
		 \
		for(vi.gy=0; vi.gy<vi.height; vi.gy++) { \
			if(vi.grid) GRIDELEM_INC(vi.grid, vi.skip, vi.gridkey); \
			\
			for(vi.gx=0; vi.gx<vi.width; vi.gx++, vi.i++) { \
				if(vi.grid) { \
					vi.co= GRIDELEM_CO(vi.grid, vi.gridkey); \
					vi.fno= GRIDELEM_NO(vi.grid, vi.gridkey); \
					\
					if(vi.gridkey->mask) { \
						vi.mask_combined = \
							paint_mask_from_gridelem(vi.grid, vi.gridkey, vi.vdata); \
						\
						if(vi.pmask_active_layer != -1) \
							vi.mask_active= &GRIDELEM_MASK(vi.grid, \
										       vi.gridkey)[vi.pmask_active_layer - \
												   vi.pmask_first_layer]; \
					} \
					\
					vi.elem= vi.grid; \
					GRIDELEM_INC(vi.grid, 1, vi.gridkey); \
				} \
				else { \
					vi.mvert= &vi.mverts[vi.vert_indices[vi.gx]]; \
					vi.co= vi.mvert->co; \
					vi.no= vi.mvert->no; \
					if(vi.pmask_layer_count) { \
						vi.mask_combined = \
							paint_mask_from_vertex(vi.vdata, vi.vert_indices[vi.gx], \
									       vi.pmask_layer_count, \
									       vi.pmask_first_layer); \
						\
						if(vi.pmask_active_layer != -1) \
							vi.mask_active = &((float*)vi.vdata->layers[vi.pmask_active_layer].data)[vi.vert_indices[vi.gx]]; \
					} \
				} \

#define BLI_pbvh_vertex_iter_end \
			} \
		} \
	} \

void BLI_pbvh_node_get_proxies(PBVHNode* node, PBVHProxyNode** proxies, int* proxy_count);
void BLI_pbvh_node_free_proxies(PBVHNode* node);
PBVHProxyNode* BLI_pbvh_node_add_proxy(PBVH* bvh, PBVHNode* node);
void BLI_pbvh_gather_proxies(PBVH* pbvh, PBVHNode*** nodes,  int* totnode);

//void BLI_pbvh_node_BB_reset(PBVHNode* node);
//void BLI_pbvh_node_BB_expand(PBVHNode* node, float co[3]);

#endif /* BLI_PBVH_H */

