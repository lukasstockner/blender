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

/** \file blender/editors/mesh/editmesh_deform_laplacian.c
 *  \ingroup mesh
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
#include "BLI_string.h"
#include "ONL_opennl.h"
#include "BLF_translation.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"
#include "ED_mesh.h"

struct BStaticAnchors {
	int numStatics;				/* Number of static anchors*/
	int numVerts;				/* Number of verts*/
	int * list_index;			/* Static vertex index list*/
	float (*co)[3];				/* Original vertex coordinates*/
	float (*no)[3];				/* Original vertex normal*/
	BMVert ** list_verts;		/* Vertex order by index*/
};
typedef struct BStaticAnchors StaticAnchors;

struct BHandlerAnchors {
	int numHandlers;			/* Number of handler anchors*/
	int * list_handlers;		/* Static vertex index list*/
};
typedef struct BHandlerAnchors HandlerAnchors;

struct BLaplacianSystem {
	float (*delta)[3];			/* Differential Coordinates*/
	int *list_uverts;			/* Unit vectors of projected edges onto the plane orthogonal to  n*/
	/* Pointers to data*/
	int numVerts;
	int numHandlers;
	int numStatics;
	BMesh *bm;
	NLContext *context;			/* System for solve general implicit rotations*/
};
typedef struct BLaplacianSystem LaplacianSystem;

enum {
	LAP_STATE_INIT = 1,
	LAP_STATE_HAS_STATIC,
	LAP_STATE_HAS_HANDLER,
	LAP_STATE_HAS_STATIC_AND_HANDLER,
	LAP_STATE_HAS_L_COMPUTE,
	LAP_STATE_UPDATE_REQUIRED
};

struct BSystemCustomData {
	LaplacianSystem * sys;
	StaticAnchors  * sa;
	HandlerAnchors * shs;
	int stateSystem;
	bool update_required;
};

typedef struct BSystemCustomData SystemCustomData;


enum {
	LAP_MODAL_CANCEL = 1,
	LAP_MODAL_CONFIRM,
	LAP_MODAL_PREVIEW,
	LAP_MODAL_MARK_STATIC,
	LAP_MODAL_MARK_HANDLER,
	LAP_MODAL_TRANSFORM, 
	LAP_MODAL_NOTHING 
};

wmKeyMap * laplacian_deform_modal_keymap(wmKeyConfig *keyconf);
void MESH_OT_vertices_laplacian_deform(wmOperatorType *ot);
static StaticAnchors * init_static_anchors(int numv, int nums);
static HandlerAnchors * init_handler_anchors(int numh);
static LaplacianSystem * init_laplacian_system(int numv, int nums, int numh);
static float cotan_weight(float *v1, float *v2, float *v3);
static int laplacian_deform_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *evt);
static int laplacian_deform_modal(bContext *C, wmOperator *op, const wmEvent *event);
static int laplacian_deform_cancel(bContext *C, wmOperator *op);
static void laplacian_deform_update_header(bContext *C);
static void compute_implict_rotations(SystemCustomData * data);
static void delete_void_pointer(void *data);
static void delete_static_anchors(StaticAnchors * sa);
static void delete_handler_anchors(HandlerAnchors * sh);
static void delete_laplacian_system(LaplacianSystem *sys);
static void init_laplacian_matrix( SystemCustomData * data);
static void laplacian_deform_mark_static(bContext *C, wmOperator *op);
static void laplacian_deform_mark_handlers(bContext *C, wmOperator *op);
static void laplacian_deform_preview(bContext *C, wmOperator *op);
static void laplacian_deform_init(struct bContext *C, LaplacianSystem * sys);
static void rotate_differential_coordinates(SystemCustomData * data);
static void update_system_state(SystemCustomData * data, int state);

static void delete_void_pointer(void *data)
{
	if (data) {
		MEM_freeN(data);
	}
}

static StaticAnchors * init_static_anchors(int numv, int nums)
{
	StaticAnchors * sa;
	sa = (StaticAnchors *)MEM_callocN(sizeof(StaticAnchors), "LapStaticAnchors");
	sa->numVerts = numv;
	sa->numStatics = nums;
	sa->list_index = (int *)MEM_callocN(sizeof(int)*(sa->numStatics), "LapListStatics");
	sa->list_verts = (BMVert**)MEM_callocN(sizeof(BMVert*)*(sa->numVerts), "LapListverts");
	sa->co = (float (*)[3])MEM_callocN(sizeof(float)*(sa->numVerts*3), "LapCoordinates");
	sa->no = (float (*)[3])MEM_callocN(sizeof(float)*(sa->numVerts*3), "LapNormals");
	memset(sa->no, 0.0, sizeof(float) * sa->numVerts * 3);
	return sa;
}

