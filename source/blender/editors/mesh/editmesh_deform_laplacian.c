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
 * Contributor(s): Alexander Pinzon
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_deform_laplacian.c
 *  \ingroup bmesh
 *
 * Deform Laplacian.
 */

#include "WM_api.h"
#include "WM_types.h"

#include "DNA_object_types.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mesh_intern.h"  /* own include */

#include "BKE_DerivedMesh.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"

#include "MEM_guardedalloc.h"
#include "BLI_math.h"
#include "BLI_array.h"
#include "ONL_opennl.h"

#include "editmesh_deform_utils.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"
#include "ED_mesh.h"

struct BLaplacianSystem {
	float *vweights;			/* Total sum of weights per vertice*/
	float (*delta)[3];			/* Differential Coordinates*/
	float (*cos)[3];			/* Original Vertices Positions*/
	float (*nos)[3];			/* Original Vertices Normals*/
	BMVert **handlers;			/* Handlers Vertices Positions*/
	int *handlers_index;		/* Handlers Vertices index*/
	int *statics_index;			/* Static Vertices index*/
	BMVert **uverts;			/* Unit vectors of projected edges onto the plane orthogonal to  n*/
	int numVerts;				/* Number of verts*/
	int numStatics;				/* Number of static amchor verts*/
	int numHandlers;			/* Number of handler anchor verts*/
	/* Pointers to data*/
	BMesh *bm;
	BMOperator *op;
	NLContext *context;			/* System for solve general implicit rotations*/
	NLContext *contextrot;		/* System for solve general Laplacian with rotated differential coordinates*/
	vptrSpMatrixD spLapMatrix;  /* Sparse Laplacian Matrix*/
	vptrVectorD VectorB;		/* Array to store vertex positions of handlers*/
	vptrVectorD VectorX;		/* Array to  store solution */
	vptrTripletD tripletList;	/* List of triplets in Laplacian Matrix*/
};
typedef struct BLaplacianSystem LaplacianSystem;

static void compute_mesh_laplacian();
static void delete_laplacian_system(LaplacianSystem *sys);
static void delete_void_pointer(void *data);
static void compute_implict_rotations(LaplacianSystem * sys);

static float cotan_weight(float *v1, float *v2, float *v3)
{
	float a[3], b[3], c[3], clen;

	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v3, v1);
	cross_v3_v3v3(c, a, b);

	clen = len_v3(c);
	
	if (clen < FLT_EPSILON)
		return 0.0f;

	return dot_v3v3(a, b) / clen;
}

static void delete_void_pointer(void *data)
{
	if (data) {
		MEM_freeN(data);
	}
}

static void delete_laplacian_system(LaplacianSystem *sys)
{
	if(!sys) return;
	delete_void_pointer(sys->vweights);
	delete_void_pointer(sys->delta);
	delete_void_pointer(sys->uverts);
	if (sys->context) {
		nlDeleteContext(sys->context);
	}
	if (sys->contextrot) {
		nlDeleteContext(sys->contextrot);
	}
	if (sys->spLapMatrix) {
		delete_spmatrix(sys->spLapMatrix);
	}
	if (sys->VectorB) {
		delete_vectord(sys->VectorB);
	}
	if (sys->VectorX) {
		delete_vectord(sys->VectorX);
	}
	if (sys->tripletList) {
		delete_triplet(sys->tripletList);
	}
	sys->bm = NULL;
	sys->op = NULL;
	MEM_freeN(sys);
}

static void memset_laplacian_system(LaplacianSystem *sys, int val)
{
	memset(sys->vweights,     val, sizeof(float) * sys->numVerts);
	memset(sys->delta,     val, sizeof(float) * sys->numVerts);
}

static void init_laplacian_system(LaplacianSystem * sys, int a_numVerts, int rows, int cols)
{
	//LaplacianSystem *sys;
	/*sys = (LaplacianSystem *)MEM_callocN(sizeof(LaplacianSystem), "bmoLaplDeformSystem");
	if (!sys) {
		return NULL;
	}*/
	sys->numVerts = a_numVerts;

	sys->spLapMatrix = new_spmatrix(rows, cols);
	if (!sys->spLapMatrix) {
		delete_laplacian_system(sys);
		return ;
	}

	sys->VectorB = new_vectord(rows);
	if (!sys->VectorB) {
		delete_laplacian_system(sys);
		return ;
	}

	sys->VectorX = new_vectord(cols);
	if (!sys->VectorX) {
		delete_laplacian_system(sys);
		return ;
	}
	
	sys->tripletList = new_triplet(a_numVerts*18);
	if (!sys->tripletList) {
		delete_laplacian_system(sys);
		return ;
	}

	sys->vweights =  (float *)MEM_callocN(sizeof(float) * sys->numVerts, "bmoLaplDeformVweights");
	if (!sys->vweights) {
		delete_laplacian_system(sys);
		return ;
	}

	sys->uverts =  (BMVert **)MEM_callocN(sizeof(BMVert *) * sys->numVerts, "bmoLaplDeformuverts");
	if (!sys->uverts) {
		delete_laplacian_system(sys);
		return ;
	}

	sys->delta =  (float (*)[3])MEM_callocN(sizeof(float) * sys->numVerts * 3, "bmoLaplDeformDelta");
	if (!sys->delta) {
		delete_laplacian_system(sys);
		return ;
	}

	//return sys;
}

