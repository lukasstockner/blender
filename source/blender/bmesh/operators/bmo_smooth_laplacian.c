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

void init_index(BMesh *bm);
void compute_weight(BMFace *f, int vid, float lambda);
float compute_weight_return( BMFace *f, int vid, float lambda);
static float cotan_weight(float *v1, float *v2, float *v3);
float area_ring(BMVert *v);

void bmo_vertexsmoothlaplacian_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMVert *v;
	BMFace *f;
	int m_vertex_id;
	NLContext *context;
	float lambda = BMO_slot_float_get(op, "lambda");
	float we;
	int i;

	init_index(bm);

		nlNewContext();
		context = nlGetCurrent();
		nlSolverParameteri(NL_NB_VARIABLES, bm->totvert);
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
		nlSolverParameteri(NL_NB_ROWS, bm->totvert);
		nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);
		nlBegin(NL_SYSTEM);
		
		for(i=0; i<bm->totvert; i++){
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
			
			nlMatrixAdd(m_vertex_id, m_vertex_id, 1.0f);
			
			we = 0.0f;
			BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
				we = we + compute_weight_return(f,m_vertex_id,  lambda);
			}
			we = lambda/(we*4.0f*area_ring(v));
			BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
				compute_weight(f,m_vertex_id,  we);
			}
			nlMatrixAdd(m_vertex_id, m_vertex_id, lambda/(4.0f*area_ring(v)));
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

void init_index(BMesh *bm){
	BM_mesh_elem_index_ensure(bm, BM_VERT);
}

/*
 *        v_i *
 *          / | \
 *         /  |  \
 *  v_beta*   |   * v_alpha
 *         \  |  /
 *          \ | /
 *            * v_neighbor
*/
void compute_weight( BMFace *f, int vid, float lambda){
	BMIter iter;
	BMVert *v;
	BMVert *vf[3];
	int i;
	float wa = 0.0f;
	i = 0;
	BM_ITER_ELEM (v, &iter, f, BM_VERTS_OF_FACE) {
		vf[i] = v;
		i = i + 1;
	}
	
	for(i=0; i<3; i++){
		int va = i;
		int vb = (i+1)%3;
		int vc = (i+2)%3;
		int va_id = BM_elem_index_get(vf[va]);
		int vb_id = BM_elem_index_get(vf[vb]);
		int vc_id = BM_elem_index_get(vf[vc]);
		if(va_id == vid ){
			int vb_id = BM_elem_index_get(vf[vb]);
			int vc_id = BM_elem_index_get(vf[vc]);
			wa = lambda*cotan_weight(vf[vb]->co, vf[vc]->co, vf[va]->co);
			nlMatrixAdd(vid, vc_id, -wa);
			wa = lambda*cotan_weight(vf[vc]->co, vf[va]->co, vf[vb]->co);
			nlMatrixAdd(vid, vb_id, -wa);
		}
	}
}

float compute_weight_return( BMFace *f, int vid, float lambda){
	BMIter iter;
	BMVert *v;
	BMVert *vf[3];
	int i;
	float wa = 0.0f;
	i = 0;
	BM_ITER_ELEM (v, &iter, f, BM_VERTS_OF_FACE) {
		vf[i] = v;
		i = i + 1;
	}
	
	for(i=0; i<3; i++){
		int va = i;
		int vb = (i+1)%3;
		int vc = (i+2)%3;
		int va_id = BM_elem_index_get(vf[va]);
		int vb_id = BM_elem_index_get(vf[vb]);
		int vc_id = BM_elem_index_get(vf[vc]);
		if(va_id == vid ){
			int vb_id = BM_elem_index_get(vf[vb]);
			int vc_id = BM_elem_index_get(vf[vc]);
			wa = cotan_weight(vf[vb]->co, vf[vc]->co, vf[va]->co); 
			wa = wa + cotan_weight(vf[vc]->co, vf[va]->co, vf[vb]->co);
		}
	}
	return wa;
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

float area_ring(BMVert *v){
	BMIter fiter;
	BMIter viter;
	BMVert *vn;
	BMFace *f;
	BMVert *vf[3];
	int i;
	float area = 0.0f;
	BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
		i = 0;
		BM_ITER_ELEM (vn, &viter, f, BM_VERTS_OF_FACE) {
			vf[i] = vn;
			i = i + 1;
		}
		if(i == 3){
			area = area + area_tri_v3(vf[0]->co, vf[1]->co, vf[2]->co);
		}
	}
	return area;

}