static HandlerAnchors * init_handler_anchors(int numh)
{
	HandlerAnchors * sh;
	sh = (HandlerAnchors *)MEM_callocN(sizeof(HandlerAnchors), "LapHandlerAnchors");
	sh->numHandlers = numh;
	sh->list_handlers = (int *)MEM_callocN(sizeof(int)*(sh->numHandlers), "LapListHandlers");
	return sh;
}

static LaplacianSystem * init_laplacian_system(int numv, int nums, int numh)
{
	LaplacianSystem *sys;
	int rows, cols;
	sys = (LaplacianSystem *)MEM_callocN(sizeof(LaplacianSystem), "LapSystem");
	if (!sys) {
		return NULL;
	}
	sys->numVerts = numv;
	sys->numStatics = nums;
	sys->numHandlers = numh;
	rows = (sys->numVerts + sys->numStatics + sys->numHandlers) * 3;
	cols = sys->numVerts * 3;
	sys->list_uverts = (int *)MEM_callocN(sizeof(BMVert *) * sys->numVerts, "LapUverts");
	sys->delta = (float (*)[3])MEM_callocN(sizeof(float) * sys->numVerts * 3, "LapDelta");
	memset(sys->delta, 0.0, sizeof(float) * sys->numVerts * 3);
	return sys;
}

static void delete_static_anchors(StaticAnchors * sa)
{
	if (!sa) return;
	delete_void_pointer(sa->co);
	delete_void_pointer(sa->list_index);
	delete_void_pointer(sa->no);
	delete_void_pointer(sa->list_verts);
	delete_void_pointer(sa);
	sa = NULL;
}

static void delete_handler_anchors(HandlerAnchors * sh)
{
	if (!sh) return;
	delete_void_pointer(sh->list_handlers);
	delete_void_pointer(sh);
	sh = NULL;
}

static void delete_laplacian_system(LaplacianSystem *sys)
{
	if (!sys) return;
	delete_void_pointer(sys->delta);
	delete_void_pointer(sys->list_uverts);
	sys->bm = NULL;
	if (sys->context) nlDeleteContext(sys->context);
	delete_void_pointer(sys);
	sys = NULL;
}

void MESH_OT_vertices_laplacian_deform(wmOperatorType *ot)
{
	ot->name = "Laplacian Deform Edit Mesh tool";
	ot->description = "Laplacian Deform Mesh tool";
	ot->idname = "MESH_OT_vertices_laplacian_deform";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;
	ot->invoke = laplacian_deform_invoke;
	ot->modal = laplacian_deform_modal;
	ot->cancel = laplacian_deform_cancel;
	ot->poll = ED_operator_editmesh_view3d;
}

static void update_system_state(SystemCustomData * data, int state)
{
	if (!data) return;
	switch(data->stateSystem) {
		case LAP_STATE_INIT:
			if (state == LAP_STATE_HAS_STATIC || state == LAP_STATE_HAS_HANDLER) {
				data->stateSystem = state;
			}
			break;
		case LAP_STATE_HAS_STATIC:
			if (state == LAP_STATE_HAS_HANDLER) {
				data->stateSystem = LAP_STATE_HAS_STATIC_AND_HANDLER;
			}
			break;
		case LAP_STATE_HAS_HANDLER:
			if (state == LAP_STATE_HAS_STATIC)  {
				data->stateSystem = LAP_STATE_HAS_STATIC_AND_HANDLER;
			}
			break;
		case LAP_STATE_HAS_STATIC_AND_HANDLER:
			if (state == LAP_STATE_HAS_L_COMPUTE) {
				data->stateSystem = LAP_STATE_HAS_L_COMPUTE;
			} 
			break;
		case LAP_STATE_HAS_L_COMPUTE:
			if (state == LAP_STATE_HAS_STATIC || state == LAP_STATE_HAS_HANDLER) {
				data->stateSystem = LAP_STATE_HAS_STATIC_AND_HANDLER;
			}
			break;
	}

}