static void init_laplacian_matrix(LaplacianSystem *sys)
{
	float *v1, *v2, *v3, *v4;
	float w2, w3, w4;
	int i, j;
	bool has_4_vert;
	unsigned int idv1, idv2, idv3, idv4, idv[4];
	BMFace *f;
	BMIter fiter;
	BMIter vi;
	BMVert *vn;
	BMVert *vf[4];
	float vfcos[4][3];

	BM_ITER_MESH (f, &fiter, sys->bm, BM_FACES_OF_MESH) {
	

		BM_ITER_ELEM_INDEX (vn, &vi, f, BM_VERTS_OF_FACE, i) {
			vf[i] = vn;
			copy_v3_v3(vfcos[i], vn->co);
		}
		has_4_vert = (i == 4) ? 1 : 0;
		idv1 = BM_elem_index_get(vf[0]);
		idv2 = BM_elem_index_get(vf[1]);
		idv3 = BM_elem_index_get(vf[2]);
		idv4 = has_4_vert ? BM_elem_index_get(vf[3]) : 0;

		v1 = vfcos[0];// vf[0]->co;
		v2 = vfcos[1];//vf[1]->co;
		v3 = vfcos[2];//vf[2]->co;
		v4 = has_4_vert ? vfcos[3] : 0;

		idv[0] = idv1;
		idv[1] = idv2;
		idv[2] = idv3;
		idv[3] = idv4;

		nlMakeCurrent(sys->context);
		nlRightHandSideAdd(0, idv1						, 0.0f);
		nlRightHandSideAdd(0, sys->numVerts + idv1		, 0.0f);
		nlRightHandSideAdd(0, 2*sys->numVerts + idv1	, 0.0f);

		for (j = 0; j < i; j++) {
			idv1 = idv[j];
			idv2 = idv[(j + 1) % i];
			idv3 = idv[(j + 2) % i];
			idv4 = idv[(j + 3) % i];

			v1 = vfcos[j];
			v2 = vfcos[(j + 1) % i];
			v3 = vfcos[(j + 2) % i];
			v4 = has_4_vert ? vfcos[(j + 3) % i] : 0;

			if (has_4_vert) {

				w2 = (cotan_weight(v4, v1, v2) + cotan_weight(v3, v1, v2)) ;
				w3 = (cotan_weight(v2, v3, v1) + cotan_weight(v4, v1, v3)) ;
				w4 = (cotan_weight(v2, v4, v1) + cotan_weight(v3, v4, v1)) ;

				sys->delta[idv1][0] -=  v4[0] * w4;
				sys->delta[idv1][1] -=  v4[1] * w4;
				sys->delta[idv1][2] -=  v4[2] * w4;

				nlMakeCurrent(sys->context);
				nlMatrixAdd(idv1					, idv4						, -w4 );
				nlMatrixAdd(sys->numVerts + idv1	, sys->numVerts + idv4		, -w4 );
				nlMatrixAdd(sys->numVerts*2 + idv1	, sys->numVerts*2 + idv4	, -w4 );

				nlMakeCurrent(sys->contextrot);
				nlMatrixAdd(idv1					, idv4						, -w4 );

				push_back_triplet(sys->tripletList, idv1					, idv4						, -w4 );
				push_back_triplet(sys->tripletList, sys->numVerts + idv1	, sys->numVerts + idv4		, -w4 );
				push_back_triplet(sys->tripletList, sys->numVerts*2 + idv1	, sys->numVerts*2 + idv4	, -w4 );
				
				
			}
			else {
				w2 = cotan_weight(v3, v1, v2);
				w3 = cotan_weight(v2, v3, v1);
				w4 = 0.0f;
			}

			sys->vweights[idv1] += w2 + w3 + w4;

			sys->delta[idv1][0] +=  v1[0] * (w2 + w3 + w4);
			sys->delta[idv1][1] +=  v1[1] * (w2 + w3 + w4);
			sys->delta[idv1][2] +=  v1[2] * (w2 + w3 + w4);

			sys->delta[idv1][0] -=  v2[0] * w2;
			sys->delta[idv1][1] -=  v2[1] * w2;
			sys->delta[idv1][2] -=  v2[2] * w2;

			sys->delta[idv1][0] -=  v3[0] * w3;
			sys->delta[idv1][1] -=  v3[1] * w3;
			sys->delta[idv1][2] -=  v3[2] * w3;

			nlMakeCurrent(sys->context);
			nlMatrixAdd(idv1					, idv2						, -w2);
			nlMatrixAdd(sys->numVerts + idv1	, sys->numVerts + idv2		, -w2);
			nlMatrixAdd(sys->numVerts*2 + idv1	, sys->numVerts*2 + idv2	, -w2);

			nlMatrixAdd(idv1					, idv3						, -w3);
			nlMatrixAdd(sys->numVerts + idv1	, sys->numVerts + idv3		, -w3);
			nlMatrixAdd(sys->numVerts*2 + idv1	, sys->numVerts*2 + idv3	, -w3);

			nlMatrixAdd(idv1					, idv1						, w2 + w3 + w4);
			nlMatrixAdd(sys->numVerts + idv1	, sys->numVerts + idv1		, w2 + w3 + w4);
			nlMatrixAdd(sys->numVerts*2 + idv1	, sys->numVerts*2 + idv1	, w2 + w3 + w4);

			nlMakeCurrent(sys->contextrot);
			nlMatrixAdd(idv1					, idv2						, -w2);
			nlMatrixAdd(idv1					, idv3						, -w3);
			nlMatrixAdd(idv1					, idv1						, w2 + w3 + w4);

			push_back_triplet(sys->tripletList, idv1					, idv2						, -w2);
			push_back_triplet(sys->tripletList, sys->numVerts + idv1	, sys->numVerts + idv2		, -w2);
			push_back_triplet(sys->tripletList, sys->numVerts*2 + idv1	, sys->numVerts*2 + idv2	, -w2);

			push_back_triplet(sys->tripletList, idv1					, idv3						, -w3);
			push_back_triplet(sys->tripletList, sys->numVerts + idv1	, sys->numVerts + idv3		, -w3);
			push_back_triplet(sys->tripletList, sys->numVerts*2 + idv1	, sys->numVerts*2 + idv3	, -w3);

			push_back_triplet(sys->tripletList, idv1					, idv1						, w2 + w3 + w4);
			push_back_triplet(sys->tripletList, sys->numVerts + idv1	, sys->numVerts + idv1		, w2 + w3 + w4);
			push_back_triplet(sys->tripletList, sys->numVerts*2 + idv1	, sys->numVerts*2 + idv1	, w2 + w3 + w4);
			
		}
	}
}

