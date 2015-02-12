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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __BKE_PTEX_H__
#define __BKE_PTEX_H__

#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"

struct DerivedMesh;
struct GSet;
struct Image;
struct Mesh;
struct Object;

void BKE_loop_ptex_init(MLoopPtex *loop_ptex,
						const MPtexTexelInfo texel_info,
						const MPtexLogRes log_res);

void BKE_loop_ptex_pattern_fill(MLoopPtex *loop_ptex, int index);

struct Image *
BKE_ptex_mesh_image_get(struct Object *ob,
						const char layer_name[MAX_CUSTOMDATA_LAYER_NAME]);

/* Free contents of loop_ptex, but not loop_ptex itself */
void BKE_loop_ptex_free(MLoopPtex *loop_ptex);

size_t BKE_loop_ptex_rect_num_bytes(const MLoopPtex *loop_ptex);

size_t BKE_ptex_rect_num_bytes(const MPtexTexelInfo texel_info,
							   const MPtexLogRes res);

size_t BKE_ptex_bytes_per_texel(const MPtexTexelInfo texel_info);

/* SDNA uses built-in type, these just convert to enum member */
MPtexDataType BKE_ptex_texel_data_type(const MPtexTexelInfo texel_info);
MPtexDataType BKE_loop_ptex_texel_data_type(const MLoopPtex *loop_ptex);

/* Resize loop_ptex to the new logres
 *
 * Return true if successful, false otherwise. */
bool BKE_loop_ptex_resize(MLoopPtex *loop_ptex, const MPtexLogRes logres);

/* Copy regions from the image into loop_ptex
 *
 * TODO(nicholasbishop): should have some way to set which loops need
 * updates, probably new flag in MLoopPtex
 *
 * Return true if successful (even if some loops have an error), false
 * otherwise. */
bool BKE_ptex_update_from_image(MLoopPtex *loop_ptex, const int totloop);

/* Add CD_LOOP_PTEX layer to DerivedMesh. The 'id' field will be just
 * the sequential loop indices, TODO */
void BKE_ptex_derived_mesh_inject(struct DerivedMesh *dm);

struct DerivedMesh *BKE_ptex_derived_mesh_subdivide(struct DerivedMesh *dm);

/* TODO */
/* CustomData layer, not saved in file */
/* TODO, should have different name now */
typedef struct {
	float uv[2];
	int id;
} MLoopInterp;

void BKE_ptex_derived_mesh_interp_coords(struct DerivedMesh *dm);

void BKE_ptex_tess_face_interp(MTessFacePtex *tess_face_ptex,
							   const MLoopInterp *loop_interp,
							   const unsigned int *loop_indices,
							   const int num_loop_indices);

/* Convert texel resolution to MPtexLogRes. Inputs must be
 * powers-of-two and within the valid range. Return true if
 * successful, false otherwise. */
bool BKE_ptex_log_res_from_res(MPtexLogRes *logres, const int u, const int v);

bool BKE_ptex_texel_info_init(MPtexTexelInfo *texel_info,
							  const MPtexDataType data_type,
							  const int num_channels);

bool BKE_ptex_import(struct Mesh *me, const char filepath[]);

void BKE_ptex_image_mark_for_update(struct Mesh *me, const int layer_offset);

/* Update borders for the set of BPXRects in rects
 *
 * Return true if successful, false otherwise */
bool BKE_ptex_filter_borders_update(struct Image *image, struct GSet *rects);

#endif