static int laplacian_deform_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *evt)
{
	SystemCustomData * sys;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	sys = op->customdata = MEM_callocN(sizeof(SystemCustomData), "LapSystemCustomData");
	if (!sys) {
		return OPERATOR_CANCELLED;
	}
	sys->sa = NULL;
	sys->shs = NULL;
	sys->sys = NULL;
	sys->stateSystem = LAP_STATE_INIT;
	BM_mesh_elem_index_ensure(em->bm, BM_VERT);

	WM_event_add_modal_handler(C, op);
	laplacian_deform_update_header(C);
	return OPERATOR_RUNNING_MODAL;
}

static void laplacian_deform_mark_static(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	SystemCustomData * data = op->customdata;
	int vid, i, nums;
	BMIter viter;
	BMVert *v;

	nums = BM_iter_mesh_count_flag(BM_VERTS_OF_MESH, em->bm, BM_ELEM_SELECT, true);
	if (data->sa) {
		if (data->sa->numVerts != em->bm->totvert) {
			delete_static_anchors(data->sa);
			data->sa = init_static_anchors(em->bm->totvert, nums);
		}
		else {
			delete_void_pointer( data->sa->list_index);
			data->sa->numStatics = nums;
			data->sa->list_index = (int *)MEM_callocN(sizeof(int)*(data->sa->numStatics), "LapListStatics");
		}
	} 
	else {
		data->sa = init_static_anchors(em->bm->totvert, nums);
	}

	i=0;
	BM_ITER_MESH (v, &viter, em->bm, BM_VERTS_OF_MESH) {
		vid = BM_elem_index_get(v);
		copy_v3_v3(data->sa->co[vid], v->co);
		data->sa->list_verts[vid] = v;
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			data->sa->list_index[i] = vid;
			i = i + 1;
		}
	}
	update_system_state(data, LAP_STATE_HAS_STATIC);
}

static void laplacian_deform_mark_handlers(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	SystemCustomData * data = op->customdata;
	BMIter viter;
	BMVert *v;
	int vid, i, numh;
	numh = BM_iter_mesh_count_flag(BM_VERTS_OF_MESH, em->bm, BM_ELEM_SELECT, true);
	if (data->shs) {
		delete_handler_anchors(data->shs);
	}
	data->shs = init_handler_anchors(numh);
	i = 0;
	BM_ITER_MESH (v, &viter, em->bm, BM_VERTS_OF_MESH) {
		vid = BM_elem_index_get(v);
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			data->shs->list_handlers[i] = vid;
			i = i + 1;
		}
	}
	update_system_state(data, LAP_STATE_HAS_HANDLER);
}

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