static void compute_implict_rotations(LaplacianSystem * sys)
{
	BMEdge *e;
	BMIter eiter;
	BMIter viter;
	BMVert *v;
	BMVert *v2;
	BMVert ** vn = NULL;
	float minj, mjt, *qj, vj[3];
	int i, j, ln, jid, k;
	vptrMatrixD C, TDelta;
	BLI_array_declare(vn);

	nlMakeCurrent(sys->context);
	BM_ITER_MESH (v, &viter, sys->bm, BM_VERTS_OF_MESH) {
		i = BM_elem_index_get(v);
		BM_ITER_ELEM(e, &eiter, v, BM_EDGES_OF_VERT) {
			if (BM_elem_index_get(e->v1) == i) {
				BLI_array_append(vn, e->v2);
			}
			else {
				BLI_array_append(vn, e->v1);
			}
		}
		BLI_array_append(vn, v);
		ln = BLI_array_count(vn);
		minj = 1000000.0f;
		for (j = 0; j < (ln-1); j++) {
			k = BM_elem_index_get(vn[j]);
			qj = sys->cos[k];// vn[j]->co;
			sub_v3_v3v3(vj, qj, sys->cos[i]); //sub_v3_v3v3(vj, qj, v->co);
			normalize_v3(vj);
			mjt = fabs(dot_v3v3(vj, sys->nos[i])); //mjt = fabs(dot_v3v3(vj, v->no));
			if (mjt < minj) {
				minj = mjt;
				sys->uverts[i] = vn[j];
				//copy_v3_v3(sys->uverts[i], vn[j]->co);
			}
		}

		C = new_matrixd( ln*3, 7);
		for (j = 0; j < ln; j++) {
			v2 = vn[j];
			k = BM_elem_index_get(v2);
			set_matrixd(C, j, 0, sys->cos[k][0]);	//set_matrixd(C, j, 0, v2->co[0]);		
			set_matrixd(C, j, 1, 0.0f);		
			set_matrixd(C, j, 2, sys->cos[k][2]);	//set_matrixd(C, j, 2, v2->co[2]);		
			set_matrixd(C, j, 3, -sys->cos[k][1]);		//set_matrixd(C, j, 3, -v2->co[1]);
			set_matrixd(C, j, 4, 1.0f);
			set_matrixd(C, j, 5, 0.0f);
			set_matrixd(C, j, 6, 0.0f);

			set_matrixd(C, ln + j, 0, sys->cos[k][1]);		//set_matrixd(C, ln + j, 0, v2->co[1]);		
			set_matrixd(C, ln + j, 1, -sys->cos[k][2]);		//set_matrixd(C, ln + j, 1, -v2->co[2]);		
			set_matrixd(C, ln + j, 2, 0.0f);		
			set_matrixd(C, ln + j, 3, sys->cos[k][0]);		//set_matrixd(C, ln + j, 3, v2->co[0]);
			set_matrixd(C, ln + j, 4, 0.0f);
			set_matrixd(C, ln + j, 5, 1.0f);
			set_matrixd(C, ln + j, 6, 0.0f);

			set_matrixd(C, ln*2 + j, 0, sys->cos[k][2]);		//set_matrixd(C, ln*2 + j, 0, v2->co[2]);		
			set_matrixd(C, ln*2 + j, 1, sys->cos[k][1]);		//set_matrixd(C, ln*2 + j, 1, v2->co[1]);		
			set_matrixd(C, ln*2 + j, 2, -sys->cos[k][0]);	//set_matrixd(C, ln*2 + j, 2, -v2->co[0]);		
			set_matrixd(C, ln*2 + j, 3, 0.0f);
			set_matrixd(C, ln*2 + j, 4, 0.0f);
			set_matrixd(C, ln*2 + j, 5, 0.0f);
			set_matrixd(C, ln*2 + j, 6, 1.0f);
		}
		TDelta = new_matrixd(3, ln * 3);
		compute_delta_rotations_matrixd(C, TDelta, sys->delta[i][0], sys->delta[i][1], sys->delta[i][2]);
		
		for (j = 0; j < ln; j++) {
			jid = BM_elem_index_get(vn[j]);
			nlMatrixAdd(i,						jid						, - get_matrixd(TDelta, 0, j));
			nlMatrixAdd(i,						sys->numVerts + jid		, - get_matrixd(TDelta, 0, j + ln));
			nlMatrixAdd(i,						sys->numVerts*2 + jid	, - get_matrixd(TDelta, 0, j + ln *2));
			nlMatrixAdd(i + sys->numVerts,		jid						, - get_matrixd(TDelta, 1, j));
			nlMatrixAdd(i + sys->numVerts,		sys->numVerts + jid		, - get_matrixd(TDelta, 1, j + ln));
			nlMatrixAdd(i + sys->numVerts,		sys->numVerts*2 + jid	, - get_matrixd(TDelta, 1, j + ln *2));
			nlMatrixAdd(i + sys->numVerts*2,	jid						, - get_matrixd(TDelta, 2, j));
			nlMatrixAdd(i + sys->numVerts*2,	sys->numVerts + jid		, - get_matrixd(TDelta, 2, j + ln));
			nlMatrixAdd(i + sys->numVerts*2,	sys->numVerts*2 + jid	, - get_matrixd(TDelta, 2, j + ln *2));

			push_back_triplet(sys->tripletList, i,						jid						, - get_matrixd(TDelta, 0, j));
			push_back_triplet(sys->tripletList, i,						sys->numVerts + jid		, - get_matrixd(TDelta, 0, j + ln));
			push_back_triplet(sys->tripletList, i,						sys->numVerts*2 + jid	, - get_matrixd(TDelta, 0, j + ln *2));
			push_back_triplet(sys->tripletList, i + sys->numVerts,		jid						, - get_matrixd(TDelta, 1, j));
			push_back_triplet(sys->tripletList, i + sys->numVerts,		sys->numVerts + jid		, - get_matrixd(TDelta, 1, j + ln));
			push_back_triplet(sys->tripletList, i + sys->numVerts,		sys->numVerts*2 + jid	, - get_matrixd(TDelta, 1, j + ln *2));
			push_back_triplet(sys->tripletList, i + sys->numVerts*2,	jid						, - get_matrixd(TDelta, 2, j));
			push_back_triplet(sys->tripletList, i + sys->numVerts*2,	sys->numVerts + jid		, - get_matrixd(TDelta, 2, j + ln));
			push_back_triplet(sys->tripletList, i + sys->numVerts*2,	sys->numVerts*2 + jid	, - get_matrixd(TDelta, 2, j + ln *2));

		}
		BLI_array_free(vn);
		BLI_array_empty(vn);
		vn = NULL;
		delete_matrixd(C);
		delete_matrixd(TDelta);

	}
}

