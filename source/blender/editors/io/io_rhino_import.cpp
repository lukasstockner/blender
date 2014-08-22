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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include "opennurbs.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <time.h>
#include <limits>

extern "C" {
	#include "DNA_scene_types.h"
	#include "BLF_translation.h"
	#include "BLI_listbase.h"
	#include "BLI_math.h"
	#include "BKE_context.h"
	#include "BKE_global.h"
	#include "BKE_main.h"
	#include "BKE_report.h"
	#include "BKE_editmesh.h"
	#include "BKE_library.h"
	#include "BKE_curve.h"
	#include "bmesh.h"
	#include "ED_screen.h"
	#include "ED_object.h"
	#include "ED_util.h"
	#include "ED_curve.h"
	#include "RNA_access.h"
	#include "RNA_define.h"
	#include "DNA_object_types.h"
	#include "DNA_curve_types.h"
	#include "UI_interface.h"
	#include "UI_resources.h"
	#include "WM_api.h"
	#include "WM_types.h"
	#include "MEM_guardedalloc.h"
	// BLI's lzma definitions don't play ball with opennurbs's zlib definitions
	// #include "BLI_blenlib.h"
	// #include "BLI_utildefines.h"
	bool BLI_replace_extension(char *path, size_t maxlen, const char *ext);

	#include "io_rhino_import.h"
}

/* Converts an openNURBS widestring (UTF-16) under memory management by ON
 * into a char* to a malloc'd UTF-8 string. */
static char *import_ON_str(ON_wString& onstr) {
	const wchar_t *curve_name_unmanaged = onstr;
	if (!curve_name_unmanaged) return NULL;
	size_t sz = wcslen(curve_name_unmanaged)*sizeof(wchar_t);
	char *ret = (char*)malloc(sz);
	wcstombs(ret, curve_name_unmanaged, sz);
	return ret;
}
static void import_ON_str(char *dest, ON_wString& onstr, size_t n) {
	const wchar_t *curve_name_unmanaged = onstr;
	if (!curve_name_unmanaged) {
		*dest = '\0';
		return;
	}
	wcstombs(dest, curve_name_unmanaged, n);
}

// Note: ignores first and last knots for Rhino compatibility. Returns:
//             (uniform)    0 <---| can't tell these two apart by knots alone.
// #define CU_NURB_CYCLIC	1 <---| "periodic" is the hint that disambiguates them.
// #define CU_NURB_ENDPOINT	2
// #define CU_NURB_BEZIER	4
static int analyze_knots(float *knots, int num_knots, int order, bool periodic, float tol=.001) {
	printf("knots{"); for (int i=0; i<num_knots; i++) printf("%f,",knots[i]); printf("}->");
	float first = knots[1];
	float last = knots[num_knots-2];
	
	bool start_clamped = true;
	for (int i=2; i<order; i++) {
		if (abs(knots[i]-first)>tol) {start_clamped = false; break;}
	}
	bool end_clamped = true;
	for (int i=num_knots-3; i>=num_knots-order; i--) {
		if (abs(knots[i]-last)>tol) {end_clamped = false; break;}
	}
	bool bezier = start_clamped && end_clamped;
	bool unif_bezier = bezier;
	if (bezier) {
		float jump = knots[order]-knots[order-1];
		for (int i=order; i<num_knots-order; i+=order-1) {
			if (abs(knots[i]-knots[i+1])>tol) {
				bezier = false;
				unif_bezier = false;
				break;
			}
			if (abs(knots[i]-knots[i-1]-jump)>tol) {
				unif_bezier = false;
			}
		}
	}
	bool unif = !start_clamped && !end_clamped && !bezier;
	if (unif) {
		float jump = knots[1] - knots[0];
		for (int i=2; i<num_knots; i++) {
			if (abs(knots[i]-knots[i-1]-jump)>tol) {
				unif = false;
				break;
			}
		}
	}
	bool unif_clamped = !unif && !bezier && start_clamped && end_clamped;
	if (unif_clamped) {
		float jump = knots[order]-knots[order-1];
		for (int i=order; i<=num_knots-order; i++) {
			if (abs(knots[i]-knots[i-1]-jump)>tol) {
				unif_clamped = false;
				break;
			}
		}
	}
	
	if (bezier) {
		printf("bez\n");
		return CU_NURB_BEZIER;
	}
	if (unif) {
		printf("unif/cyc\n");
		return periodic? CU_NURB_CYCLIC : 0;
	}
	if (unif_clamped) {
		printf("endpt\n");
		return CU_NURB_ENDPOINT;
	}
	return CU_NURB_CUSTOMKNOT;
}

/* Determines smallest, second smallest knots and scales them to correspond to
 * 0 and 1 respectively.
 * uv=='u': operate on u knots
 * uv=='v': operate on v knots
 */
