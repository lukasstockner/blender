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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/editmesh.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "BLI_math.h"

#include "BKE_editmesh.h"
#include "BKE_cdderivedmesh.h"


BMEditMesh *BKE_editmesh_create(BMesh *bm, const bool do_tessellate)
{
	BMEditMesh *em = MEM_callocN(sizeof(BMEditMesh), __func__);

	em->bm = bm;
	if (do_tessellate) {
		BKE_editmesh_tessface_calc(em);
	}

	return em;
}

BMEditMesh *BKE_editmesh_copy(BMEditMesh *em)
{
	BMEditMesh *em_copy = MEM_callocN(sizeof(BMEditMesh), __func__);
	*em_copy = *em;

	em_copy->derivedCage = em_copy->derivedFinal = NULL;

	em_copy->derivedVertColor = NULL;
	em_copy->derivedVertColorLen = 0;
	em_copy->derivedFaceColor = NULL;
	em_copy->derivedFaceColorLen = 0;

	em_copy->bm = BM_mesh_copy(em->bm);

	/* The tessellation is NOT calculated on the copy here,
	 * because currently all the callers of this function use
	 * it to make a backup copy of the BMEditMesh to restore
	 * it in the case of errors in an operation. For perf
	 * reasons, in that case it makes more sense to do the
	 * tessellation only when/if that copy ends up getting
	 * used.*/
	em_copy->looptris = NULL;

	return em_copy;
}

/**
 * \brief Return the BMEditMesh for a given object
 *
 * \note this function assumes this is a mesh object,
 * don't add NULL data check here. caller must do that
 */
BMEditMesh *BKE_editmesh_from_object(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);
	/* sanity check */
#ifndef NDEBUG
	if (((Mesh *)ob->data)->edit_btmesh) {
		BLI_assert(((Mesh *)ob->data)->edit_btmesh->ob == ob);
	}
#endif
	return ((Mesh *)ob->data)->edit_btmesh;
}


static void editmesh_tessface_calc_intern(BMEditMesh *em)
{
	/* allocating space before calculating the tessellation */


	BMesh *bm = em->bm;

	/* this assumes all faces can be scan-filled, which isn't always true,
	 * worst case we over alloc a little which is acceptable */
	const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
	const int looptris_tot_prev_alloc = em->looptris ? (MEM_allocN_len(em->looptris) / sizeof(*em->looptris)) : 0;

	BMLoop *(*looptris)[3];

#if 0
	/* note, we could be clever and re-use this array but would need to ensure
	 * its realloced at some point, for now just free it */
	if (em->looptris) MEM_freeN(em->looptris);

	/* Use em->tottri when set, this means no reallocs while transforming,
	 * (unless scanfill fails), otherwise... */
	/* allocate the length of totfaces, avoid many small reallocs,
	 * if all faces are tri's it will be correct, quads == 2x allocs */
	BLI_array_reserve(looptris, (em->tottri && em->tottri < bm->totface * 3) ? em->tottri : bm->totface);
#else

	/* this means no reallocs for quad dominant models, for */
	if ((em->looptris != NULL) &&
	    /* (*em->tottri >= looptris_tot)) */
	    /* check against alloc'd size incase we over alloc'd a little */
	    ((looptris_tot_prev_alloc >= looptris_tot) && (looptris_tot_prev_alloc <= looptris_tot * 2)))
	{
		looptris = em->looptris;
	}
	else {
		if (em->looptris) MEM_freeN(em->looptris);
		looptris = MEM_mallocN(sizeof(*looptris) * looptris_tot, __func__);
	}

#endif

	em->looptris = looptris;

	/* after allocating the em->looptris, we're ready to tessellate */
	BM_bmesh_calc_tessellation(em->bm, em->looptris, &em->tottri);

}

void BKE_editmesh_tessface_calc(BMEditMesh *em)
{
	editmesh_tessface_calc_intern(em);

	/* commented because editbmesh_build_data() ensures we get tessfaces */
#if 0
	if (em->derivedFinal && em->derivedFinal == em->derivedCage) {
		if (em->derivedFinal->recalcTessellation)
			em->derivedFinal->recalcTessellation(em->derivedFinal);
	}
	else if (em->derivedFinal) {
		if (em->derivedCage->recalcTessellation)
			em->derivedCage->recalcTessellation(em->derivedCage);
		if (em->derivedFinal->recalcTessellation)
			em->derivedFinal->recalcTessellation(em->derivedFinal);
	}
#endif
}

void BKE_editmesh_update_linked_customdata(BMEditMesh *em)
{
	BMesh *bm = em->bm;
	int act;

	if (CustomData_has_layer(&bm->pdata, CD_MTEXPOLY)) {
		act = CustomData_get_active_layer(&bm->pdata, CD_MTEXPOLY);
		CustomData_set_layer_active(&bm->ldata, CD_MLOOPUV, act);

		act = CustomData_get_render_layer(&bm->pdata, CD_MTEXPOLY);
		CustomData_set_layer_render(&bm->ldata, CD_MLOOPUV, act);

		act = CustomData_get_clone_layer(&bm->pdata, CD_MTEXPOLY);
		CustomData_set_layer_clone(&bm->ldata, CD_MLOOPUV, act);

		act = CustomData_get_stencil_layer(&bm->pdata, CD_MTEXPOLY);
		CustomData_set_layer_stencil(&bm->ldata, CD_MLOOPUV, act);
	}
}