/*
	for i=1:n
        j = nbr_i(i);
        pi = xyz(i, :);
        ni = normal(:, i)';
        pj = xyz(j, :);
        uij = pj -pi;
        uij = uij - (dot(uij,ni))*ni;
        uij = uij/max(norm(uij), 0.00001);
        e2 = cross(ni, uij);
        deltai = delta(i,:);
        alpha = dot(ni,deltai);
        beta = dot(uij,deltai);
        gamma = dot(e2,deltai);
        
        pi = xyz_prime_nonh(i,:);
        ni = normal_deform(:, i)';
        pj = xyz_prime_nonh(j,:);
        uij = pj -pi;
        uij = uij - (dot(uij,ni))*ni;
        uij = uij/max(norm(uij), 0.000001);
        e2 = cross(ni, uij);  
        
        new_d = alpha*ni + beta*uij + gamma*e2;
        new_delta(i,:) = new_d;
    end
		*/
void rotate_differential_coordinates(LaplacianSystem *sys)
{
	BMFace *f;
	BMVert *v, *v2;
	BMIter fiter;
	BMIter viter, viter2;
	float alpha, beta, gamma,
		pj[3], ni[3], di[3],
		uij[3], dun[3], e2[3], pi[3], fni[3], vn[4][3];
	int i, j, *vin, lvin, num_fni, k;

	BLI_array_declare(vin);

	BM_ITER_MESH (v, &viter, sys->bm, BM_VERTS_OF_MESH) {
		i = BM_elem_index_get(v);
		copy_v3_v3(pi, sys->cos[i]); //copy_v3_v3(pi, v->co);
		copy_v3_v3(ni, sys->nos[i]); //copy_v3_v3(ni, v->no);
		k = BM_elem_index_get(sys->uverts[i]);
		copy_v3_v3(pj, sys->cos[k]); //copy_v3_v3(pj, sys->uverts[i]->co);
		sub_v3_v3v3(uij, pj, pi);
		mul_v3_v3fl(dun, ni, dot_v3v3(uij, ni));
		sub_v3_v3(uij, dun);
		normalize_v3(uij);
		cross_v3_v3v3(e2, ni, uij);
		copy_v3_v3(di, sys->delta[i]);
		alpha = dot_v3v3(ni, di);
        beta = dot_v3v3(uij, di);
        gamma = dot_v3v3(e2, di);

		pi[0] = nlGetVariable(0, i);
		pi[1] = nlGetVariable(0, i + sys->numVerts);
		pi[2] = nlGetVariable(0, i + sys->numVerts*2);
		ni[0] = 0.0f;	ni[1] = 0.0f;	ni[2] = 0.0f;
		num_fni = 0;
		nlMakeCurrent(sys->context);
		BM_ITER_ELEM_INDEX(f, &fiter, v, BM_FACES_OF_VERT, num_fni) {
			BM_ITER_ELEM(v2, &viter2, f, BM_VERTS_OF_FACE) {
				BLI_array_append(vin, BM_elem_index_get(v2));
			}
			lvin = BLI_array_count(vin);
			
			for (j=0; j<lvin; j++ ) {
				vn[j][0] = nlGetVariable(0, vin[j]);
				vn[j][1] = nlGetVariable(0, vin[j] + sys->numVerts);
				vn[j][2] = nlGetVariable(0, vin[j] + sys->numVerts*2);
				if (j == BM_elem_index_get(sys->uverts[i])) {
					copy_v3_v3(pj, vn[j]);
				}
			}
			if (lvin == 3) {
				normal_tri_v3(fni, vn[0], vn[1], vn[2]);
			} 
			else if(lvin == 4) {
				normal_quad_v3(fni, vn[0], vn[1], vn[2], vn[3]);
			} 
			add_v3_v3(ni, fni);
			if (vin) {
				BLI_array_free(vin);
				BLI_array_empty(vin);
				vin = NULL;
			}
		}
		if (num_fni>0) mul_v3_fl(fni, 1.0f/num_fni);
		sub_v3_v3v3(uij, pj, pi);
		mul_v3_v3fl(dun, ni, dot_v3v3(uij, ni));
		sub_v3_v3(uij, dun);
		normalize_v3(uij);
		cross_v3_v3v3(e2, ni, uij);

		nlMakeCurrent(sys->contextrot);
		nlRightHandSideSet(0, i, alpha*ni[0] + beta*uij[0] + gamma*e2[0]);
		nlRightHandSideSet(1, i, alpha*ni[1] + beta*uij[1] + gamma*e2[1]);
		nlRightHandSideSet(2, i, alpha*ni[2] + beta*uij[2] + gamma*e2[2]);
	}
	
}

