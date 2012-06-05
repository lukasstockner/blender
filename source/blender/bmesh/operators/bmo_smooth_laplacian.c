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
#define SMOOTH_LAPLACIAN_EDGE_FACTOR 2.0f

static float cotan_weight(float *v1, float *v2, float *v3);
int vert_is_boundary(BMVert *v);
void compute_weights_in_ring(BMVert *v, float lambda, float min_area);
void compute_weights_in_border(BMVert *v, float lambda, float min_area);
float compute_volume(BMesh *bm, BMOperator *op);
void volume_preservation(BMesh *bm, BMOperator *op, float vini, float vend);

void bmo_vertexsmoothlaplacian_exec(BMesh *bm, BMOperator *op)
{
	int i;
	int m_vertex_id;
	float lambda, lambda_border, min_area;
	float vini, vend;
	BMOIter siter;
	BMVert *v;
	NLContext *context;

	BM_mesh_elem_index_ensure(bm, BM_VERT);
	lambda = BMO_slot_float_get(op, "lambda");
	lambda_border = BMO_slot_float_get(op, "lambda_border");
	min_area = BMO_slot_float_get(op, "min_area");
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
		if (vert_is_boundary(v) == 0) {
			compute_weights_in_ring(v, lambda, min_area);
		}else{
			compute_weights_in_border(v, lambda_border, min_area);
		}
		
	}
		
	nlEnd(NL_MATRIX);
	nlEnd(NL_SYSTEM);

	if(bm->totvert <32){
		nlPrintMatrix();
	}

	if (nlSolveAdvanced(NULL, NL_TRUE) ) {
		vini = compute_volume(bm, op);
		BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
			m_vertex_id = BM_elem_index_get(v);
			v->co[0] =  nlGetVariable(0, m_vertex_id);
			v->co[1] =  nlGetVariable(1, m_vertex_id);
			v->co[2] =  nlGetVariable(2, m_vertex_id);
		}
		vend = compute_volume(bm, op);
		volume_preservation(bm, op, vini, vend);
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

int vert_is_boundary(BMVert *v){
	BMEdge *ed;
	BMIter ei;
	BM_ITER_ELEM(ed, &ei, v, BM_EDGES_OF_VERT) {
		if(BM_edge_is_boundary(ed)){
			return 1;
		}
	}
	return 0;
}