static void normalize_knots(Nurb *nu, char uv) {
	float tol = .001;
	float *knots = (uv=='u')? nu->knotsu : nu->knotsv;
	int num_knots = (uv=='u')? KNOTSU(nu) : KNOTSV(nu);
	int i=0;
	float lowest=std::numeric_limits<float>::infinity();
	float second_lowest=lowest; /* constraint: second_lowest > lowest (NOT >=). */
	for (int i=0; i<num_knots; i++) {
		if (knots[i] <= lowest) {
			lowest = knots[i];
			continue;
		}
		if (knots[i] <= lowest+tol) continue;
		/* have: knots[i] > lowest+tol */
		if (knots[i] <= second_lowest) {
			second_lowest = knots[i];
			continue;
		}
		/* have: knots[i]>lowest && knots[i]>second_lowest => nothing to do */
	}
	if (lowest==second_lowest) {
		fprintf(stderr, "Could not normalize knots: lowest = second lowest.\n");
		return;
	}
	if (!isfinite(lowest) || !isfinite(second_lowest)) {
		fprintf(stderr, "Could not normalize knots: too few?\n");
		return;
	}
	// (new knot) = ((old knot)-(smallest knot)) / ((sec smallest knot)-(smallest knot))
	double denominator = 1.0 / (second_lowest-lowest);
	for (; i<num_knots; i++) {
		knots[i] = (knots[i]-lowest)*denominator;
	}
	
	// Now we rescale the trim curves so that they hold position rel. to knots
	int uv_idx = (uv=='u')? 0 : 1;
	for (NurbTrim *nt = (NurbTrim*)nu->trims.first; nt; nt=nt->next) {
		for (Nurb *trim_nurb = (Nurb*)nt->nurb_list.first; trim_nurb; trim_nurb=trim_nurb->next) {
			float umin=std::numeric_limits<float>::infinity();
			float umax=-std::numeric_limits<float>::infinity();
			int ptsu = std::max(trim_nurb->pntsu,1);
			int ptsv = std::max(trim_nurb->pntsv,1);
			int bp_count = ptsu*ptsv;
			BPoint *bp = trim_nurb->bp; // Control points
			for (int bpnum=0; bpnum<bp_count; bpnum++) {
				double old = bp[bpnum].vec[uv_idx];
				double new_uv = (old-lowest)*denominator;
				bp[bpnum].vec[uv_idx] = new_uv;
				umin = std::min(umin,(float)new_uv);
				umax = std::max(umax,(float)new_uv);
			}
		}
	}
	BKE_nurbs_cached_UV_mesh_clear(nu,true);
}

/****************************** Curve Import *********************************/
static float null_loc[] = {0,0,0};
static float null_rot[] = {0,0,0};
static Nurb* rhino_import_curve(bContext *C,
							   ON_Curve *curve,
							   ON_Object *Object,
							   ON_3dmObjectAttributes *Attributes,
							   bool newobj=true,
							   bool cast_lines_to_nurbs=false,
							   bool dont_add_to_scene=false);
static void rhino_import_mesh(bContext *C,
							  ON_Mesh *curve,
							  ON_Object *Object,
							  ON_3dmObjectAttributes *Attributes,
							  bool newobj=true);
static void rhino_import_surface(bContext *C,
								 ON_Surface *surf,
								 ON_Object *obj,
								 ON_3dmObjectAttributes *attrs,
								 bool newobj=true);

// !!!!!!########$$$$$$$ todo $$$$$$#########!!!!!!!!!
// Wrap with curve object creation code
static void rhino_import_polycurve(bContext *C, ON_PolyCurve *pc, ON_Object *obj, ON_3dmObjectAttributes *attrs, bool newobj) {
	char curve_name[MAX_ID_NAME];
	import_ON_str(curve_name,attrs->m_name,MAX_ID_NAME);
	
	// Create NURBS object in editmode
	BLI_assert(false);
	
	const ON_SimpleArray<ON_Curve*> &curves = pc->SegmentCurves();
	int num_curves = curves.Count();
	for (int i=0; i<num_curves; i++) {
		ON_Curve *curve = *curves.At(i);
		rhino_import_curve(C, curve, obj, attrs, false, true, false);
	}
	
	// Leave NURBS object editmode
	printf("polycurve done\n");
}