void bmo_deform_laplacian_vert_exec(BMesh *bm, BMOperator *op)
{
	int vid;
	BMOIter siter;
	BMVert *v;
	LaplacianSystem *sys;

	if (bm->totface == 0) return;
	init_laplacian_system(sys,  bm->totvert, bm->totvert*3 + 40*3, bm->totvert * 3 );
	if (!sys) return;
	sys->bm = bm;
	sys->op = op;
	memset_laplacian_system(sys, 0);
	BM_mesh_elem_index_ensure(bm, BM_VERT);

	nlNewContext();
	sys->context = nlGetCurrent();
	nlNewContext();
	sys->contextrot = nlGetCurrent();

	nlMakeCurrent(sys->context);
	nlSolverParameteri(NL_NB_VARIABLES, bm->totvert*3);
	nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
	nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
	nlSolverParameteri(NL_NB_ROWS, bm->totvert*3 + 40*3);
	nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 1);

	nlMakeCurrent(sys->contextrot);
	nlSolverParameteri(NL_NB_VARIABLES, bm->totvert);
	nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
	nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
	nlSolverParameteri(NL_NB_ROWS, bm->totvert + 40);
	nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);

	nlMakeCurrent(sys->context);
	nlBegin(NL_SYSTEM);
	nlBegin(NL_MATRIX);

	nlMakeCurrent(sys->contextrot);
	nlBegin(NL_SYSTEM);
	nlBegin(NL_MATRIX);
	
	//init_laplacian_matrix(sys);
	//compute_implicit_rotations(sys);

	/* Block code only for testing*/
	BMO_ITER (v, &siter, sys->op->slots_in, "verts", BM_VERT) {
		vid = BM_elem_index_get(v);
		if (vid < 10) {
			nlMakeCurrent(sys->context);
			nlRightHandSideAdd(0, bm->totvert * 3 + vid * 3	 	, v->co[0]);
			nlRightHandSideAdd(0, bm->totvert * 3 + vid * 3 + 1	, v->co[1]);
			nlRightHandSideAdd(0, bm->totvert * 3 + vid * 3 + 2	, v->co[2]);

			nlMatrixAdd(bm->totvert * 3 + vid * 3		, vid					, 1.0f);
			nlMatrixAdd(bm->totvert * 3	+ vid * 3 + 1   , bm->totvert + vid		, 1.0f);
			nlMatrixAdd(bm->totvert * 3	+ vid * 3 + 2	, 2*bm->totvert + vid	, 1.0f);

			nlMakeCurrent(sys->contextrot);
			nlRightHandSideAdd(0, bm->totvert + vid 	 	, v->co[0]);
			nlRightHandSideAdd(1, bm->totvert + vid 		, v->co[1]);
			nlRightHandSideAdd(2, bm->totvert + vid 		, v->co[2]);

			nlMatrixAdd(bm->totvert + vid 		, vid					, 1.0f);

			set_vectord(sys->VectorB, bm->totvert * 3 + vid * 3	 	, v->co[0]);
			set_vectord(sys->VectorB, bm->totvert * 3 + vid * 3 + 1	, v->co[1]);
			set_vectord(sys->VectorB, bm->totvert * 3 + vid * 3 + 2	, v->co[2]);

			push_back_triplet(sys->tripletList, bm->totvert * 3 + vid * 3		, vid					, 1.0f);
			push_back_triplet(sys->tripletList, bm->totvert * 3	+ vid * 3 + 1   , bm->totvert + vid		, 1.0f);
			push_back_triplet(sys->tripletList, bm->totvert * 3	+ vid * 3 + 2	, 2*bm->totvert + vid	, 1.0f);
			

		} else if (vid <= 39) {
			nlMakeCurrent(sys->context);
			nlRightHandSideAdd(0, bm->totvert * 3 + vid * 3	 	, v->co[0]*2.0f+2.0f);
			nlRightHandSideAdd(0, bm->totvert * 3 + vid * 3 + 1	, v->co[1]*2.0f+2.0f);
			nlRightHandSideAdd(0, bm->totvert * 3 + vid * 3 + 2	, v->co[2]);

			nlMatrixAdd(bm->totvert * 3 + vid * 3		, vid					, 1.0f);
			nlMatrixAdd(bm->totvert * 3	+ vid * 3 + 1   , bm->totvert + vid		, 1.0f);
			nlMatrixAdd(bm->totvert * 3	+ vid * 3 + 2	, 2*bm->totvert + vid	, 1.0f);

			nlMakeCurrent(sys->contextrot);
			nlRightHandSideAdd(0, bm->totvert + vid 	 	, v->co[0]*2.0f+2.0f);
			nlRightHandSideAdd(1, bm->totvert + vid			, v->co[1]*2.0f+2.0f);
			nlRightHandSideAdd(2, bm->totvert + vid			, v->co[2]);

			nlMatrixAdd(bm->totvert + vid 		, vid					, 1.0f);

			set_vectord(sys->VectorB, bm->totvert * 3 + vid * 3	 	, v->co[0]+2.0f);
			set_vectord(sys->VectorB, bm->totvert * 3 + vid * 3 + 1	, v->co[1]+2.0f);
			set_vectord(sys->VectorB, bm->totvert * 3 + vid * 3 + 2	, v->co[2]);

			push_back_triplet(sys->tripletList, bm->totvert * 3 + vid * 3		, vid					, 1.0f);
			push_back_triplet(sys->tripletList, bm->totvert * 3	+ vid * 3 + 1   , bm->totvert + vid		, 1.0f);
			push_back_triplet(sys->tripletList, bm->totvert * 3	+ vid * 3 + 2	, 2*bm->totvert + vid	, 1.0f);
		}
	}


	nlMakeCurrent(sys->context);
	nlEnd(NL_MATRIX);
	nlEnd(NL_SYSTEM);

	/* Solve system with Eigen3*/
	//set_spmatrix_from_triplets(sys->spLapMatrix, sys->tripletList);
	//solve_system(sys->spLapMatrix, sys->VectorB, sys->VectorX);

	if (nlSolveAdvanced(NULL, NL_TRUE) ) {
		rotate_differential_coordinates(sys);
		nlMakeCurrent(sys->contextrot);
		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);
		if (nlSolveAdvanced(NULL, NL_TRUE) ) {
			BMO_ITER (v, &siter, sys->op->slots_in, "verts", BM_VERT) {
				vid = BM_elem_index_get(v);
				/* Solve system with Eigen3*/
				//v->co[0] = get_vectord(sys->VectorX, vid);
				//v->co[1] = get_vectord(sys->VectorX, sys->numVerts + vid);
				//v->co[2] = get_vectord(sys->VectorX, 2*sys->numVerts + vid);
				v->co[0] = nlGetVariable(0, vid);
				v->co[1] = nlGetVariable(1, vid);
				v->co[2] = nlGetVariable(2, vid);
			}
		}
	}

	delete_laplacian_system(sys);
}