void BKE_editmesh_free_derivedmesh(BMEditMesh *em)
{
	if (em->derivedCage) {
		em->derivedCage->needsFree = 1;
		em->derivedCage->release(em->derivedCage);
	}
	if (em->derivedFinal && em->derivedFinal != em->derivedCage) {
		em->derivedFinal->needsFree = 1;
		em->derivedFinal->release(em->derivedFinal);
	}

	em->derivedCage = em->derivedFinal = NULL;
}

/*does not free the BMEditMesh struct itself*/
void BKE_editmesh_free(BMEditMesh *em)
{
	BKE_editmesh_free_derivedmesh(em);

	BKE_editmesh_color_free(em);

	if (em->looptris) MEM_freeN(em->looptris);

	if (em->bm)
		BM_mesh_free(em->bm);
}

void BKE_editmesh_color_free(BMEditMesh *em)
{
	if (em->derivedVertColor) MEM_freeN(em->derivedVertColor);
	if (em->derivedFaceColor) MEM_freeN(em->derivedFaceColor);
	em->derivedVertColor = NULL;
	em->derivedFaceColor = NULL;

	em->derivedVertColorLen = 0;
	em->derivedFaceColorLen = 0;

}

void BKE_editmesh_color_ensure(BMEditMesh *em, const char htype)
{
	switch (htype) {
		case BM_VERT:
			if (em->derivedVertColorLen != em->bm->totvert) {
				BKE_editmesh_color_free(em);
				em->derivedVertColor = MEM_mallocN(sizeof(*em->derivedVertColor) * em->bm->totvert, __func__);
				em->derivedVertColorLen = em->bm->totvert;
			}
			break;
		case BM_FACE:
			if (em->derivedFaceColorLen != em->bm->totface) {
				BKE_editmesh_color_free(em);
				em->derivedFaceColor = MEM_mallocN(sizeof(*em->derivedFaceColor) * em->bm->totface, __func__);
				em->derivedFaceColorLen = em->bm->totface;
			}
			break;
		default:
			BLI_assert(0);
			break;
	}
}


/* ==================== topology hashing ======================= */

 unsigned int mm2_hash(char *key, unsigned int len, unsigned int seed) {
		const unsigned int m = 0x5bd1e995;
		char r = 24;
		unsigned int h = len + seed;
		for (; len >= 4; len -= 4, key += 4) {
			unsigned int k = *(unsigned int *) key * m;
			k ^= k >> r;
			k *= m;
			h = (h * m) ^ k;
		}

		switch (len) {
			/* everything fall-through */
			case 3: 
				h ^= key[2] << 16;
			case 2: 
				h ^= key[1] << 8;
			case 1: 
				h ^= key[0];
				h *= m;
			default: /* do nothing */;
		}
		h ^= h >> 13;
		h *= m;
		h ^= h >> 15;
		return h;
}


int hashfunc(void *data, int len, int oldhash)
{
	return (int) mm2_hash(data, len, oldhash);
}

int hashloop_topo(BMLoop *l, int oldhash)
{
	/* skip header, don't track customdata */
	return hashfunc(((char *)l) + sizeof(BMHeader), sizeof(BMLoop) - sizeof(BMHeader), oldhash);
}

int hashedge_topo(BMEdge *bme, int oldhash)
{
	return hashfunc(&bme->v1, sizeof(BMEdge) - sizeof(BMHeader) - sizeof(BMFlagLayer *), oldhash);
}

int hashface_topo(BMFace *f, int oldhash)
{
	/* skip header & flags & face normals and material */
	int a = f->len + (int)f->l_first;
	return hashfunc(&a, sizeof(int), oldhash);
}

int bmesh_topohash(BMesh *bm)
{
	BMIter iter;
	BMFace *f;
	BMVert *v;

	int hash = bm->totedge + bm->totvert + bm->totloop + bm->totface;

	if (!hash) {
		return 0;
	}

	if (bm->totedge == 0 && bm->totface == 0) {
		/* cloud mesh */
		return hashfunc(&bm->totvert, sizeof(int), hash);
	}

	BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
		BM_elem_flag_set(f, BM_ELEM_TAG, false);
	}

	/* todo -- have a deeper look onto how we can do this the fastest and safest way */
	BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
		if (v->e) {
			hash = hashedge_topo(v->e, hash);
			if (v->e->l && !BM_elem_flag_test_bool(v->e->l->f, BM_ELEM_TAG)) {
				hash = hashloop_topo(v->e->l, hash);
				hash = hashface_topo(v->e->l->f, hash);
				BM_elem_flag_set(v->e->l->f, BM_ELEM_TAG, true);
			}
		}

	}

	BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
		BM_elem_flag_set(f, BM_ELEM_TAG, false);
	}

	return hash;
}

void BKE_editmesh_topochange_calc(BMEditMesh *em)
{
	em->topochange.topohash = bmesh_topohash(em->bm);
	memcpy(&em->topochange.totvert, &em->bm->totvert, sizeof(int) * 4);
}


bool BKE_editmesh_topo_has_changed(BMEditMesh *em)
{
	/* fast way to compare em->bm->totvert/totface/totetc with topochange */
	if (!memcmp(&em->bm->totvert, &em->topochange.totvert, sizeof(int) * 4)) {
		int hash = bmesh_topohash(em->bm);
		if (hash == em->topochange.topohash)
			return false;
		else return true;
	}
	return true;
}