static Nurb *nurb_from_ON_NurbsCurve(ON_NurbsCurve *nc) {
	Nurb *nu = (Nurb *)MEM_callocN(sizeof(Nurb), "rhino_imported_NURBS_curve");
	nu->flag = (nc->Dimension()==2)? CU_2D : CU_3D;
	nu->type = CU_NURBS;
	nu->resolu = 10;
	nu->resolv = 10;
	nu->pntsu = nc->CVCount();
	nu->pntsv = 1;
	nu->orderu = nc->Order();
	nu->orderv = 1;
	nu->editknot = NULL;
	BLI_assert(nu->pntsu + nu->orderu - 2 == nc->KnotCount());
	BPoint *bp = nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * ((nu->pntsu) * 1), "rhino_imported_NURBS_curve_points");
	nu->knotsu = (float *)MEM_callocN(sizeof(float) * ((nu->pntsu+nu->orderu) * 1), "rhino_imported_NURBS_curve_points");
	//int on_dim = nc->Dimension();
	//bool is_rational = nc->IsRational();
	for (int i=0; i<nu->pntsu; i++) {
		ON_4dPoint control_vert;
		nc->GetCV(i, control_vert);
		bp->vec[0] = control_vert.x/control_vert.w;
		bp->vec[1] = control_vert.y/control_vert.w;
		bp->vec[2] = control_vert.z/control_vert.w;
		bp->vec[3] = control_vert.w;
		bp++;
	}
	int i=1; for (int l=nu->pntsu+nu->orderu-1; i<l; i++) {
		nu->knotsu[i] = nc->Knot(i-1);
	}
	nu->knotsu[0] = nu->knotsu[1];
	nu->knotsu[i] = nu->knotsu[i-1];
	nu->flagu = analyze_knots(nu->knotsu, nu->pntsu+nu->orderu, nu->orderu, nc->IsPeriodic());
	double minu,maxu;
	nc->GetDomain(&minu, &maxu);
	normalize_knots(nu, 'u');
	return nu;
}

static Nurb *rhino_import_nurbscurve(bContext *C, ON_NurbsCurve *nc, ON_Object *obj, ON_3dmObjectAttributes *attrs, bool newobj) {
	char curve_name[MAX_ID_NAME];
	int layer;
	Object *obedit;
	Curve *cu = NULL;
	Nurb *nu = NULL;
	ListBase *editnurb;
	
	obedit = CTX_data_edit_object(C);
	layer = attrs->m_layer_index;
	if (newobj) {
		import_ON_str(curve_name,attrs->m_name,MAX_ID_NAME);
		if (layer==0) layer = 1;
		//Exit editmode if we're in it
		if (obedit) {
			ED_object_editmode_load(obedit);
			BLI_assert(!CTX_data_edit_object(C));
		}
		obedit = ED_object_add_type(C, OB_CURVE, null_loc, null_rot, true, layer);
		rename_id((ID *)obedit, curve_name);
		rename_id((ID *)obedit->data, curve_name);
		cu = (Curve*)obedit->data;
		cu->resolu = 15;
		cu->resolv = 1;
	} else {
		cu = (Curve*)obedit->data;
	}
	
	nu = nurb_from_ON_NurbsCurve(nc);
	
	editnurb = object_editcurve_get(obedit);
	BLI_addtail(editnurb, nu);
	ED_object_editmode_exit(C, EM_FREEDATA);
	return nu;
}

static void rhino_import_linecurve(bContext *C, ON_LineCurve *lc, ON_Object *obj, ON_3dmObjectAttributes *attrs, bool newobj) {
	char curve_name[MAX_ID_NAME];
	BMEditMesh *em;
	BMesh *bm;
	Object *obedit;
	ON_3dPoint *from, *to;
	float from_f[3], to_f[3];
	int layer;
	BMVert *v1, *v2;
	BMEdge *e1;
	
	obedit = CTX_data_edit_object(C);
	layer = attrs->m_layer_index;
	if (newobj) {
		import_ON_str(curve_name,attrs->m_name,MAX_ID_NAME);
		if (layer==0) layer = 1;
		//Exit editmode if we're in it
		if (obedit) {
			ED_object_editmode_load(obedit);
			BLI_assert(!CTX_data_edit_object(C));
		}
		obedit = ED_object_add_type(C, OB_MESH, null_loc, null_rot, false, layer);
		rename_id((ID*)obedit, curve_name);
		rename_id((ID*)obedit->data, curve_name);
		ED_object_editmode_enter(C, EM_DO_UNDO | EM_IGNORE_LAYER);
	}
	BLI_assert(obedit);
	em = BKE_editmesh_from_object(obedit);
	BLI_assert(em);
	bm = em->bm;
	
	from = &lc->m_line.from;
	from_f[0]=from->x; from_f[1]=from->y; from_f[2]=from->z;
	to = &lc->m_line.to;
	to_f[0]=to->x; to_f[1]=to->y; to_f[2]=to->z;
	v1 = BM_vert_create(bm, from_f, NULL, BM_CREATE_NOP);
	v2 = BM_vert_create(bm, to_f, NULL, BM_CREATE_NOP);
	e1 = BM_edge_create(bm, v1, v2, NULL, BM_CREATE_NOP);
	
	if (newobj) ED_object_editmode_exit(C, EM_FREEDATA);
	printf("linecurve to layer %i\n", layer);
}