///////////////////////////////////////////////////////////////////////////////
enum {
	LAP_MODAL_CANCEL = 1,
	LAP_MODAL_CONFIRM,
	LAP_MODAL_PREVIEW,
	LAP_MODAL_MARK_STATIC,
	LAP_MODAL_MARK_HANDLER
};

static void laplacian_deform_mark_static(bContext *C, wmOperator *op);
static void laplacian_deform_mark_handlers(bContext *C, wmOperator *op);
static void laplacian_deform_preview(bContext *C, wmOperator *op);
static void laplacian_deform_init(struct bContext *C, LaplacianSystem * sys);

static void laplacian_deform_init(struct bContext *C, LaplacianSystem * sys)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMIter viter;
	BMVert *v;
	int count = 0, i;

	sys->numVerts = em->bm->totvert;
	sys->cos =  (float (*)[3])MEM_callocN(sizeof(float) * sys->numVerts * 3, "MeshEditLaplDeformCos");
	sys->nos =  (float (*)[3])MEM_callocN(sizeof(float) * sys->numVerts * 3, "MeshEditLaplDeformNos");
	if (!sys->nos) {
		delete_laplacian_system(sys);
		return ;
	}

	BM_ITER_MESH_INDEX (v, &viter, em->bm, BM_VERTS_OF_MESH, i) {
		copy_v3_v3(sys->cos[i], v->co);
		copy_v3_v3(sys->nos[i], v->no);
	}
}

static int laplacian_deform_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *evt)
{
	LaplacianSystem * sys;
	printf("\ntest_invoke\n");
	sys = op->customdata = MEM_callocN(sizeof(LaplacianSystem), "MeshEditLaplDeformSystem");
	if (!sys) {
		return OPERATOR_CANCELLED;
	}

	laplacian_deform_init(C, sys);
	WM_event_add_modal_handler(C, op);
	return OPERATOR_RUNNING_MODAL;
}

static int laplacian_deform_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	//printf("\ntest_modal\n");
	if(event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case LAP_MODAL_CANCEL:
				return OPERATOR_CANCELLED;
			case LAP_MODAL_MARK_HANDLER:
				printf("\nTST_MODAL_MARK_HANDLER\n");
				laplacian_deform_mark_handlers(C, op);
				break;
			case LAP_MODAL_MARK_STATIC:
				printf("\nTST_MODAL_MARK_STATIC\n");
				laplacian_deform_mark_static(C, op);
				break;
			case LAP_MODAL_PREVIEW:
				printf("\nLAP_MODAL_PREVIEW\n");
				laplacian_deform_preview(C, op);
				break;
			default:
				return OPERATOR_PASS_THROUGH;
				break;

		}
	}
	else {
		return OPERATOR_PASS_THROUGH;
	}
	return OPERATOR_RUNNING_MODAL;

}

static int laplacian_deform_cancel(bContext *C, wmOperator *op)
{
	printf("\ntest_cancel\n");
	return OPERATOR_CANCELLED;
}

