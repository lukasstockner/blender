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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BKE_crazyspace.h
 *  \ingroup bke
 */

#ifndef __BKE_CRAZYSPACE_H__
#define __BKE_CRAZYSPACE_H__

#ifdef __cplusplus
extern "C" {
#endif
struct Scene;
struct Object;
struct BMEditMesh;
struct Mesh;

/* crazyspace.c */
float (*BKE_crazyspace_get_mapped_editverts(struct Scene *scene, struct Object *obedit))[3];
void BKE_crazyspace_set_quats_editmesh(
        struct BMEditMesh *em, float (*origcos)[3], float (*mappedcos)[3], float (*quats)[4],
        const bool use_select);
void BKE_crazyspace_set_quats_mesh(struct Mesh *me, float (*origcos)[3], float (*mappedcos)[3], float (*quats)[4]);
int BKE_sculpt_get_first_deform_matrices(struct Scene *scene, struct Object *ob, float (**deformmats)[3][3], float (**deformcos)[3]);
void BKE_crazyspace_build_sculpt(struct Scene *scene, struct Object *ob, float (**deformmats)[3][3], float (**deformcos)[3]);

/* Returns true if the object's derived cage vertex indeces can be assumed to be in sync to
* the editdata (base) vertex indeces */
bool BKE_crazyspace_cageindexes_in_sync(struct Object *ob);

/* Maps editmesh vertex indeces to derivedmesh cage vertex indces and returns the map.
* If returns NULL, it means that mapping failed for some reason (modifier failing to set CD_ORIGINDEX, etc).
* It is the caller's responsibility to free the returned array! */
int *BKE_crazyspace_map_em_to_cage(struct Object *ob, struct BMEditMesh *em, struct DerivedMesh *cage_dm);

/* Calculates editmesh active element selection center in global space on derived cage 
 * (used in calculating visual manipulator and transform constraint centers) */
void BKE_crazyspace_cage_active_sel_center(struct BMEditSelection *active_sel, struct DerivedMesh *cage,
											int *derived_index_map, float *cent);

#ifdef __cplusplus
}
#endif

#endif