static void rhino_import_polylinecurve(bContext *C, ON_PolylineCurve *plc, ON_Object *obj, ON_3dmObjectAttributes *attrs, bool newobj) {
	char curve_name[MAX_ID_NAME];
	BMEditMesh *em;
	BMesh *bm;
	Object *obedit;
	ON_3dPoint *from, *to;
	float from_f[3], to_f[3];
	int layer;
	BMVert *v1, *v2;
	BMEdge *e1;
	ON_Polyline *pline;
	
	obedit = CTX_data_edit_object(C);
	if (newobj) {
		import_ON_str(curve_name,attrs->m_name,MAX_ID_NAME);
		layer = attrs->m_layer_index;
		if (layer==0) layer = 1;
		//Exit editmode if we're in it
		if (obedit) {
			ED_object_editmode_load(obedit);
			BLI_assert(!CTX_data_edit_object(C));
		}
		obedit = ED_object_add_type(C, OB_MESH, null_loc, null_rot, false, layer);
		rename_id((ID*)obedit, curve_name);
		rename_id((ID*)obedit->data, curve_name);
		ED_object_editmode_enter(C, EM_DO_UNDO | EM_IGNORE_LAYER);
	}
	BLI_assert(obedit);
	em = BKE_editmesh_from_object(obedit);
	BLI_assert(em);
	bm = em->bm;
	
	pline = &plc->m_pline;
	int len = pline->Count();
	from = pline->At(0);
	from_f[0]=from->x; from_f[1]=from->y; from_f[2]=from->z;
	v1 = BM_vert_create(bm, from_f, NULL, BM_CREATE_NOP);
	for (int i=1; i<len; i++) {
		to = pline->At(i);
		to_f[0]=to->x; to_f[1]=to->y; to_f[2]=to->z;
		v2 = BM_vert_create(bm, to_f, NULL, BM_CREATE_NOP);
		e1 = BM_edge_create(bm, v1, v2, NULL, BM_CREATE_NOP);
		v1 = v2;
		for (int j=0; j<3; j++) from_f[j] = to_f[j];
	}
	
	if (newobj) ED_object_editmode_exit(C, EM_FREEDATA);

	printf("polylinecurve imported to layer %i\n", layer);
}

// Todo: make this efficient.
// We can do it in some cases using Blender's notion of linked objects.
// However, openNURBS supports "linked objects with a twist" (twist = deformed
// or restricted param domain, etc) that are going to have to be converted
// to full objects in Blender regardless.
static void rhino_import_curveproxy(bContext *C, ON_CurveProxy *cp, ON_Object *obj, ON_3dmObjectAttributes *attrs, bool newobj) {
	char curve_name[MAX_ID_NAME];
	printf("curveproxy: %s\n", curve_name);
	ON_Curve *pc = cp->DuplicateCurve(); // Applies domain restrictions, etc
	rhino_import_curve(C, pc, obj, attrs, newobj);
	delete pc;
}

// Returns a Nurb* object iff one was created
static Nurb* rhino_import_curve(bContext *C,
								ON_Curve *curve,
								ON_Object *Object,
								ON_3dmObjectAttributes *Attributes,
								bool newobj,
								bool cast_lines_to_nurbs,
								bool dont_add_to_scene) {
	Nurb *ret = NULL;
	ON_PolyCurve *pc;
	ON_LineCurve *lc;
	ON_PolylineCurve *plc;
	ON_CurveProxy *cp;
	ON_NurbsCurve *nc = NULL;
	ON_ArcCurve *ac;
	ON_CurveOnSurface *cos;
	bool nc_needs_destroy = false;
	
	ac = ON_ArcCurve::Cast(curve);
	if (ac) {
		nc = ON_NurbsCurve::New();
		ac->GetNurbForm(*nc);
		nc_needs_destroy = true;
		ac = NULL;
	}
	cos = ON_CurveOnSurface::Cast(curve);
	if (cos) {
		nc = ON_NurbsCurve::New();
		cos->GetNurbForm(*nc);
		nc_needs_destroy = true;
		cos = NULL;
	}
	pc = ON_PolyCurve::Cast(curve);
	if (pc && cast_lines_to_nurbs) {
		nc = ON_NurbsCurve::New();
		pc->GetNurbForm(*nc);
		nc_needs_destroy = true;
		pc = NULL;
	}
	lc = ON_LineCurve::Cast(curve);
	if (lc && cast_lines_to_nurbs) {
		nc = ON_NurbsCurve::New();
		lc->GetNurbForm(*nc);
		nc_needs_destroy = true;
		lc = NULL;
	}
	plc = ON_PolylineCurve::Cast(curve);
	if (plc && cast_lines_to_nurbs) {
		nc = ON_NurbsCurve::New();
		plc->GetNurbForm(*nc);
		nc_needs_destroy = true;
		plc = NULL;
	}
	cp = ON_CurveProxy::Cast(curve);
	if (cp && cast_lines_to_nurbs) {
		nc = ON_NurbsCurve::New();
		cp->GetNurbForm(*nc);
		nc_needs_destroy = true;
		cp = NULL;
	}
	if (!nc) nc = ON_NurbsCurve::Cast(curve);
	if (nc && !pc && !lc && !plc && !cp) {
		if (dont_add_to_scene) {
			ret = nurb_from_ON_NurbsCurve(nc);
		} else {
			rhino_import_nurbscurve(C, nc, Object, Attributes, newobj);
		}
		if (nc_needs_destroy) nc->Destroy();
	} else {
		if (pc) rhino_import_polycurve(C, pc, Object, Attributes, newobj);
		if (lc) rhino_import_linecurve(C, lc, Object, Attributes, newobj);
		if (plc) rhino_import_polylinecurve(C, plc, Object, Attributes, newobj);
		if (cp) rhino_import_curveproxy(C, cp, Object, Attributes, newobj);
	}
	return ret;
}