static void init_laplacian_matrix( SystemCustomData * data)
{
	float v1[3], v2[3], v3[3], v4[3], no[3], cf[3], nt[3];
	float w2, w3, w4;
	int i, j, vid, vidf[4];
	bool has_4_vert;
	unsigned int idv1, idv2, idv3, idv4, idv[4];
	BMFace *f;
	BMIter fiter;
	BMIter vi;
	BMVert *vn;
	LaplacianSystem * sys = data->sys;
	StaticAnchors * sa = data->sa;

	BM_ITER_MESH (f, &fiter, sys->bm, BM_FACES_OF_MESH) {
	
		BM_ITER_ELEM_INDEX (vn, &vi, f, BM_VERTS_OF_FACE, i) {
			vid = BM_elem_index_get(vn);
			vidf[i] = vid;
		}
		has_4_vert = (i == 4) ? 1 : 0;
		idv1 = vidf[0];
		idv2 = vidf[1];
		idv3 = vidf[2];
		idv4 = has_4_vert ? vidf[3] : 0;
		if (has_4_vert) {
			normal_quad_v3(no, sa->co[idv1], sa->co[idv2], sa->co[idv3], sa->co[idv4]); 
			add_v3_v3v3(cf, sa->co[idv1], sa->co[idv2]);
			add_v3_v3(cf, sa->co[idv3]);
			add_v3_v3(cf, sa->co[idv4]);
			mul_v3_fl(cf, 1.0f/4.0f);

			mul_v3_v3fl(nt, no, len_v3v3(cf, sa->co[idv1]));
			add_v3_v3(sa->no[idv1], nt);

			mul_v3_v3fl(nt, no, len_v3v3(cf, sa->co[idv2]));
			add_v3_v3(sa->no[idv2], nt);

			mul_v3_v3fl(nt, no, len_v3v3(cf, sa->co[idv3]));
			add_v3_v3(sa->no[idv3], nt);

			mul_v3_v3fl(nt, no, len_v3v3(cf, sa->co[idv4]));
			add_v3_v3(sa->no[idv4], nt);
		} 
		else {
			normal_tri_v3(no, sa->co[idv1], sa->co[idv2], sa->co[idv3]); 
			add_v3_v3v3(cf, sa->co[idv1], sa->co[idv2]);
			add_v3_v3(cf, sa->co[idv3]);
			mul_v3_fl(cf, 1.0f/3.0f);

			mul_v3_v3fl(nt, no, len_v3v3(cf, sa->co[idv1]));
			add_v3_v3(sa->no[idv1], nt);

			mul_v3_v3fl(nt, no, len_v3v3(cf, sa->co[idv2]));
			add_v3_v3(sa->no[idv2], nt);

			mul_v3_v3fl(nt, no, len_v3v3(cf, sa->co[idv3]));
			add_v3_v3(sa->no[idv3], nt);
		}


		idv[0] = idv1;
		idv[1] = idv2;
		idv[2] = idv3;
		idv[3] = idv4;

		/*nlMakeCurrent(sys->context);
		nlRightHandSideSet(0, idv1						, 0.0f);
		nlRightHandSideSet(0, sys->numVerts + idv1		, 0.0f);
		nlRightHandSideSet(0, 2*sys->numVerts + idv1	, 0.0f);*/

		for (j = 0; j < i; j++) {
			idv1 = idv[j];
			idv2 = idv[(j + 1) % i];
			idv3 = idv[(j + 2) % i];
			idv4 = has_4_vert ? idv[(j + 3) % i] : 0;

			copy_v3_v3( v1, sa->co[idv1]);
			copy_v3_v3( v2, sa->co[idv2]);
			copy_v3_v3( v3, sa->co[idv3]);
			if (has_4_vert) copy_v3_v3(v4, sa->co[idv4]);

			if (has_4_vert) {

				w2 = (cotan_weight(v4, v1, v2) + cotan_weight(v3, v1, v2)) /2.0f ;
				w3 = (cotan_weight(v2, v3, v1) + cotan_weight(v4, v1, v3)) /2.0f ;
				w4 = (cotan_weight(v2, v4, v1) + cotan_weight(v3, v4, v1)) /2.0f;

				sys->delta[idv1][0] -=  v4[0] * w4;
				sys->delta[idv1][1] -=  v4[1] * w4;
				sys->delta[idv1][2] -=  v4[2] * w4;

				nlRightHandSideAdd(0, idv1, -v4[0] * w4);
				nlRightHandSideAdd(1, idv1, -v4[1] * w4);
				nlRightHandSideAdd(2, idv1, -v4[2] * w4);

				nlMatrixAdd(idv1, idv4, -w4 );				
			}
			else {
				w2 = cotan_weight(v3, v1, v2);
				w3 = cotan_weight(v2, v3, v1);
				w4 = 0.0f;
			}

			sys->delta[idv1][0] +=  v1[0] * (w2 + w3 + w4);
			sys->delta[idv1][1] +=  v1[1] * (w2 + w3 + w4);
			sys->delta[idv1][2] +=  v1[2] * (w2 + w3 + w4);

			sys->delta[idv1][0] -=  v2[0] * w2;
			sys->delta[idv1][1] -=  v2[1] * w2;
			sys->delta[idv1][2] -=  v2[2] * w2;

			sys->delta[idv1][0] -=  v3[0] * w3;
			sys->delta[idv1][1] -=  v3[1] * w3;
			sys->delta[idv1][2] -=  v3[2] * w3;

			nlRightHandSideAdd(0, idv1	, v1[0] * (w2 + w3 + w4));
			nlRightHandSideAdd(1, idv1	, v1[1] * (w2 + w3 + w4));
			nlRightHandSideAdd(2, idv1	, v1[2] * (w2 + w3 + w4));

			nlRightHandSideAdd(0, idv1	, -v2[0] * w2);
			nlRightHandSideAdd(1, idv1	, -v2[1] * w2);
			nlRightHandSideAdd(2, idv1	, -v2[2] * w2);

			nlRightHandSideAdd(0, idv1	, -v3[0] * w3);
			nlRightHandSideAdd(1, idv1	, -v3[1] * w3);
			nlRightHandSideAdd(2, idv1	, -v3[2] * w3);

			nlMatrixAdd(idv1, idv2, -w2);
			nlMatrixAdd(idv1, idv3, -w3);
			nlMatrixAdd(idv1, idv1, w2 + w3 + w4);
			
		}
	}
}