float compute_volume(BMesh *bm, BMOperator *op)
{
	float vol = 0.0f;
	float x1, y1, z1, x2, y2, z2, x3, y3, z3;
	int i;
	BMFace *f;
	BMIter fiter;
	BMIter vi;
	BMVert *vn;
	BMVert *vf[3];
	
	BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
		i = 0;
		BM_ITER_ELEM (vn, &vi, f, BM_VERTS_OF_FACE) {
			vf[i] = vn;
			i = i + 1;
		}
		x1 = vf[0]->co[0];
		y1 = vf[0]->co[1];
		z1 = vf[0]->co[2];

		x2 = vf[1]->co[0];
		y2 = vf[1]->co[1];
		z2 = vf[1]->co[2];

		x3 = vf[2]->co[0];
		y3 = vf[2]->co[1];
		z3 = vf[2]->co[2];

		vol = vol + (1.0 / 6.0) * (0.0 - x3*y2*z1 + x2*y3*z1 + x3*y1*z2 - x1*y3*z2 - x2*y1*z3 + x1*y2*z3);
	}
	return fabs(vol);
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
void compute_weights_in_ring(BMVert *v, float lambda, float min_area)
{	
	float area = 0.0f;
	float at;
	float factor;
	float sumw = 0.0f;
	float w1, w2;
	float *weight = NULL;
	int ai, bi, ci;
	int id1, id2, id3;
	int i, j;
	int * index = NULL;
	int zeroa = 1;
	BMIter fi;
	BMIter vi;
	BMFace *f;
	BMVert *vn;
	BMVert *vf[3];
	
	BLI_array_declare(index);
	BLI_array_declare(weight);

	if (v == NULL) {
		BLI_array_free(index);
		BLI_array_free(weight);
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
			at = area_tri_v3(vf[0]->co, vf[1]->co, vf[2]->co);
			if (fabsf(at) < min_area) {
				zeroa = 0;
			}
			area = area + at;
			w1 = cotan_weight (vf[bi]->co, vf[ci]->co, vf[ai]->co); 
			w2 = cotan_weight (vf[ci]->co, vf[ai]->co, vf[bi]->co);
			id2 = BM_elem_index_get (vf[bi]);
			id3 = BM_elem_index_get (vf[ci]);
			BLI_array_grow_one(index);
			BLI_array_grow_one(weight);
			index[j] = id3;
			weight[j] = w1;
			j = j + 1;
			BLI_array_grow_one(index);
			BLI_array_grow_one(weight);
			index[j] = id2;
			weight[j] = w2;
			sumw = sumw + w1 + w2;
			j = j + 1;
		}
	}
	for (i = 0; i < j; i = i + 2) {
		if (zeroa == 1) {
			factor = lambda / (SMOOTH_LAPLACIAN_AREA_FACTOR * sumw * area);
			w1 = -factor * weight[i];
			w2 = -factor * weight[i+1];
			id2 = index[i];
			id3 = index[i+1];
			nlMatrixAdd(id1, id2, w1);
			nlMatrixAdd(id1, id3, w2);
		} else {
			nlMatrixAdd(id1, id2, 0.0f);
			nlMatrixAdd(id1, id3, 0.0f);
		}
	}
	if (zeroa == 1) {
		nlMatrixAdd(id1, id1, 1.0f + lambda / (SMOOTH_LAPLACIAN_AREA_FACTOR * area));
	} else {
		nlMatrixAdd(id1, id1, 1.0f);

	}

	BLI_array_free(index);
	BLI_array_free(weight);
	
}

void compute_weights_in_border(BMVert *v, float lambda, float min_area){
	float factor;
	float sumw = 0.0f;
	float w1;
	float *weight = NULL;
	int id1, id2;
	int i, j;
	int * index = NULL;
	int zerolen = 0;
	BMEdge *ed;
	BMIter ei;
	BMVert *vn;
	
	BLI_array_declare(index);
	BLI_array_declare(weight);

	if (v == NULL) {
		BLI_array_free(index);
		BLI_array_free(weight);
		return;
	}
	
	id1 = BM_elem_index_get(v);
	j = 0;
	BM_ITER_ELEM (ed, &ei, v, BM_EDGES_OF_VERT) {
		vn = BM_edge_other_vert(ed, v);
		if(vert_is_boundary(vn)==1){
			w1 = len_v3v3(v->co, vn->co);
			if (fabsf(w1) < min_area) {
				zerolen = 1;
			}else{
				w1 = 1.0f/w1;
			}
			id2 = BM_elem_index_get(vn);
			BLI_array_grow_one(index);
			BLI_array_grow_one(weight);
			index[j] = id2;
			weight[j] = w1;
			j = j + 1;
			sumw = sumw + w1;
		}
	}
	for (i = 0; i < j; i++) {
		if (zerolen == 0 ) {
			factor = lambda *SMOOTH_LAPLACIAN_EDGE_FACTOR / sumw;
			w1 = -factor * weight[i];
			id2 = index[i];
			nlMatrixAdd(id1, id2, w1);
		} else {
			nlMatrixAdd(id1, id2, 0.0f);
		}
	}
	
	if (zerolen == 0) {
		nlMatrixAdd(id1, id1, 1.0f + lambda * SMOOTH_LAPLACIAN_EDGE_FACTOR);
	} else {
		nlMatrixAdd(id1, id1, 1.0f);
	}

	BLI_array_free(index);
	BLI_array_free(weight);
}

void volume_preservation(BMesh *bm, BMOperator *op, float vini, float vend)
{
	float beta;
	BMOIter siter;
	BMVert *v;

	if (vend != 0.0f) {	
		beta  = pow (vini / vend, 1.0f / 3.0f);
		BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
			mul_v3_fl(v->co, beta );
		}
	}
}