wmKeyMap * laplacian_deform_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{LAP_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{LAP_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
		{LAP_MODAL_PREVIEW, "PREVIEW", 0, "Preview", ""},
		{LAP_MODAL_MARK_STATIC, "MARK_STATIC", 0, "Mark Vertex as Static", ""},
		{LAP_MODAL_MARK_HANDLER, "MARK_HANDLER", 0, "Mark Vertex as Handler", ""},
		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Laplacian Deform Modal Map");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items)
		return NULL;

	keymap = WM_modalkeymap_add(keyconf, "Laplacian Deform Modal Map", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, LAP_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, SPACEKEY, KM_PRESS, KM_ANY, 0, LAP_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, SKEY, KM_PRESS, 0, 0, LAP_MODAL_MARK_STATIC);
	WM_modalkeymap_add_item(keymap, HKEY, KM_PRESS, 0, 0, LAP_MODAL_MARK_HANDLER);
	WM_modalkeymap_add_item(keymap, PKEY, KM_PRESS, 0, 0, LAP_MODAL_PREVIEW);

	WM_modalkeymap_assign(keymap, "MESH_OT_vertices_laplacian_deform");

	return keymap;
}

void MESH_OT_vertices_laplacian_deform(wmOperatorType *ot)
{
	ot->name = "Laplacian Deform Edit Mesh tool";
	ot->description = "Laplacian Deform Mesh tool Description";
	ot->idname = "MESH_OT_vertices_laplacian_deform";
	ot->poll = ED_operator_editmesh_view3d;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;
	
	ot->invoke = laplacian_deform_invoke;
	ot->modal = laplacian_deform_modal;
	ot->cancel = laplacian_deform_cancel;
}

static void laplacian_deform_mark_static(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	LaplacianSystem * sys = op->customdata;
	BMIter viter;
	BMVert *v;
	int * static_anchors = NULL, vid, i;
	BLI_array_declare(static_anchors);

	BM_ITER_MESH (v, &viter, em->bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			vid = BM_elem_index_get(v);
			BLI_array_append(static_anchors, vid);
		}
	}
	if (sys->statics_index) {
		MEM_freeN(sys->statics_index);
	}
	sys->statics_index = MEM_callocN(sizeof(int) * BLI_array_count(static_anchors), "MeshEditLaplDeformStaticAnchors");
	
	if (!sys->statics_index) {
		return;
	}
	for (i=0; i<BLI_array_count(static_anchors); i++){
		sys->statics_index[i] = static_anchors[i];
	}
	BLI_array_free(static_anchors);
	
}

static void laplacian_deform_mark_handlers(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	LaplacianSystem * sys = op->customdata;
	BMIter viter;
	BMVert *v;
	BMVert **array_anchors = NULL;
	int vid, i, *array_index = NULL;
	BLI_array_declare(array_index);
	BLI_array_declare(array_anchors);

	BM_ITER_MESH (v, &viter, em->bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			vid = BM_elem_index_get(v);
			BLI_array_append(array_index, vid);
			BLI_array_append(array_anchors, v);
		}
	}
	if (sys->handlers_index) {
		MEM_freeN(sys->handlers_index);
	}
	sys->handlers_index = (int *)MEM_callocN(sizeof(int) * BLI_array_count(array_index), "MeshEditLaplDeformHandlersAnchorsidx");
	
	if (!sys->handlers_index) {
		return;
	}
	if (sys->handlers) {
		MEM_freeN(sys->handlers);
	}
	sys->handlers = (BMVert **)MEM_callocN(sizeof(BMVert *) * BLI_array_count(array_index), "MeshEditLaplDeformHandlersAnchors");
	
	if (!sys->handlers) {
		return;
	}
	for (i=0; i<BLI_array_count(array_index); i++){
		sys->handlers_index[i] = array_index[i];
		sys->handlers[i] = array_anchors[i];
		/*printf("%d = [%f, %f, %f]\n", 
			sys->handlers_index[i], 
			sys->handlers[i]->co[0], 
			sys->handlers[i]->co[1], 
			sys->handlers[i]->co[2]);*/

	}
	BLI_array_free(array_index);
	BLI_array_free(array_anchors);
	
}