static void compute_implict_rotations(SystemCustomData * data)
{
	BMEdge *e;
	BMIter eiter;
	BMIter viter;
	BMVert *v;
	int vid, * vidn = NULL;
	float minj, mjt, qj[3], vj[3];
	int i, j, ln;
	LaplacianSystem * sys = data->sys;
	StaticAnchors * sa = data->sa;
	BLI_array_declare(vidn);

	BM_ITER_MESH (v, &viter, sys->bm, BM_VERTS_OF_MESH) {
		i = BM_elem_index_get(v);
		normalize_v3( sa->no[i]);
		BM_ITER_ELEM(e, &eiter, v, BM_EDGES_OF_VERT) {
			vid = BM_elem_index_get(e->v1);
			if (vid == i) {
				vid = BM_elem_index_get(e->v2);
				BLI_array_append(vidn, vid);
			}
			else {
				BLI_array_append(vidn, vid);
			}
		}
		BLI_array_append(vidn, i);
		ln = BLI_array_count(vidn);
		minj = 1000000.0f;
		for (j = 0; j < (ln-1); j++) {
			vid = vidn[j];
			copy_v3_v3(qj, sa->co[vid]);// vn[j]->co;
			sub_v3_v3v3(vj, qj, sa->co[i]); //sub_v3_v3v3(vj, qj, v->co);
			normalize_v3(vj);
			mjt = fabs(dot_v3v3(vj, sa->no[i])); //mjt = fabs(dot_v3v3(vj, v->no));
			if (mjt < minj) {
				minj = mjt;
				sys->list_uverts[i] = vidn[j];
			}
		}
		
		BLI_array_free(vidn);
		BLI_array_empty(vidn);
		vidn = NULL;
	}
}

static void rotate_differential_coordinates(SystemCustomData * data)
{
	BMFace *f;
	BMVert *v, *v2;
	BMIter fiter;
	BMIter viter, viter2;
	float alpha, beta, gamma,
		pj[3], ni[3], di[3], cf[3],
		uij[3], dun[3], e2[3], pi[3], fni[3], vn[4][3];
	int i, j, vin[4], lvin, num_fni, k;
	LaplacianSystem * sys = data->sys;
	StaticAnchors * sa = data->sa;


	BM_ITER_MESH (v, &viter, sys->bm, BM_VERTS_OF_MESH) {
		i = BM_elem_index_get(v);
		copy_v3_v3(pi, sa->co[i]); //copy_v3_v3(pi, v->co);
		copy_v3_v3(ni, sa->no[i]); //copy_v3_v3(ni, v->no);
		k = sys->list_uverts[i];
		copy_v3_v3(pj, sa->co[k]); //copy_v3_v3(pj, sys->uverts[i]->co);
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
		pi[1] = nlGetVariable(1, i);
		pi[2] = nlGetVariable(2, i);
		ni[0] = 0.0f;	ni[1] = 0.0f;	ni[2] = 0.0f;
		num_fni = 0;
		BM_ITER_ELEM_INDEX(f, &fiter, v, BM_FACES_OF_VERT, num_fni) {
			BM_ITER_ELEM_INDEX(v2, &viter2, f, BM_VERTS_OF_FACE, j) {
				vin[j] = BM_elem_index_get(v2);
			}
			lvin = j;

			
			for (j=0; j<lvin; j++ ) {
				vn[j][0] = nlGetVariable(0, vin[j]);
				vn[j][1] = nlGetVariable(1, vin[j]);
				vn[j][2] = nlGetVariable(2, vin[j]);
				if (vin[j] == sys->list_uverts[i]) {
					copy_v3_v3(pj, vn[j]);
				}
			}

			if (lvin == 3) {
				normal_tri_v3(fni, vn[0], vn[1], vn[2]);
				add_v3_v3v3(cf, vn[0], vn[1]);
				add_v3_v3(cf, vn[2]);
				mul_v3_fl(cf, 1.0f/3.0f);
				mul_v3_fl(fni, len_v3v3(cf, pi));
				
			} 
			else if(lvin == 4) {
				normal_quad_v3(fni, vn[0], vn[1], vn[2], vn[3]);
				add_v3_v3v3(cf, vn[0], vn[1]);
				add_v3_v3(cf, vn[2]);
				add_v3_v3(cf, vn[3]);
				mul_v3_fl(cf, 1.0f/4.0f);
				mul_v3_fl(fni, len_v3v3(cf, pi));
			} 

			add_v3_v3(ni, fni);

		}

		normalize_v3(ni);
		sub_v3_v3v3(uij, pj, pi);
		mul_v3_v3fl(dun, ni, dot_v3v3(uij, ni));
		sub_v3_v3(uij, dun);
		normalize_v3(uij);
		cross_v3_v3v3(e2, ni, uij);
		fni[0] = alpha*ni[0] + beta*uij[0] + gamma*e2[0];
		fni[1] = alpha*ni[1] + beta*uij[1] + gamma*e2[1];
		fni[2] = alpha*ni[2] + beta*uij[2] + gamma*e2[2];

		if (len_v3(fni) > FLT_EPSILON) {
			nlRightHandSideSet(0, i, fni[0]);
			nlRightHandSideSet(1, i, fni[1]);
			nlRightHandSideSet(2, i, fni[2]);
		} 
		else {
			nlRightHandSideSet(0, i, sys->delta[i][0]);
			nlRightHandSideSet(1, i, sys->delta[i][1]);
			nlRightHandSideSet(2, i, sys->delta[i][2]);
		}


	}
	
}