static void rhino_import_mesh(bContext *C,
							  ON_Mesh *mesh,
							  ON_Object *obj,
							  ON_3dmObjectAttributes *attrs,
							  bool newobj) {
	char mesh_name[MAX_ID_NAME];
	BMEditMesh *em;
	BMesh *bm;
	int layer;
	
	Object *obedit = CTX_data_edit_object(C);
	if (newobj) {
		import_ON_str(mesh_name,attrs->m_name,MAX_ID_NAME);
		layer = attrs->m_layer_index;
		if (layer==0) layer = 1;
		//Exit editmode if we're in it
		if (obedit) {
			ED_object_editmode_load(obedit);
			BLI_assert(!CTX_data_edit_object(C));
		}
		obedit = ED_object_add_type(C, OB_MESH, null_loc, null_rot, false, layer);
		rename_id((ID*)obedit, mesh_name);
		rename_id((ID*)obedit->data, mesh_name);
		ED_object_editmode_enter(C, EM_DO_UNDO | EM_IGNORE_LAYER);
	}
	BLI_assert(obedit);
	em = BKE_editmesh_from_object(obedit);
	BLI_assert(em);
	bm = em->bm;
	
	ON_SimpleArray<ON_3fPoint>& ON_v = mesh->m_V;
	ON_SimpleArray<ON_MeshFace>& ON_f = mesh->m_F;
	//ON_SimpleArray<ON_3fVector>& ON_vnormals = mesh->m_N;
	//ON_SimpleArray<ON_3fVector>& ON_fnormals = mesh->m_FN;
	ON_SimpleArray<ON_2dex> ON_e; mesh->GetMeshEdges(ON_e);
	
	int num_v = ON_v.Count();
	std::vector<BMVert*> blend_v(num_v);
	for (int i=0; i<num_v; i++) {
		ON_3fPoint *pt = ON_v.At(i);
		float xyz[3] = {pt->x, pt->y, pt->z};
		BMVert *bv = BM_vert_create(bm, xyz, NULL, BM_CREATE_NOP);
		blend_v[i] = bv;
	}
	
	int num_e = ON_e.Count();
	std::vector<BMEdge*> blend_e(num_e);
	for (int i=0; i<num_e; i++) {
		ON_2dex *e = ON_e.At(i);
		BMEdge *be = BM_edge_create(bm, blend_v[e->i], blend_v[e->j], NULL, BM_CREATE_NOP);
		blend_e[i] = be;
	}
	
	int num_f = ON_f.Count();
	std::vector<BMFace*> blend_f(num_f);
	for (int i=0; i<num_f; i++) {
		ON_MeshFace *f = ON_f.At(i);
		BMVert *v0 = blend_v[f->vi[0]];
		BMVert *v1 = blend_v[f->vi[1]];
		BMVert *v2 = blend_v[f->vi[2]];
		BMVert *v3 = blend_v[f->vi[3]];
		BMFace *bf = BM_face_create_quad_tri(bm, v0, v1, v2, (v2==v3)?NULL:v3, NULL, BM_CREATE_NOP);
		blend_f[i] = bf;
	}
	
	if (newobj) ED_object_editmode_exit(C, EM_FREEDATA);
	
	printf("mesh imported to layer %i\n", layer);
}

/****************************** Surfaces *******************************/
static Curve* rhino_import_nurbs_surf_start(bContext *C,
										   ON_Object *obj,
										   ON_3dmObjectAttributes *attrs) {
	char curve_name[MAX_ID_NAME];
	Object *obedit = CTX_data_edit_object(C);
	int layer = attrs->m_layer_index;
	if (layer==0) layer = 1;
	import_ON_str(curve_name,attrs->m_name,MAX_ID_NAME);
	if (obedit) {
		ED_object_editmode_load(obedit);
		BLI_assert(!CTX_data_edit_object(C));
	}
	obedit = ED_object_add_type(C, OB_SURF, null_loc, null_rot, true, layer);
	rename_id((ID *)obedit, curve_name);
	rename_id((ID *)obedit->data, curve_name);
	Curve *cu = (Curve*)obedit->data;
	cu->resolu = cu->resolv = 10;
	cu->resolu_ren = cu->resolv_ren = 15;
	return cu;
}

static void rhino_import_nurbs_surf_end(bContext *C) {
	Curve *cu = (Curve*)CTX_data_edit_object(C)->data;
	float cent[3];
//	BKE_curve_center_median(cu, cent);
//	copy_v3_v3(cu->loc, cent);
//	mul_v3_fl(cent, -1);
//	BKE_curve_translate(cu, cent, false);
	ED_object_editmode_exit(C, EM_FREEDATA);
	printf("nurbssurf done\n");
}

