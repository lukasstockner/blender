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
#include "BLI_math_geom.h"
#include "BLI_smallhash.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"

#include "bmesh.h"

#include "ONL_opennl.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define SMOOTH_LAPLACIAN_AREA_FACTOR 4.0f

static float cotan_weight(float *v1, float *v2, float *v3);
void compute_weights_in_ring(BMVert *v, float lambda);

void bmo_vertexsmoothlaplacian_exec(BMesh *bm, BMOperator *op)
{
	int i;
	int m_vertex_id;
	float lambda;
	BMOIter siter;
	BMVert *v;
	NLContext *context;

	BM_mesh_elem_index_ensure(bm, BM_VERT);
	lambda = BMO_slot_float_get(op, "lambda");
	nlNewContext();
	context = nlGetCurrent();

	nlSolverParameteri(NL_NB_VARIABLES, bm->totvert);
	nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
	nlSolverParameteri(NL_NB_ROWS, bm->totvert);
	nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);

	nlBegin(NL_SYSTEM);	
	for (i=0; i < bm->totvert; i++) {
		nlLockVariable(i);
	}
	BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
		m_vertex_id = BM_elem_index_get(v);
		nlUnlockVariable(m_vertex_id);
		nlSetVariable(0,m_vertex_id, v->co[0]);
		nlSetVariable(1,m_vertex_id, v->co[1]);
		nlSetVariable(2,m_vertex_id, v->co[2]);
	}

	nlBegin(NL_MATRIX);
	BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
		m_vertex_id = BM_elem_index_get(v);
		nlRightHandSideAdd(0, m_vertex_id, v->co[0]);
		nlRightHandSideAdd(1, m_vertex_id, v->co[1]);
		nlRightHandSideAdd(2, m_vertex_id, v->co[2]);
		compute_weights_in_ring(v, lambda);
	}
		
	nlEnd(NL_MATRIX);
	nlEnd(NL_SYSTEM);

	if(bm->totvert <32){
		nlPrintMatrix();
	}

	nlSolveAdvanced(NULL, NL_TRUE);

	BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
		m_vertex_id = BM_elem_index_get(v);
		v->co[0] =  nlGetVariable(0, m_vertex_id);
		v->co[1] =  nlGetVariable(1, m_vertex_id);
		v->co[2] =  nlGetVariable(2, m_vertex_id);
	}
		
	nlDeleteContext(context);
}

static float cotan_weight(float *v1, float *v2, float *v3)
{
	float a[3], b[3], c[3], clen;

	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v3, v1);
	cross_v3_v3v3(c, a, b);

	clen = len_v3(c);

	if (clen == 0.0f)
		return 0.0f;
	
	return dot_v3v3(a, b) / clen;
}

/* Compute weigth between vertice v_i and all your neighbors
 * weight between v_i and v_neighbor 
 * Wij = cot(alpha) + cot(beta) / (4.0 * total area of all faces  * sum all weight)
 *        v_i *
 *          / | \
 *         /  |  \
 *  v_beta*   |   * v_alpha
 *         \  |  /
 *          \ | /
 *            * v_neighbor
*/
void compute_weights_in_ring(BMVert *v, float lambda)
{	
	float area = 0.0f;
	float factor;
	float sumw = 0.0f;
	float w1, w2;
	float *weight = NULL;
	int ai, bi, ci;
	int id1, id2, id3;
	int i, j;
	int * index = NULL;
	BMIter fi;
	BMIter vi;
	BMFace *f;
	BMVert *vn;
	BMVert *vf[3];
	
	BLI_array_declare(index);
	BLI_array_declare(weight);

	if (v == NULL) {
		return;
	}

	id1 = BM_elem_index_get(v);
	j = 0;
	BM_ITER_ELEM (f, &fi, v, BM_FACES_OF_VERT) {
		i = 0;
		ai = -1;
		BM_ITER_ELEM (vn, &vi, f, BM_VERTS_OF_FACE) {
			vf[i] = vn;
			if (BM_elem_index_get (vf[i]) == id1) {
				ai = i;
				bi = (i + 1) % 3;
				ci = (i + 2) % 3;
			}
			i = i + 1;
		}
		if (i == 3 && ai > -1){
			area = area + area_tri_v3(vf[0]->co, vf[1]->co, vf[2]->co);
			w1 = cotan_weight (vf[bi]->co, vf[ci]->co, vf[ai]->co); 
			w2 = cotan_weight (vf[ci]->co, vf[ai]->co, vf[bi]->co);
			id2 = BM_elem_index_get (vf[bi]);
			id3 = BM_elem_index_get (vf[ci]);
			BLI_array_grow_one (index);
			BLI_array_grow_one (weight);
			index[j] = id3;
			weight[j] = w1;
			j = j + 1;
			BLI_array_grow_one (index);
			BLI_array_grow_one (weight);
			index[j] = id2;
			weight[j] = w2;
			sumw = sumw + w1 + w2;
			j = j + 1;
		}
	}
	for(i = 0; i < j; i = i + 2){
		factor = lambda / (SMOOTH_LAPLACIAN_AREA_FACTOR * sumw * area);
		w1 = -factor * weight[i];
		w2 = -factor * weight[i+1];
		id2 = index[i];
		id3 = index[i+1];
		nlMatrixAdd(id1, id2, w1);
		nlMatrixAdd(id1, id3, w2);
	}
	nlMatrixAdd(id1, id1, 1.0f + lambda / (SMOOTH_LAPLACIAN_AREA_FACTOR * area));

	BLI_array_free(index);
	BLI_array_free(weight);
	
}