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
* Contributor(s): Grigory Revzin.
*
* ***** END GPL LICENSE BLOCK *****
*/

#include <float.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "MEM_guardedalloc.h"

#include "bmesh.h"


static bool vtx_dist_add(BMVert *v, BMVert *v_other,
	float *dists, const float *dists_prev,
	float loc_to_world_mtx[3][3])
{
	if ((BM_elem_flag_test(v_other, BM_ELEM_SELECT) == 0) &&
		(BM_elem_flag_test(v_other, BM_ELEM_HIDDEN) == 0))
	{
		const int i = BM_elem_index_get(v);
		const int i_other = BM_elem_index_get(v_other);
		float vec[3];
		float dist_other;
		sub_v3_v3v3(vec, v->co, v_other->co);
		mul_m3_v3(loc_to_world_mtx, vec);

		dist_other = dists_prev[i] + len_v3(vec);
		if (dist_other < dists[i_other]) {
			dists[i_other] = dist_other;
			return true;
		}
	}
	return false;
}


void BM_prop_dist_calc_connected(BMesh *bm, float loc_to_world_mtx[3][3], float *dists)
{
	/* need to be very careful of feedback loops here, store previous dist's to avoid feedback */
	float *dists_prev = MEM_mallocN(bm->totvert * sizeof(float), __func__);

	BLI_LINKSTACK_DECLARE(queue, BMVert *);

	/* any BM_ELEM_TAG'd vertex is in 'queue_next', so we don't add in twice */
	BLI_LINKSTACK_DECLARE(queue_next, BMVert *);

	BLI_LINKSTACK_INIT(queue);
	BLI_LINKSTACK_INIT(queue_next);

	{
		BMIter viter;
		BMVert *v;
		int i;

		BM_ITER_MESH_INDEX(v, &viter, bm, BM_VERTS_OF_MESH, i) {
			BM_elem_index_set(v, i); /* set_inline */
			BM_elem_flag_disable(v, BM_ELEM_TAG);

			if (BM_elem_flag_test(v, BM_ELEM_SELECT) == 0 || BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
				dists[i] = FLT_MAX;
			}
			else {
				BLI_LINKSTACK_PUSH(queue, v);
				dists[i] = 0.0f;
			}
		}
	}

	do {
		BMVert *v;
		LinkNode *lnk;

		memcpy(dists_prev, dists, sizeof(float) * bm->totvert);

		while ((v = BLI_LINKSTACK_POP(queue))) {
			BMIter iter;
			BMEdge *e;
			BMLoop *l;

			/* connected edge-verts */
			BM_ITER_ELEM(e, &iter, v, BM_EDGES_OF_VERT) {
				if (BM_elem_flag_test(e, BM_ELEM_HIDDEN) == 0) {
					BMVert *v_other = BM_edge_other_vert(e, v);
					/* */
					if (vtx_dist_add(v, v_other, dists, dists_prev, loc_to_world_mtx)) {
						if (BM_elem_flag_test(v_other, BM_ELEM_TAG) == 0) {
							BM_elem_flag_enable(v_other, BM_ELEM_TAG);
							BLI_LINKSTACK_PUSH(queue_next, v_other);
						}
					}
				}
			}

			/* connected face-verts (excluding adjacent verts) */
			BM_ITER_ELEM(l, &iter, v, BM_LOOPS_OF_VERT) {
				if ((BM_elem_flag_test(l->f, BM_ELEM_HIDDEN) == 0) && (l->f->len > 3)) {
					BMLoop *l_end = l->prev;
					l = l->next->next;
					do {
						BMVert *v_other = l->v;
						if (vtx_dist_add(v, v_other, dists, dists_prev, loc_to_world_mtx)) {
							if (BM_elem_flag_test(v_other, BM_ELEM_TAG) == 0) {
								BM_elem_flag_enable(v_other, BM_ELEM_TAG);
								BLI_LINKSTACK_PUSH(queue_next, v_other);
							}
						}
					} while ((l = l->next) != l_end);
				}
			}
		}

		/* clear for the next loop */
		for (lnk = queue_next; lnk; lnk = lnk->next) {
			BM_elem_flag_disable((BMVert *)lnk->link, BM_ELEM_TAG);
		}

		BLI_LINKSTACK_SWAP(queue, queue_next);

		/* none should be tagged now since 'queue_next' is empty */
		BLI_assert(BM_iter_mesh_count_flag(BM_VERTS_OF_MESH, bm, BM_ELEM_TAG, true) == 0);

	} while (BLI_LINKSTACK_SIZE(queue));

	BLI_LINKSTACK_FREE(queue);
	BLI_LINKSTACK_FREE(queue_next);

	MEM_freeN(dists_prev);
}