static int laplacian_deform_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	SystemCustomData * data =  op->customdata;
	if (data) if (data->update_required) {
		data->update_required = false;
		printf("\nLAP_MODAL_PREVIEW\n");
		laplacian_deform_preview(C, op);
		laplacian_deform_update_header(C);
		ED_region_tag_redraw(CTX_wm_region(C));
		return OPERATOR_RUNNING_MODAL;
	}

	if(event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case LAP_MODAL_CANCEL:
				printf("\nLAP_MODAL_CANCEL\n");
				ED_area_headerprint(CTX_wm_area(C), NULL);
				laplacian_deform_cancel(C, op);
				return OPERATOR_CANCELLED;
			case LAP_MODAL_MARK_HANDLER:
				printf("\nLAP_MODAL_MARK_HANDLER\n");
				laplacian_deform_mark_handlers(C, op);
				laplacian_deform_update_header(C);
				break;
			case LAP_MODAL_MARK_STATIC:
				printf("\nLAP_MODAL_MARK_STATIC\n");
				laplacian_deform_mark_static(C, op);
				laplacian_deform_update_header(C);
				break;
			case LAP_MODAL_PREVIEW:
				printf("\nLAP_MODAL_PREVIEW\n");
				laplacian_deform_preview(C, op);
				laplacian_deform_update_header(C);
				ED_region_tag_redraw(CTX_wm_region(C));
				break;
			case LAP_MODAL_TRANSFORM:
				printf("\nLAP_MODAL_TRANSFORM\n");
				if (data) {
					data->update_required = true;
				}
				return OPERATOR_PASS_THROUGH;
				break;
			case LAP_MODAL_NOTHING:
				printf("\nLAP_MODAL_NOTHING\n");
				return OPERATOR_PASS_THROUGH;
				break;
			/*default:
				return OPERATOR_PASS_THROUGH;
				break;*/
		}
	}
	else {
		if (event->type >= LEFTMOUSE && event->type <= WHEELOUTMOUSE) {
			return OPERATOR_PASS_THROUGH;
		}
	}
	return OPERATOR_RUNNING_MODAL;

}