static void nurb_normalize_knots(Nurb *nu) {
	printf("\tnormalizing knots\n");
	normalize_knots(nu, 'u');
	normalize_knots(nu, 'v');
	BKE_nurb_knot_calc_u(nu);
	BKE_nurb_knot_calc_v(nu);
}

static Nurb* rhino_import_nurbs_surf(bContext *C,
									 ON_Surface *raw_surf,
									 ON_Object *obj,
									 ON_3dmObjectAttributes *attrs,
									 bool newobj,
									 bool normalize_knots=true) {
	ON_NurbsSurface *surf = ON_NurbsSurface::Cast(raw_surf);
	bool surf_needs_delete = false;
	if (!surf) {
		surf = ON_NurbsSurface::New();
		surf_needs_delete = true;
		int success = raw_surf->GetNurbForm(*surf);
		if (!success) {
			delete surf;
			return NULL;
		}
	}
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = (Curve*)obedit->data;
	Nurb *nu = (Nurb *)MEM_callocN(sizeof(Nurb), "rhino_imported_NURBS_surf");
	nu->flag = CU_3D;
	nu->type = CU_NURBS;
	nu->resolu = cu->resolu;
	nu->resolv = cu->resolv;
	nu->pntsu = surf->CVCount(0);
	nu->pntsv = surf->CVCount(1);
	nu->orderu = surf->Order(0);
	nu->orderv = surf->Order(1);
	nu->editknot = NULL;
	if (surf->IsPeriodic(0))
		nu->flagu |= CU_NURB_CYCLIC;
	if (surf->IsPeriodic(1))
		nu->flagv |= CU_NURB_CYCLIC;
	nu->flagu |= CU_NURB_ENDPOINT;
	nu->flagv |= CU_NURB_ENDPOINT;
	BPoint *bp = nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * (nu->pntsu * nu->pntsv), "rhino_imported_NURBS_surf_points");
	nu->knotsu = (float *)MEM_callocN(sizeof(float) * ((nu->pntsu+nu->orderu) * 1), "rhino_imported_NURBS_surf_points");
	nu->knotsv = (float *)MEM_callocN(sizeof(float) * ((nu->pntsv+nu->orderv) * 1), "rhino_imported_NURBS_surf_points");
	//int on_dim = surf->Dimension();
	for (int j=0; j<nu->pntsv; j++) {
		for (int i=0; i<nu->pntsu; i++) {
			ON_4dPoint control_vert;
			surf->GetCV(i, j, control_vert);
			bp->vec[0] = control_vert.x/control_vert.w;
			bp->vec[1] = control_vert.y/control_vert.w;
			bp->vec[2] = control_vert.z/control_vert.w;
			bp->vec[3] = control_vert.w;
			bp++;
		}
	}
	// Eval code has hardcoded knot range, so we will ignore these for now
	int i=1; for (int l=nu->pntsu+nu->orderu-1; i<l; i++) {
		nu->knotsu[i] = surf->Knot(0,i-1);
	}
	nu->knotsu[0] =nu->knotsu[1];
	nu->knotsu[i] = nu->knotsu[i-1];
	i=1; for (int l=nu->pntsv+nu->orderv-1; i<l; i++) {
		nu->knotsv[i] = surf->Knot(1,i-1);
	}
	nu->knotsv[0] = nu->knotsv[1];
	nu->knotsv[i] = nu->knotsv[i-1];
	nu->flagu = analyze_knots(nu->knotsu, nu->pntsu+nu->orderu, nu->orderu, surf->IsPeriodic(0));
	nu->flagv = analyze_knots(nu->knotsv, nu->pntsv+nu->orderv, nu->orderv, surf->IsPeriodic(1));
	if (normalize_knots) nurb_normalize_knots(nu);
	
	ListBase *editnurb = object_editcurve_get(obedit);
	BLI_addtail(editnurb, nu);
	if (surf_needs_delete) delete surf;
	return nu;
}


static void rhino_import_surface(bContext *C,
								 ON_Surface *surf,
								 ON_Object *obj,
								 ON_3dmObjectAttributes *attrs,
								 bool newobj) {
	//ON_Extrusion *ext = ON_Extrusion::Cast(surf);
	ON_NurbsSurface *ns = ON_NurbsSurface::Cast(surf);
	//ON_PlaneSurface *ps = ON_PlaneSurface::Cast(surf);
	//ON_RevSurface *rs = ON_RevSurface::Cast(surf);
	//ON_SumSurface *ss = ON_SumSurface::Cast(surf);
	//ON_SurfaceProxy *sp = ON_SurfaceProxy::Cast(surf);
	bool did_handle = false;
	if (ns) {
		rhino_import_nurbs_surf_start(C, obj, attrs);
		rhino_import_nurbs_surf(C, ns, obj, attrs, newobj, false);
		rhino_import_nurbs_surf_end(C);
		did_handle = true;
	}
	if (!did_handle && surf->HasNurbForm()) {
		rhino_import_nurbs_surf_start(C, obj, attrs);
		rhino_import_nurbs_surf(C, surf, obj, attrs, newobj, false);
		rhino_import_nurbs_surf_end(C);
		did_handle = true;
	}
	if (!did_handle) {
		char surf_name[MAX_ID_NAME];
		import_ON_str(surf_name,attrs->m_name,MAX_ID_NAME);
		printf("couldn't handle %s\n",surf_name);
	}
}

