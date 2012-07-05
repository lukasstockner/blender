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
 * Contributor(s): Joseph Eagar,
 *                 Alexander Pinzon
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

struct BBMOLaplacianSystem {
	float *eweights;		/* Length weights per Edge */
	float (*fweights)[3];   /* Cotangent weights per face */
	float *ring_areas;		/* Total area per ring*/
	float *vlengths;		/* Total sum of lengths(edges) per vertice*/
	float *vweights;		/* Total sum of weights per vertice*/
	int numEdges;			/* Number of edges*/
	int numFaces;			/* Number of faces*/
	int numVerts;			/* Number of verts*/
	short *zerola;			/* Is zero area or length*/

	/* Pointers to data*/
	BMesh *bm;
	BMOperator *op;
	NLContext *context;

	/*Data*/
	float min_area;
};
typedef struct BBMOLaplacianSystem BMOLaplacianSystem;

static float cotan_weight(float *v1, float *v2, float *v3);
int vert_is_boundary(BMVert *v);
float compute_volume(BMesh *bm, BMOperator *op);
void volume_preservation(BMesh *bm, BMOperator *op, float vini, float vend, int usex, int usey, int usez);
static void init_laplacian(BMOLaplacianSystem * sys);
static void fill_laplacian_matrix(BMOLaplacianSystem * sys);
static void delete_void_MLS(void * data);
static void delete_BMOLaplacianSystem(BMOLaplacianSystem * sys);
static void memset_BMOLaplacianSystem(BMOLaplacianSystem *sys, int val);
static BMOLaplacianSystem * init_BMOLaplacianSystem( int a_numEdges, int a_numFaces, int a_numVerts);

static void delete_void_MLS(void * data)
{
	if (data) {
		MEM_freeN(data);
		data = NULL;
	}
}

static void delete_BMOLaplacianSystem(BMOLaplacianSystem * sys) 
{
	delete_void_MLS(sys->eweights);
	delete_void_MLS(sys->fweights);
	delete_void_MLS(sys->ring_areas);
	delete_void_MLS(sys->vlengths);
	delete_void_MLS(sys->vweights);
	delete_void_MLS(sys->zerola);
	if (sys->context) {
		nlDeleteContext(sys->context);
	}
	sys->bm = NULL;
	sys->op = NULL;
	MEM_freeN(sys);
}

static void memset_BMOLaplacianSystem(BMOLaplacianSystem *sys, int val)
{
	memset(sys->eweights	, val, sizeof(float) * sys->numEdges);
	memset(sys->fweights	, val, sizeof(float) * sys->numFaces * 3);
	memset(sys->ring_areas	, val, sizeof(float) * sys->numVerts);
	memset(sys->vlengths	, val, sizeof(float) * sys->numVerts);
	memset(sys->vweights	, val, sizeof(float) * sys->numVerts);
	memset(sys->zerola		, val, sizeof(short) * sys->numVerts);
}