static int laplacian_deform_cancel(bContext *C, wmOperator *op)
{
	SystemCustomData * data;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	data = op->customdata;
	if (data) {
		delete_laplacian_system(data->sys);
		delete_static_anchors(data->sa);
		delete_handler_anchors(data->shs);
		delete_void_pointer(data);
	}

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
		{LAP_MODAL_TRANSFORM, "TRANSFROM", 0, "Transform Verts", ""},
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

	WM_modalkeymap_add_item(keymap, QKEY, KM_PRESS, 0, 0, LAP_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, JKEY, KM_PRESS, 0, 0, LAP_MODAL_MARK_STATIC);
	WM_modalkeymap_add_item(keymap, HKEY, KM_PRESS, 0, 0, LAP_MODAL_MARK_HANDLER);
	WM_modalkeymap_add_item(keymap, PKEY, KM_PRESS, 0, 0, LAP_MODAL_PREVIEW);

	WM_modalkeymap_add_item(keymap, SKEY, KM_PRESS, 0, 0, LAP_MODAL_TRANSFORM);
	WM_modalkeymap_add_item(keymap, RKEY, KM_PRESS, 0, 0, LAP_MODAL_TRANSFORM);
	WM_modalkeymap_add_item(keymap, GKEY, KM_PRESS, 0, 0, LAP_MODAL_TRANSFORM);

	WM_modalkeymap_add_item(keymap, ZKEY, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, TKEY, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, NKEY, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, BKEY, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, CKEY, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, AKEY, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD0, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD1, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD2, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD3, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD4, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD5, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD6, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD7, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD8, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PAD9, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PADMINUS, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	WM_modalkeymap_add_item(keymap, PADPLUSKEY, KM_PRESS, 0, 0, LAP_MODAL_NOTHING);
	

	WM_modalkeymap_assign(keymap, "MESH_OT_vertices_laplacian_deform");

	return keymap;
}

static void laplacian_deform_update_header(bContext *C)
{
	#define HEADER_LENGTH 256
	char header[HEADER_LENGTH];

	BLI_snprintf(header, HEADER_LENGTH, IFACE_("First Mark Static Anchors J, Second Mark Handler Anchors H, "
		"Third Transform Handler Anchors, Fourth Compute solution P, Esc or Q: cancel, "));

	ED_area_headerprint(CTX_wm_area(C), header);
}

static void laplacian_deform_preview(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	SystemCustomData * data = op->customdata;
	LaplacianSystem * sys;
	StaticAnchors * sa;
	HandlerAnchors * shs;
	struct BMesh *bm = em->bm;
	int vid, i, n, ns, nh;
	BMIter viter;
	BMVert *v;

	if (data->stateSystem < LAP_STATE_HAS_STATIC_AND_HANDLER) return;

	if (data->stateSystem == LAP_STATE_HAS_STATIC_AND_HANDLER ) {
		if (data->sys) {
			delete_laplacian_system(data->sys);
		}
		data->sys = init_laplacian_system(data->sa->numVerts, data->sa->numStatics, data->shs->numHandlers);
		sys = data->sys;
		sa = data->sa;
		shs = data->shs;
		sys->bm = bm;
		n = sys->numVerts;
		ns = sa->numStatics;
		nh = shs->numHandlers;
		nlNewContext();
		sys->context = nlGetCurrent();

		nlSolverParameteri(NL_NB_VARIABLES, n);
		nlSolverParameteri(NL_SYMMETRIC, NL_FALSE);
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
		nlSolverParameteri(NL_NB_ROWS, n + nh + ns);
		nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);

		nlBegin(NL_SYSTEM);
		for (i=0; i<n; i++) {
			nlSetVariable(0, i, sa->co[i][0]);
			nlSetVariable(1, i, sa->co[i][1]);
			nlSetVariable(2, i, sa->co[i][2]);
		}
		for (i=0; i<nh; i++) {
			vid = shs->list_handlers[i];
			nlSetVariable(0, vid, sa->list_verts[vid]->co[0]);
			nlSetVariable(1, vid, sa->list_verts[vid]->co[1]);
			nlSetVariable(2, vid, sa->list_verts[vid]->co[2]);
		}
		for (i=0; i<ns; i++) {
			nlLockVariable(sa->list_index[i]);
		}
		nlBegin(NL_MATRIX);

		init_laplacian_matrix(data);
		compute_implict_rotations(data);

		for (i=0; i<ns; i++) {
			vid = sa->list_index[i];
			nlRightHandSideAdd(0, n + i , sa->co[vid][0]);
			nlRightHandSideAdd(1, n + i , sa->co[vid][1]);
			nlRightHandSideAdd(2, n + i , sa->co[vid][2]);
			nlMatrixAdd(n + i, vid, 1.0f);
		}

		for (i=0; i<nh; i++)
		{
			vid = shs->list_handlers[i];
			nlRightHandSideAdd(0, n + ns + i 	, sa->list_verts[vid]->co[0]);
			nlRightHandSideAdd(1, n + ns + i	, sa->list_verts[vid]->co[1]);
			nlRightHandSideAdd(2, n + ns + i	, sa->list_verts[vid]->co[2]);
			nlMatrixAdd(n + ns + i 		, vid					, 1.0f);
		}
		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);

		if (nlSolveAdvanced(NULL, NL_TRUE) ) {
			
			nlBegin(NL_SYSTEM);
			nlBegin(NL_MATRIX);
			rotate_differential_coordinates(data);

			for (i=0; i<ns; i++) {
				vid = sa->list_index[i];
				nlRightHandSideAdd(0, n + i , sa->co[vid][0]);
				nlRightHandSideAdd(1, n + i , sa->co[vid][1]);
				nlRightHandSideAdd(2, n + i , sa->co[vid][2]);
			}

			for (i=0; i<nh; i++)
			{
				vid = shs->list_handlers[i];
				nlRightHandSideAdd(0, n + ns + i 	, sa->list_verts[vid]->co[0]);
				nlRightHandSideAdd(1, n + ns + i	, sa->list_verts[vid]->co[1]);
				nlRightHandSideAdd(2, n + ns + i	, sa->list_verts[vid]->co[2]);
			}
			nlEnd(NL_MATRIX);
			nlEnd(NL_SYSTEM);
			if (nlSolveAdvanced(NULL, NL_FALSE) ) {
				BM_ITER_MESH (v, &viter, em->bm, BM_VERTS_OF_MESH) {
					vid = BM_elem_index_get(v);
					v->co[0] = nlGetVariable(0, vid);
					v->co[1] = nlGetVariable(1, vid);
					v->co[2] = nlGetVariable(2, vid);
				}			
			}
			printf("\nSystem solved");
		}
		else{
			printf("\nNo solution found");
		}
		update_system_state(data, LAP_STATE_HAS_L_COMPUTE);
	} else if (data->stateSystem == LAP_STATE_HAS_L_COMPUTE ) {
		sys = data->sys;
		sa = data->sa;
		shs = data->shs;
		sys->bm = bm;
		n = sys->numVerts;
		ns = sa->numStatics;
		nh = shs->numHandlers;

		nlBegin(NL_SYSTEM);
		nlBegin(NL_MATRIX);

		for (i=0; i<n; i++) {
			nlRightHandSideAdd(0, i  , sys->delta[i][0]);
			nlRightHandSideAdd(1, i  , sys->delta[i][1]);
			nlRightHandSideAdd(2, i  , sys->delta[i][2]);
		}
		for (i=0; i<ns; i++) {
			vid = sa->list_index[i];
			nlRightHandSideAdd(0, n + i , sa->co[vid][0]);
			nlRightHandSideAdd(1, n + i , sa->co[vid][1]);
			nlRightHandSideAdd(2, n + i , sa->co[vid][2]);
			nlMatrixAdd(n + i, vid, 1.0f);
		}

		for (i=0; i<nh; i++)
		{
			vid = shs->list_handlers[i];
			nlRightHandSideAdd(0, n + ns + i 	, sa->list_verts[vid]->co[0]);
			nlRightHandSideAdd(1, n + ns + i	, sa->list_verts[vid]->co[1]);
			nlRightHandSideAdd(2, n + ns + i	, sa->list_verts[vid]->co[2]);
			nlMatrixAdd(n + ns + i 		, vid					, 1.0f);
		}
		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);

		if (nlSolveAdvanced(NULL, NL_FALSE) ) {
			
			nlBegin(NL_SYSTEM);
			nlBegin(NL_MATRIX);
			rotate_differential_coordinates(data);

			for (i=0; i<ns; i++) {
				vid = sa->list_index[i];
				nlRightHandSideAdd(0, n + i , sa->co[vid][0]);
				nlRightHandSideAdd(1, n + i , sa->co[vid][1]);
				nlRightHandSideAdd(2, n + i , sa->co[vid][2]);
			}

			for (i=0; i<nh; i++)
			{
				vid = shs->list_handlers[i];
				nlRightHandSideAdd(0, n + ns + i 	, sa->list_verts[vid]->co[0]);
				nlRightHandSideAdd(1, n + ns + i	, sa->list_verts[vid]->co[1]);
				nlRightHandSideAdd(2, n + ns + i	, sa->list_verts[vid]->co[2]);
			}
			nlEnd(NL_MATRIX);
			nlEnd(NL_SYSTEM);
			if (nlSolveAdvanced(NULL, NL_FALSE) ) {
				BM_ITER_MESH (v, &viter, em->bm, BM_VERTS_OF_MESH) {
					vid = BM_elem_index_get(v);
					v->co[0] = nlGetVariable(0, vid);
					v->co[1] = nlGetVariable(1, vid);
					v->co[2] = nlGetVariable(2, vid);
				}			
			}
			printf("\nSystem solved 2");
		}
		else{
			printf("\nNo solution found 2");
		}
		update_system_state(data, LAP_STATE_HAS_L_COMPUTE);

	}
}