static void rhino_import_brep_face(bContext *C,
								   ON_BrepFace *face,
								   ON_Object *parentObj,
								   ON_3dmObjectAttributes *parentAttrs) {
	/* Create the Surface */
	ON_Surface *face_surf = const_cast<ON_Surface*>(face->ProxySurface());
	ON_NurbsSurface *ns = ON_NurbsSurface::Cast(face_surf);
	bool should_destroy_ns = false;
	if (!ns) {
		ns = ON_NurbsSurface::New();
		int success = face_surf->GetNurbForm(*ns);
		if (!success) {
			delete ns;
			return;
		}
		should_destroy_ns = true;
	}
	Nurb *nu = rhino_import_nurbs_surf(C, ns, parentObj, parentAttrs, false, false);

	/* Add the trim curves */
	ON_BrepLoop *outer_loop = face->OuterLoop();
	int loop_count = face->LoopCount();
	if (loop_count>0) nu->flag |= CU_TRIMMED;
	printf("   outer_loop: 0x%lx\n",long(outer_loop));
	for (int loopnum=0; loopnum<loop_count; loopnum++) {
		ON_BrepLoop *loop = face->Loop(loopnum);
		if (loop==outer_loop && loop_count!=1) continue;
		int trim_count = loop->TrimCount();
		printf("   loop: 0x%lx\n",long(loop));
		NurbTrim *trim = (NurbTrim*)MEM_callocN(sizeof(NurbTrim),"NURBS_imported_trim");
		trim->type = (loop==outer_loop)? CU_TRIM_AND : CU_TRIM_SUB;
		trim->parent_nurb = nu;
		ListBase *nurb_list = &trim->nurb_list;
		for (int trimnum=0; trimnum<trim_count; trimnum++) {
			ON_BrepTrim *trim = loop->Trim(trimnum);
			ON_Curve *cu = const_cast<ON_Curve*>(trim->ProxyCurve());
			printf("      trim: 0x%lx %s\n",long(trim),cu->ClassId()->ClassName());
			Nurb *trim_nurb = rhino_import_curve(C, cu, parentObj, parentAttrs, false, true, true);
			BLI_addtail(nurb_list, trim_nurb);
		}
		BLI_addtail(&nu->trims, trim);
	}
	
	//nurb_normalize_knots(nu);

	if (should_destroy_ns) delete ns;
}

static void rhino_import_brep(bContext *C,
							  ON_Brep *brep,
							  ON_Object *obj,
							  ON_3dmObjectAttributes *attrs) {
	rhino_import_nurbs_surf_start(C, obj, attrs);
	ON_ObjectArray<ON_BrepFace>& brep_f = brep->m_F;
	int num_faces = brep_f.Count();
	bool havent_created_surf = true;
	for (int facenum=0; facenum<num_faces; facenum++) {
		ON_BrepFace *face = &brep_f[facenum];
		rhino_import_brep_face(C, face, obj, attrs);
		if (havent_created_surf) havent_created_surf = false;
	}
	rhino_import_nurbs_surf_end(C);
}