static BMOLaplacianSystem * init_BMOLaplacianSystem( int a_numEdges, int a_numFaces, int a_numVerts) 
{
	BMOLaplacianSystem * sys; 
	sys = MEM_callocN(sizeof(BMOLaplacianSystem), "ModLaplSmoothSystem");
	sys->numEdges = a_numEdges;
	sys->numFaces = a_numFaces;
	sys->numVerts = a_numVerts;

	sys->eweights =  MEM_callocN(sizeof(float) * sys->numEdges, "ModLaplSmoothEWeight");
	if (!sys->eweights) {
		delete_BMOLaplacianSystem(sys);
		return NULL;
	}
	
	sys->fweights =  MEM_callocN(sizeof(float) * 3 * sys->numFaces, "ModLaplSmoothFWeight");
	if (!sys->fweights) {
		delete_BMOLaplacianSystem(sys);
		return NULL;
	}
	
	sys->ring_areas =  MEM_callocN(sizeof(float) * sys->numVerts, "ModLaplSmoothRingAreas");
	if (!sys->ring_areas) {
		delete_BMOLaplacianSystem(sys);
		return NULL;
	}
	
	sys->vlengths =  MEM_callocN(sizeof(float) * sys->numVerts, "ModLaplSmoothVlengths");
	if (!sys->vlengths) {
		delete_BMOLaplacianSystem(sys);
		return NULL;
	}

	sys->vweights =  MEM_callocN(sizeof(float) * sys->numVerts, "ModLaplSmoothVweights");
	if (!sys->vweights) {
		delete_BMOLaplacianSystem(sys);
		return NULL;
	}

	sys->zerola =  MEM_callocN(sizeof(short) * sys->numVerts, "ModLaplSmoothZeloa");
	if (!sys->zerola) {
		delete_BMOLaplacianSystem(sys);
		return NULL;
	}

	return sys;
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

static void init_laplacian(BMOLaplacianSystem * sys)
{
	float *v1, *v2, *v3, *v4;
	float w1, w2, w3, w4;
	float areaf;
	int i, j;
	unsigned int idv1, idv2, idv3, idv4, idv[4];
	int has_4_vert ;
	BMEdge *e;
	BMFace *f;
	BMIter eiter;
	BMIter fiter;
	BMIter vi;
	BMVert *vn;
	BMVert *vf[4];

	BM_ITER_MESH_INDEX (e, &eiter, sys->bm, BM_EDGES_OF_MESH, j) {
		if (!BM_elem_flag_test(e, BM_ELEM_SELECT) && BM_edge_is_boundary(e)) {
			v1 = e->v1->co;
			v2 =  e->v2->co;
			idv1 = BM_elem_index_get(e->v1);
			idv2 = BM_elem_index_get(e->v2);
			
			w1 = len_v3v3(v1, v2);
			if (w1 > sys->min_area) {
				w1 = 1.0f / w1;
				i = BM_elem_index_get(e);
				sys->eweights[i] = w1;
				sys->vlengths[idv1] += w1;
				sys->vlengths[idv2] += w1;
			}else{
				sys->zerola[idv1] = 1;
				sys->zerola[idv2] = 1;
			}
		}
	}

	BM_ITER_MESH (f, &fiter, sys->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {

			BM_ITER_ELEM_INDEX (vn, &vi, f, BM_VERTS_OF_FACE, i) {
				vf[i] = vn;
			}
			has_4_vert = (i == 4) ? 1 : 0;
			idv1 = BM_elem_index_get(vf[0]);
			idv2 = BM_elem_index_get(vf[1]);
			idv3 = BM_elem_index_get(vf[2]);
			idv4 = has_4_vert ? BM_elem_index_get(vf[3]) : 0;

			v1 = vf[0]->co;
			v2 = vf[1]->co;
			v3 = vf[2]->co;
			v4 = has_4_vert ? vf[3]->co : 0;

			if (has_4_vert) {
				areaf = area_quad_v3(v1, v2, v3, v4);
			} else {
				areaf = area_tri_v3(v1, v2, v3);
			}

			if (fabs(areaf) < sys->min_area) { 
				sys->zerola[idv1] = 1;
				sys->zerola[idv2] = 1;
				sys->zerola[idv3] = 1;
				if (has_4_vert) sys->zerola[idv4] = 1;
			}

			sys->ring_areas[idv1] += areaf;
			sys->ring_areas[idv2] += areaf;
			sys->ring_areas[idv3] += areaf;
			if (has_4_vert) sys->ring_areas[idv4] += areaf;

			if (has_4_vert) {
			
				idv[0] = idv1;
				idv[1] = idv2;
				idv[2] = idv3;
				idv[3] = idv4;

				for (j = 0; j < 4; j++) {
					idv1 = idv[j];
					idv2 = idv[(j + 1) % 4];
					idv3 = idv[(j + 2) % 4];
					idv4 = idv[(j + 3) % 4];

					v1 = vf[j]->co;
					v2 = vf[(j + 1) % 4]->co;
					v3 = vf[(j + 2) % 4]->co;
					v4 = vf[(j + 3) % 4]->co;

					w2 = cotan_weight(v4, v1, v2) + cotan_weight(v3, v1, v2);
					w3 = cotan_weight(v2, v3, v1) + cotan_weight(v4, v1, v3);
					w4 = cotan_weight(v2, v4, v1) + cotan_weight(v3, v4, v1);
	
					sys->vweights[idv1] += (w2 + w3 + w4) / 4.0f;
				}
			} else {			
				i = BM_elem_index_get(f);

				w1 = cotan_weight(v1, v2, v3);
				w2 = cotan_weight(v2, v3, v1);
				w3 = cotan_weight(v3, v1, v2);

				sys->fweights[i][0] += w1;
				sys->fweights[i][1] += w2;
				sys->fweights[i][2] += w3;
			
				sys->vweights[idv1] += w2 + w3;
				sys->vweights[idv2] += w1 + w3;
				sys->vweights[idv3] += w1 + w2;
			}
		}
	}
}

static void fill_laplacian_matrix(BMOLaplacianSystem * sys)
{
	float *v1, *v2, *v3, *v4;
	float w2, w3, w4;
	int i, j;
	int has_4_vert ;
	unsigned int idv1, idv2, idv3, idv4, idv[4];

	BMEdge *e;
	BMFace *f;
	BMIter eiter;
	BMIter fiter;
	BMIter vi;
	BMVert *vn;
	BMVert *vf[4];

	BM_ITER_MESH (f, &fiter, sys->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
			BM_ITER_ELEM_INDEX (vn, &vi, f, BM_VERTS_OF_FACE, i) {
				vf[i] = vn;
			}
			has_4_vert = (i == 4) ? 1 : 0;
			if (has_4_vert) {
				idv[0] = BM_elem_index_get(vf[0]);
				idv[1] = BM_elem_index_get(vf[1]);
				idv[2] = BM_elem_index_get(vf[2]);
				idv[3] = BM_elem_index_get(vf[3]);
				for (j = 0; j < 4; j++) {
					idv1 = idv[j];
					idv2 = idv[(j + 1) % 4];
					idv3 = idv[(j + 2) % 4];
					idv4 = idv[(j + 3) % 4];

					v1 = vf[j]->co;
					v2 = vf[(j + 1) % 4]->co;
					v3 = vf[(j + 2) % 4]->co;
					v4 = vf[(j + 3) % 4]->co;

					w2 = cotan_weight(v4, v1, v2) + cotan_weight(v3, v1, v2);
					w3 = cotan_weight(v2, v3, v1) + cotan_weight(v4, v1, v3);
					w4 = cotan_weight(v2, v4, v1) + cotan_weight(v3, v4, v1);

					w2 = w2 / 4.0f;
					w3 = w3 / 4.0f;
					w4 = w4 / 4.0f;
		
					if (!vert_is_boundary(vf[j]) && sys->zerola[idv1] == 0) { 
						nlMatrixAdd(idv1, idv2, w2 * sys->vweights[idv1]);
						nlMatrixAdd(idv1, idv3, w3 * sys->vweights[idv1]);
						nlMatrixAdd(idv1, idv4, w4 * sys->vweights[idv1]);
					}
				}
			} else {
				idv1 = BM_elem_index_get(vf[0]);
				idv2 = BM_elem_index_get(vf[1]);
				idv3 = BM_elem_index_get(vf[2]);
				/* Is ring if number of faces == number of edges around vertice*/
				i = BM_elem_index_get(f);
				if (!vert_is_boundary(vf[0]) && sys->zerola[idv1] == 0) { 
					nlMatrixAdd(idv1, idv2, sys->fweights[i][2] * sys->vweights[idv1]);
					nlMatrixAdd(idv1, idv3, sys->fweights[i][1] * sys->vweights[idv1]);
				}
				if (!vert_is_boundary(vf[1]) && sys->zerola[idv2] == 0) { 
					nlMatrixAdd(idv2, idv1, sys->fweights[i][2] * sys->vweights[idv2]);
					nlMatrixAdd(idv2, idv3, sys->fweights[i][0] * sys->vweights[idv2]);
				}
				if (!vert_is_boundary(vf[2]) && sys->zerola[idv3] == 0) { 
					nlMatrixAdd(idv3, idv1, sys->fweights[i][1] * sys->vweights[idv3]);
					nlMatrixAdd(idv3, idv2, sys->fweights[i][0] * sys->vweights[idv3]);
				}
			}
		}
	}
	BM_ITER_MESH (e, &eiter, sys->bm, BM_EDGES_OF_MESH) {
		if (!BM_elem_flag_test(e, BM_ELEM_SELECT) && BM_edge_is_boundary(e) ) {
			v1 = e->v1->co;
			v2 =  e->v2->co;
			idv1 = BM_elem_index_get(e->v1);
			idv2 = BM_elem_index_get(e->v2);
			if (sys->zerola[idv1] == 0 && sys->zerola[idv2] == 0) {
				i = BM_elem_index_get(e);
				nlMatrixAdd(idv1, idv2, sys->eweights[i] * sys->vlengths[idv1]);
				nlMatrixAdd(idv2, idv1, sys->eweights[i] * sys->vlengths[idv2]);
			}
		}
	}
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

int vert_is_boundary(BMVert *v)
{
	BMEdge *ed;
	BMFace *f;
	BMIter ei;
	BMIter fi;
	BM_ITER_ELEM(ed, &ei, v, BM_EDGES_OF_VERT) {
		if (BM_edge_is_boundary(ed)) {
			return 1;
		}
	}
	BM_ITER_ELEM (f, &fi, v, BM_FACES_OF_VERT) {
		if (!BM_elem_flag_test(f, BM_ELEM_SELECT)) {
			return 1;
		}
	}
	return 0;
}

float compute_volume(BMesh *bm, BMOperator *op)
{
	float vol = 0.0f;
	float x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;
	int i;
	BMFace *f;
	BMIter fiter;
	BMIter vi;
	BMVert *vn;
	BMVert *vf[4];
	
	BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
		BM_ITER_ELEM_INDEX (vn, &vi, f, BM_VERTS_OF_FACE, i) {
			vf[i] = vn;
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

		vol += (1.0 / 6.0) * (0.0 - x3*y2*z1 + x2*y3*z1 + x3*y1*z2 - x1*y3*z2 - x2*y1*z3 + x1*y2*z3);

		if (i == 4) {
			x4 = vf[3]->co[0];
			y4 = vf[3]->co[1];
			z4 = vf[3]->co[2];
			vol += (1.0 / 6.0) * (x1*y3*z4 - x1*y4*z3 - x3*y1*z4 + x3*z1*y4 + y1*x4*z3 - x4*y3*z1);
		}
	}
	return fabs(vol);
}

void volume_preservation(BMesh *bm, BMOperator *op, float vini, float vend, int usex, int usey, int usez)
{
	float beta;
	BMOIter siter;
	BMVert *v;

	if (vend != 0.0f) {	
		beta  = pow (vini / vend, 1.0f / 3.0f);
		BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
			if (usex) {
				v->co[0] *=  beta;
			}
			if (usey) {
				v->co[1] *= beta;
			}
			if (usez) {
				v->co[2] *= beta;
			}
			
		}
	}
}

