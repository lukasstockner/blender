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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_KEY_H__
#define __BKE_KEY_H__

/** \file BKE_key.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

typedef struct Object Object;
typedef struct Key Key;
typedef struct KeyBlock KeyBlock;
typedef struct ID ID;
typedef struct ListBase ListBase;
typedef struct Curve Curve;
typedef struct Scene Scene;
typedef struct Lattice Lattice;
typedef struct Mesh Mesh;
typedef struct WeightsArrayCache WeightsArrayCache;
typedef struct BMEditMesh BMEditMesh;
typedef struct ScratchKeyBlock ScratchKeyBlock;

/* Kernel prototypes */
#ifdef __cplusplus
extern "C" {
#endif

void        BKE_key_free(Key *sc);
void        BKE_key_free_nolib(Key *key);
Key		   *BKE_key_add(ID *id);
Key		   *BKE_key_copy(Key *key);
Key		   *BKE_key_copy_nolib(Key *key);
void        BKE_key_make_local(Key *key);
void        BKE_key_sort(Key *key);

void key_curve_position_weights(float t, float data[4], int type);
void key_curve_tangent_weights(float t, float data[4], int type);
void key_curve_normal_weights(float t, float data[4], int type);

float *BKE_key_evaluate_object_ex(Scene *scene, Object *ob, int *r_totelem,
                                  float *arr, size_t arr_size);
float *BKE_key_evaluate_object(Scene *scene, Object *ob, int *r_totelem);

Key      *BKE_key_from_object(Object *ob);
KeyBlock *BKE_keyblock_from_object(Object *ob);
KeyBlock *BKE_keyblock_from_object_reference(Object *ob);

KeyBlock *BKE_keyblock_add(Key *key, const char *name);
KeyBlock *BKE_keyblock_add_ctime(Key *key, const char *name, const bool do_force);
KeyBlock *BKE_keyblock_from_key(Key *key, int index);
KeyBlock *BKE_keyblock_find_name(Key *key, const char name[]);
void      BKE_keyblock_copy_settings(KeyBlock *kb_dst, const KeyBlock *kb_src);
char     *BKE_keyblock_curval_rnapath_get(Key *key, KeyBlock *kb);

/* returns a pointer to active shape value (mixval/anim-driven val) */
float    *BKE_keyblock_get_active_value(Key *key, KeyBlock *kb);

/* ==== scratch keyblock ==== */

/* performs a first-time setup of the scratch. if it's already inited 
 * (e. g. existed in a mainfile), does nothing. */
void BKE_key_init_scratch(Object *ob);

/* moves current edit data to and from the scratch shape */
void BKE_key_editdata_to_scratch(Object *ob, bool shapedata_indeces_in_sync);

/* populates the current editdata from scratch shapekey */
void BKE_key_editdata_from_scratch(Object *ob);

/* evaluates the current shape key situation and puts it into the editmesh coordinates */
void BKE_key_eval_editmesh_rel(BMEditMesh *edbm, bool pinned);

/* ========================= */

// needed for the GE
typedef struct WeightsArrayCache {
	int num_defgroup_weights;
	float **defgroup_weights;
} WeightsArrayCache;

float **BKE_keyblock_get_per_block_weights(Object *ob, Key *key, WeightsArrayCache *cache);
void BKE_keyblock_free_per_block_weights(Key *key, float **per_keyblock_weights, WeightsArrayCache *cache);
void BKE_key_evaluate_relative(const int start, int end, const int tot, char *basispoin, Key *key, KeyBlock *actkb,
                               float **per_keyblock_weights, const int mode);

/* conversion functions */
void    BKE_key_convert_to_mesh(KeyBlock *kb, Mesh *me);
void    BKE_key_convert_from_mesh(Mesh *me, KeyBlock *kb);
void    BKE_key_convert_to_lattice(KeyBlock *kb, Lattice *lt);
void    BKE_key_convert_from_lattice(Lattice *lt, KeyBlock *kb);
void    BKE_key_convert_to_curve(KeyBlock *kb, Curve  *cu, ListBase *nurb);
void    BKE_key_convert_from_curve(Curve *cu, KeyBlock *kb, ListBase *nurb);
float (*BKE_key_convert_to_vertcos(Object *ob, KeyBlock *kb))[3];
void    BKE_key_convert_from_vertcos(Object *ob, KeyBlock *kb, float (*vertCos)[3]);
void    BKE_key_convert_from_offset(Object *ob, KeyBlock *kb, float (*ofs)[3]);


/* other management */
/* moves a shape key to new_index. safe, clamps index to key->totkey, updates reference keys and 
 * the object's active shape index */
void	BKE_keyblock_move(Object *ob, KeyBlock *key_block, int new_index);

/* basic key math */
float	(*BKE_keyblock_math_deltas(Object *ob, KeyBlock *a, KeyBlock *basis))[3];
float	(*BKE_keyblock_math_deltas_mult(Object *ob, KeyBlock *a, KeyBlock *basis, float mult, float dists[]))[3];

void	BKE_keyblock_math_add(Object *ob, KeyBlock *r, KeyBlock *a, KeyBlock* basis, float mult);
void	BKE_keyblock_math_interp(Object *ob, KeyBlock *r, KeyBlock *a, float mult);


/* key.c */
extern int slurph_opt;

#ifdef __cplusplus
};
#endif

#endif  /* __BKE_KEY_H__ */
