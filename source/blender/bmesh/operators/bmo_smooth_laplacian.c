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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_smooth_laplacian.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_array.h"
#include "BLI_heap.h"
#include "BLI_math.h"
#include "BLI_smallhash.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_vertexsmoothlaplacian_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMVert *v;
	BMEdge *e;
	BLI_array_declare(cos);
	float (*cos)[3] = NULL;
	float *co, *co2, clipdist = BMO_slot_float_get(op, "clipdist");
	int i, j, clipx, clipy, clipz;
	
	clipx = BMO_slot_bool_get(op, "mirror_clip_x");
	clipy = BMO_slot_bool_get(op, "mirror_clip_y");
	clipz = BMO_slot_bool_get(op, "mirror_clip_z");

	i = 0;
	BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
		BLI_array_grow_one(cos);
		co = cos[i];
		
		j  = 0;
		BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
			co2 = BM_edge_other_vert(e, v)->co;
			add_v3_v3v3(co, co, co2);
			j += 1;
		}
		
		if (!j) {
			copy_v3_v3(co, v->co);
			i++;
			continue;
		}

		mul_v3_fl(co, 1.0f / (float)j);
		mid_v3_v3v3(co, co, v->co);

		if (clipx && fabsf(v->co[0]) <= clipdist)
			co[0] = 0.0f;
		if (clipy && fabsf(v->co[1]) <= clipdist)
			co[1] = 0.0f;
		if (clipz && fabsf(v->co[2]) <= clipdist)
			co[2] = 0.0f;

		i++;
	}

	i = 0;
	BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
		copy_v3_v3(v->co, cos[i]);
		i++;
	}

	BLI_array_free(cos);
}