void bmo_smooth_laplacian_vert_exec(BMesh *bm, BMOperator *op)
{
	int i;
	int m_vertex_id;
	int usex, usey, usez;
	float lambda, lambda_border;
	float vini, vend;
	float w;
	BMOIter siter;
	BMVert *v;
	BMOLaplacianSystem * sys;

	sys = init_BMOLaplacianSystem(bm->totedge, bm->totface, bm->totvert);
	if (!sys) return;
	sys->bm = bm;
	sys->op = op;

	memset_BMOLaplacianSystem(sys, 0);

	BM_mesh_elem_index_ensure(bm, BM_VERT);
	lambda = BMO_slot_float_get(op, "lambda");
	lambda_border = BMO_slot_float_get(op, "lambda_border");
	sys->min_area = BMO_slot_float_get(op, "min_area");
	usex = BMO_slot_bool_get(op, "use_x");
	usey = BMO_slot_bool_get(op, "use_y");
	usez = BMO_slot_bool_get(op, "use_z");


	nlNewContext();
	sys->context = nlGetCurrent();

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
	init_laplacian(sys);
	BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
		m_vertex_id = BM_elem_index_get(v);
		nlRightHandSideAdd(0, m_vertex_id, v->co[0]);
		nlRightHandSideAdd(1, m_vertex_id, v->co[1]);
		nlRightHandSideAdd(2, m_vertex_id, v->co[2]);
		i = m_vertex_id;
		if (sys->zerola[i] == 0) {
			w = sys->vweights[i] * sys->ring_areas[i];
			sys->vweights[i] = (w == 0.0f) ? 0.0f : - lambda  / (4.0f * w);
			w = sys->vlengths[i];
			sys->vlengths[i] = (w == 0.0f) ? 0.0f : - lambda_border  * 2.0f / w;

			if (!vert_is_boundary(v)) { 
				nlMatrixAdd(i, i,  1.0f + lambda / (4.0f * sys->ring_areas[i]));
			} else { 
				nlMatrixAdd(i, i,  1.0f + lambda_border * 2.0f);
			}
		} else {
			nlMatrixAdd(i, i, 1.0f);
		}	
	}
	fill_laplacian_matrix(sys);
		
	nlEnd(NL_MATRIX);
	nlEnd(NL_SYSTEM);

	if (nlSolveAdvanced(NULL, NL_TRUE) ) {
		vini = compute_volume(bm, op);
		BMO_ITER (v, &siter, bm, op, "verts", BM_VERT) {
			m_vertex_id = BM_elem_index_get(v);
			if (usex) {
				v->co[0] =  nlGetVariable(0, m_vertex_id);
			}
			if (usey) {
				v->co[1] =  nlGetVariable(1, m_vertex_id);
			}
			if (usez) {
				v->co[2] =  nlGetVariable(2, m_vertex_id);
			}
		}
		vend = compute_volume(bm, op);
		volume_preservation(bm, op, vini, vend, usex, usey, usez);
	}
		
	delete_BMOLaplacianSystem(sys);
}