/****************************** Import 3dm File *******************************/
int rhino_import(bContext *C, wmOperator *op) {
	char filename[FILE_MAX];
	int i,n,MatCount;
	double Domain[2];

	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}
	RNA_string_get(op->ptr, "filepath", filename);

	FILE *f = ON::OpenFile(filename, "rb");
	ON_BinaryFile file(ON::read3dm, f);

	ON_Object			*Object= NULL;
	ON_Geometry			*Geometry;
	ON_3dmObjectAttributes	Attributes;
	ON_String			Comments;
	ON_3dmProperties		Properties;
	ON_3dmSettings		Settings;
	//Material			*Materials= NULL;

	// Read Start Section
	if (!file.Read3dmStartSection(&i,Comments))
		return(0);
	//  printf("Version: %d, Comments: %s\n",i,Comments);
	
	// Read Properties Section
	if (!file.Read3dmProperties(Properties))
		return(0);
	
	// Read Settings Section
	if (!file.Read3dmSettings(Settings))
		return(0);
	
	if (file.BeginRead3dmBitmapTable()) {
		if (!file.EndRead3dmBitmapTable())
			return(0);
	} //fi
	
	//// Read Material Table
	//if (file.BeginRead3dmMaterialTable()) {
	//	ON_Material		*Mat;
	//	char		*Text;
	
	//	//Materials= (Material*)malloc(sizeof(Material));
	//	MatCount= 0;
	
	//	while (file.Read3dmMaterial(&Mat)) {
	//		Text= (char*)malloc(sizeof(char)*Mat -> MaterialName().Length());
	//		Materials= (Material*)realloc(Materials,sizeof(Material)*(MatCount+1));
	
	//		Materials[MatCount].Index= Mat -> MaterialIndex();
	//		for (i= 0; i < Mat -> MaterialName().Length(); i++)
	//			Text[i]= Mat -> MaterialName()[i];
	//		Text[i]= 0;
	//		if (!Text[0]) {
	//			Text= (char*)malloc(sizeof(char)*32);
	//			sprintf(Text,"Material.%d\n",MatCount+1);
	//		} //fi
	//		Materials[MatCount].MatID= MatList -> Create(Text);
	
	//		MatList -> Diff(Materials[MatCount].MatID)[0]= (unsigned char)Mat -> Diffuse().FractionRed()*255;
	//		MatList -> Diff(Materials[MatCount].MatID)[1]= (unsigned char)Mat -> Diffuse().FractionGreen()*255;
	//		MatList -> Diff(Materials[MatCount].MatID)[2]= (unsigned char)Mat -> Diffuse().FractionBlue()*255;
	
	//		MatList -> Spec(Materials[MatCount].MatID)[0]= (unsigned char)Mat -> Specular().FractionRed()*255;
	//		MatList -> Spec(Materials[MatCount].MatID)[1]= (unsigned char)Mat -> Specular().FractionGreen()*255;
	//		MatList -> Spec(Materials[MatCount].MatID)[2]= (unsigned char)Mat -> Specular().FractionBlue()*255;
	
	//		free(Text);
	//		MatCount++;
	//	} //eof
	
	//	if (!file.EndRead3dmMaterialTable())
	//		return(0);
	//} //fi
	
	// Read Layer Table
	if (file.BeginRead3dmLayerTable()) {
		if (!file.EndRead3dmLayerTable())
			return(0);
	} //fi
	
	// Read Group Table
	if (file.BeginRead3dmGroupTable()) {
		if (!file.EndRead3dmGroupTable())
			return(0);
	} //fi
	
	// Read Light Table
	if (file.BeginRead3dmLightTable()) {
		if (!file.EndRead3dmLightTable())
			return(0);
	} //fi
	
	// Read Object Info
	if (file.BeginRead3dmObjectTable()) {
		
		//    while(file.Read3dmObject(&Object,&Attributes,0)) {
		while (1) {
			i= file.Read3dmObject(&Object,&Attributes,0);
			if (!i) break;
			char obj_name[MAX_ID_NAME];
			import_ON_str(obj_name, Attributes.m_name, MAX_ID_NAME);
			bool did_decode = false;
			Geometry= ON_Geometry::Cast(Object);
			
			if (ON_Curve::Cast(Geometry)) {
				printf("--- Curve->%s \"%s\" ---\n",Object->ClassId()->ClassName(),obj_name);
				rhino_import_curve(C, ON_Curve::Cast(Geometry), Object, &Attributes, true);
				did_decode = true;
			}
			
			if (ON_Surface::Cast(Geometry)) {
				printf("--- Surface->%s \"%s\" ---\n",Object->ClassId()->ClassName(),obj_name);
				did_decode = true;
				rhino_import_surface(C, ON_Surface::Cast(Geometry), Object, &Attributes);
			}
			
			if (ON_Mesh::Cast(Geometry)) {
				printf("--- Mesh->%s \"%s\" ---\n",Object->ClassId()->ClassName(),obj_name);
				did_decode = true;
				rhino_import_mesh(C, ON_Mesh::Cast(Geometry), Object, &Attributes);
			}
			
			if (ON_Brep::Cast(Geometry)) {
				printf("--- BREP->%s \"%s\" ---\n",Object->ClassId()->ClassName(),obj_name);
				ON_Brep *brep = ON_Brep::Cast(Geometry);
				rhino_import_brep(C, brep, Object, &Attributes);
				did_decode = true;
			}
			
			if (!did_decode) {
				printf("--- ?->%s \"%s\" ---\n",Object->ClassId()->ClassName(),obj_name);
			}
		}
		
		file.EndRead3dmObjectTable();
	}
	
	
	ON::CloseFile(f);
	ED_undo_push(C, "Imported 3dm file");
	return OPERATOR_FINISHED; //OPERATOR_CANCELLED
}

void WM_OT_rhino_import(struct wmOperatorType *ot) {
	ot->name = "Import Rhino 3DM";
	ot->description = "Load a Rhino-compatible .3dm file";
	ot->idname = "WM_OT_rhino_import";
	
	ot->invoke = WM_operator_filesel;
	ot->exec = rhino_import;
	ot->poll = WM_operator_winactive;
	
	RNA_def_string(ot->srna, "filter_glob", "*.3dm", 16,
	                          "Glob Filter", "Rhino Extension Glob Filter");
	RNA_def_string(ot->srna, "filename_ext", ".3dm", 16,
	                          "Rhino File Extension", "Rhino File Extension");
	
	WM_operator_properties_filesel(ot, FOLDERFILE , FILE_BLENDER, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
}