static void laplacian_deform_preview(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	LaplacianSystem * sys = op->customdata;
	struct BMesh *bm = em->bm;
	int vid, i;
	BMIter viter;
	BMVert *v;

	//printf("\nline 1");
	if (bm->totface == 0) return;
	init_laplacian_system(sys,  bm->totvert, bm->totvert*3 + (sys->numHandlers + sys->numStatics) *3, bm->totvert * 3 );
	//printf("\nline 1");
	if (!sys) return;
	sys->bm = bm;
	//sys->op = op;
	memset_laplacian_system(sys, 0);
	BM_mesh_elem_index_ensure(bm, BM_VERT);

	//printf("\nline 2");
	nlNewContext();
	sys->context = nlGetCurrent();
	nlNewContext();
	sys->contextrot = nlGetCurrent();

	nlMakeCurrent(sys->context);
	nlSolverParameteri(NL_NB_VARIABLES, bm->totvert*3);
	nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
	nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
	nlSolverParameteri(NL_NB_ROWS, bm->totvert*3 + (sys->numHandlers + sys->numStatics)*3);
	nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 1);

	nlMakeCurrent(sys->contextrot);
	nlSolverParameteri(NL_NB_VARIABLES, bm->totvert);
	nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
	nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
	nlSolverParameteri(NL_NB_ROWS, bm->totvert + (sys->numHandlers + sys->numStatics));
	nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);

	nlMakeCurrent(sys->context);
	nlBegin(NL_SYSTEM);
	nlBegin(NL_MATRIX);

	nlMakeCurrent(sys->contextrot);
	nlBegin(NL_SYSTEM);
	nlBegin(NL_MATRIX);
	
	//printf("\nline 3");
	init_laplacian_matrix(sys);
	//printf("\nline 4");
	compute_implict_rotations(sys);
	//printf("\nline 5");

	/* Block code only for testing*/

	for (i=0; i<sys->numStatics; i++) {
		vid = sys->statics_index[i];
		nlMakeCurrent(sys->context);
		nlRightHandSideAdd(0, bm->totvert * 3 + i * 3	 	, sys->cos[vid][0]);
		nlRightHandSideAdd(0, bm->totvert * 3 + i * 3 + 1	, sys->cos[vid][1]);
		nlRightHandSideAdd(0, bm->totvert * 3 + i * 3 + 2	, sys->cos[vid][2]);

		nlMatrixAdd(bm->totvert * 3 + i * 3			, vid					, 1.0f);
		nlMatrixAdd(bm->totvert * 3	+ i * 3 + 1		, bm->totvert + vid		, 1.0f);
		nlMatrixAdd(bm->totvert * 3	+ i * 3 + 2		, 2*bm->totvert + vid	, 1.0f);

		nlMakeCurrent(sys->contextrot);
		nlRightHandSideAdd(0, bm->totvert + i 	 	, sys->cos[vid][0]);
		nlRightHandSideAdd(1, bm->totvert + i 		, sys->cos[vid][1]);
		nlRightHandSideAdd(2, bm->totvert + i 		, sys->cos[vid][2]);

		nlMatrixAdd(bm->totvert + vid 		, vid					, 1.0f);

		set_vectord(sys->VectorB, bm->totvert * 3 + vid * 3	 	, sys->cos[vid][0]);
		set_vectord(sys->VectorB, bm->totvert * 3 + vid * 3 + 1	, sys->cos[vid][1]);
		set_vectord(sys->VectorB, bm->totvert * 3 + vid * 3 + 2	, sys->cos[vid][2]);

		push_back_triplet(sys->tripletList, bm->totvert * 3 + i * 3			, vid					, 1.0f);
		push_back_triplet(sys->tripletList, bm->totvert * 3	+ i * 3 + 1		, bm->totvert + vid		, 1.0f);
		push_back_triplet(sys->tripletList, bm->totvert * 3	+ i * 3 + 2		, 2*bm->totvert + vid	, 1.0f);
		
	}

	//printf("\nline 6");
	for (i=0; i<sys->numHandlers; i++)
	{
		vid = sys->handlers_index[i];
		nlMakeCurrent(sys->context);
		nlRightHandSideAdd(0, bm->totvert * 3 + sys->numStatics + i * 3	 	, sys->handlers[vid]->co[0]);
		nlRightHandSideAdd(0, bm->totvert * 3 + sys->numStatics + i * 3 + 1	, sys->handlers[vid]->co[1]);
		nlRightHandSideAdd(0, bm->totvert * 3 + sys->numStatics + i * 3 + 2	, sys->handlers[vid]->co[2]);

		nlMatrixAdd(bm->totvert * 3 + sys->numStatics + i * 3		, vid					, 1.0f);
		nlMatrixAdd(bm->totvert * 3	+ sys->numStatics + i * 3 + 1   , bm->totvert + vid		, 1.0f);
		nlMatrixAdd(bm->totvert * 3	+ sys->numStatics + i * 3 + 2	, 2*bm->totvert + vid	, 1.0f);

		nlMakeCurrent(sys->contextrot);
		nlRightHandSideAdd(0, bm->totvert + sys->numStatics + i 	 	, sys->handlers[vid]->co[0]);
		nlRightHandSideAdd(1, bm->totvert + sys->numStatics + i			, sys->handlers[vid]->co[1]);
		nlRightHandSideAdd(2, bm->totvert + sys->numStatics + i			, sys->handlers[vid]->co[2]);

		nlMatrixAdd(bm->totvert + sys->numStatics + i 		, vid					, 1.0f);

		set_vectord(sys->VectorB, bm->totvert * 3 + sys->numStatics + i * 3	 	, sys->handlers[vid]->co[0]);
		set_vectord(sys->VectorB, bm->totvert * 3 + sys->numStatics + i * 3 + 1	, sys->handlers[vid]->co[1]);
		set_vectord(sys->VectorB, bm->totvert * 3 + sys->numStatics + i * 3 + 2	, sys->handlers[vid]->co[2]);

		push_back_triplet(sys->tripletList, bm->totvert * 3 + sys->numStatics + i * 3		, vid					, 1.0f);
		push_back_triplet(sys->tripletList, bm->totvert * 3	+ sys->numStatics + i * 3 + 1   , bm->totvert + vid		, 1.0f);
		push_back_triplet(sys->tripletList, bm->totvert * 3	+ sys->numStatics + i * 3 + 2	, 2*bm->totvert + vid	, 1.0f);
	}


	nlMakeCurrent(sys->context);
	nlEnd(NL_MATRIX);
	nlEnd(NL_SYSTEM);

	/* Solve system with Eigen3*/
	//set_spmatrix_from_triplets(sys->spLapMatrix, sys->tripletList);
	//solve_system(sys->spLapMatrix, sys->VectorB, sys->VectorX);

	if (nlSolveAdvanced(NULL, NL_TRUE) ) {
		rotate_differential_coordinates(sys);
		nlMakeCurrent(sys->contextrot);
		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);
		if (nlSolveAdvanced(NULL, NL_TRUE) ) {
			//BMO_ITER (v, &siter, sys->op->slots_in, "verts", BM_VERT) {
			BM_ITER_MESH (v, &viter, em->bm, BM_VERTS_OF_MESH) {
				vid = BM_elem_index_get(v);
				/* Solve system with Eigen3*/
				//v->co[0] = get_vectord(sys->VectorX, vid);
				//v->co[1] = get_vectord(sys->VectorX, sys->numVerts + vid);
				//v->co[2] = get_vectord(sys->VectorX, 2*sys->numVerts + vid);
				v->co[0] = nlGetVariable(0, vid);
				v->co[1] = nlGetVariable(1, vid);
				v->co[2] = nlGetVariable(2, vid);
			}
		}
	}

	//delete_laplacian_system(sys);
}