void move_selected_verts_to_top(BMesh *bm, int *indexes)
{
	BMVert *v;
	BMIter viter;
	int a, b, ix;

	b = bm->totvert - 1;
	a = 0;

	BM_ITER_MESH(v, &viter, bm, BM_VERTS_OF_MESH) {
		/* if the vert is selected, get its index to top, otherwise to bottom */
		ix = BM_elem_index_get(v);
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			indexes[a] = ix;
			++a;
		}
		else {
			indexes[b] = ix;
			--b;
		}
	}
}

float dist_proj_sq(float a[3], float b[3], float loc_to_world_mtx[3][3], float proj_plane_n[3])
{
	float v[3], v1[3];
	sub_v3_v3v3(v, a, b);
	mul_m3_v3(loc_to_world_mtx, v);
	project_v3_v3v3(v1, v, proj_plane_n);
	sub_v3_v3(v, v1);
	return len_squared_v3(v);
}

float dist_sq(float a[3], float b[3])
{
	float v[3];
	sub_v3_v3v3(v, a, b);
	return len_squared_v3(v);
}

float dist_transform(float a[3], float b[3], float loc_to_world_mtx[3][3]) 
{
	float v[3];
	sub_v3_v3v3(v, a, b);
	mul_v3_m3v3(v, loc_to_world_mtx, v);
	return len_v3(v);
}


void BM_prop_dist_calc(BMesh *bm, float loc_to_world_mtx[3][3], float proj_plane_n[3], float dists[])
{
	int a, b;
	BMVert *unsel_vert, *sel_vert, *decision_vert;
	float dist, dist_max;
	int *vindexes;

	if (bm->totvertsel == bm->totvert) {
		memset(dists, 0.0f, sizeof(float) * bm->totvert);
		return;
	}

	vindexes = MEM_mallocN(sizeof(int) * bm->totvert, __func__);

	memset(dists, FLT_MAX, sizeof(float) * bm->totvert);
	
	move_selected_verts_to_top(bm, vindexes);
	/* we have to loop over all vertices for each vertex, ahh n^2 
	 * to counter this, we are trying to reduce the loop count by stopping once we are through the selected vertices 
	 */

	/* some dupli code, but it saves from checking the proj_plane_n on every loop */
	if (proj_plane_n) {
		for (a = bm->totvertsel; a < bm->totvert; ++a) {	
			unsel_vert = BM_vert_at_index(bm, vindexes[a]);

			if (BM_elem_flag_test_bool(unsel_vert, BM_ELEM_HIDDEN))
				continue;

			dist_max = FLT_MAX;

			for (b = 0; b < bm->totvertsel; ++b) {
				sel_vert = BM_vert_at_index(bm, vindexes[b]);
				dist = dist_proj_sq(sel_vert->co, unsel_vert->co, loc_to_world_mtx, proj_plane_n);
				if (dist <= dist_max) {
					dist_max = dist;
				}
			}

			dists[vindexes[a]] = sqrtf(dist_max);
		}
	}
	else {
		for (a = bm->totvertsel; a < bm->totvert; ++a) {
			/* all unselected verts are at the end now */
			unsel_vert = BM_vert_at_index(bm, vindexes[a]);

			if (BM_elem_flag_test_bool(unsel_vert, BM_ELEM_HIDDEN))
				continue;

			dist_max = FLT_MAX;

			for (b = 0; b < bm->totvertsel; ++b) {
				/* all selected verts are at the beginning */
				sel_vert = BM_vert_at_index(bm, vindexes[b]);
				/* can use the localspace mesh here - linear transforms never change the relationships between distances - 
				 *  why do mul_m3_v3 if we don't have to? */
				dist = dist_sq(sel_vert->co, unsel_vert->co);

				if (dist <= dist_max) {
					dist_max = dist;
					decision_vert = BM_vert_at_index(bm, vindexes[b]);
				}
			}
			dists[vindexes[a]] = dist_transform(decision_vert->co, unsel_vert->co, loc_to_world_mtx);
		}
	}
	
	/* set distance to selected vertices to zero */
	for (b = 0; b < bm->totvertsel; ++b)
		dists[vindexes[b]] = 0.0f;

	MEM_freeN(vindexes);
}
