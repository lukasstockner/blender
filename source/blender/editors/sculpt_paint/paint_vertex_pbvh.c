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
 * The Original Code is: all of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sculpt_paint/paint_vertex_pbvh.c
 *  \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_bitmap.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_action.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_report.h"
#include "BKE_colortools.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_buffers.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"

typedef struct {
	PaintSession *psession;
	const float *ray_start, *ray_normal;
	bool hit;
	float dist;
} VPaintRaycastData;


static void vpaint_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
	if (BKE_pbvh_node_get_tmin(node) < *tmin) {
		VPaintRaycastData *vrd = data_v;

		if (BKE_pbvh_node_raycast(vrd->psession->pbvh, node, NULL, false,
		                          vrd->ray_start, vrd->ray_normal, &vrd->dist))
		{
			vrd->hit = 1;
			*tmin = vrd->dist;
		}
	}
}

static bool vpaint_stroke_get_location(bContext *C, float out[3], const float mouse[2])
{
	Object *ob;
	PaintSession *psession;
	float ray_start[3], ray_end[3], ray_normal[3], dist;
	VPaintRaycastData vrd;
	ViewContext vc;

	view3d_set_viewcontext(C, &vc);

	ob = vc.obact;

	psession = ob->paint;

	//sculpt_stroke_modifiers_check(C, ob);

	dist = paint_pbvh_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, false);

	vrd.psession = psession;
	vrd.hit = 0;
	vrd.ray_start = ray_start;
	vrd.ray_normal = ray_normal;
	vrd.dist = dist;

	BKE_pbvh_raycast(psession->pbvh, vpaint_raycast_cb, &vrd,
	                 ray_start, ray_normal, false);

	copy_v3_v3(out, ray_normal);
	mul_v3_fl(out, vrd.dist);
	add_v3_v3(out, ray_start);

	return vrd.hit;
}

static bool vpaint_stroke_test_start(bContext *C, struct wmOperator *UNUSED(op),
                                    const float mouse[2])
{
	float co[3];
	/* Don't start the stroke until mouse goes over the mesh.
	 * note: mouse will only be null when re-executing the saved stroke. */
	if (!mouse || vpaint_stroke_get_location(C, co, mouse)) {
		Object *ob = CTX_data_active_object(C);

		ED_view3d_init_mats_rv3d(ob, CTX_wm_region_view3d(C));

//		sculpt_update_cache_invariants(C, sd, psession, op, mouse);

//		sculpt_undo_push_begin(sculpt_tool_name(sd));

		return 1;
	}
	else
		return 0;
}

static int vpaint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int retval;

//	op->customdata = paint_stroke_new(C, op, NULL, vpaint_stroke_test_start,
//	                                  vpaint_stroke_update_step, NULL,
//	                                  vpaint_stroke_done, event->type);
	(void)vpaint_stroke_test_start;
	
	if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
		paint_stroke_data_free(op);
		return OPERATOR_FINISHED;
	}

	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	OPERATOR_RETVAL_CHECK(retval);
	BLI_assert(retval == OPERATOR_RUNNING_MODAL);
	
	return OPERATOR_RUNNING_MODAL;
}

void PAINT_OT_vertex_paint_pbvh(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Paint";
	ot->idname = "PAINT_OT_vertex_paint_pbvh";
	ot->description = "Paint a stroke in the active vertex color layer";
	
	/* api callbacks */
	ot->invoke = vpaint_invoke;
	ot->modal = paint_stroke_modal;
//	ot->exec = vpaint_exec;
	ot->poll = vertex_paint_poll;
//	ot->cancel = vpaint_cancel;
	
	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

	paint_stroke_operator_properties(ot);
}


/* if the polygons from the mesh and the 'derivedFinal' match
 * we can assume that no modifiers are applied and that its worth adding tessellated faces
 * so 'vertex_paint_use_fast_update_check()' returns true */
static bool vertex_paint_use_tessface_check(Object *ob, Mesh *me)
{
	DerivedMesh *dm = ob->derivedFinal;

	if (me && dm) {
		return (me->mpoly == CustomData_get_layer(&dm->polyData, CD_MPOLY));
	}

	return false;
}

static void update_tessface_data(Object *ob, Mesh *me)
{
	if (vertex_paint_use_tessface_check(ob, me)) {
		/* assume if these exist, that they are up to date & valid */
		if (!me->mcol || !me->mface) {
			/* should always be true */
			/* XXX Why this clearing? tessface_calc will reset it anyway! */
#if 0
			if (me->mcol) {
				memset(me->mcol, 255, 4 * sizeof(MCol) * me->totface);
			}
#endif

			/* create tessfaces because they will be used for drawing & fast updates */
			BKE_mesh_tessface_calc(me); /* does own call to update pointers */
		}
	}
	else {
		if (me->totface) {
			/* this wont be used, theres no need to keep it */
			BKE_mesh_tessface_clear(me);
		}
	}

}

static bool make_vertexcol(Object *ob)  /* single ob */
{
	Mesh *me;

	if ((ob->id.lib) ||
	    ((me = BKE_mesh_from_object(ob)) == NULL) ||
	    (me->totpoly == 0) ||
	    (me->edit_btmesh))
	{
		return false;
	}

	/* copies from shadedisplist to mcol */
	if (!me->mloopcol && me->totloop) {
		if (!me->mcol) {
			CustomData_add_layer(&me->fdata, CD_MCOL, CD_DEFAULT, NULL, me->totface);
		}
		if (!me->mloopcol) {
			CustomData_add_layer(&me->ldata, CD_MLOOPCOL, CD_DEFAULT, NULL, me->totloop);
		}
		BKE_mesh_update_customdata_pointers(me, true);
	}

	update_tessface_data(ob, me);

	DAG_id_tag_update(&me->id, 0);

	return (me->mloopcol != NULL);
}

static void vpaint_init_session(Scene *scene, Object *ob)
{
	PaintSession *psession = ob->paint;
	/* needs to be called after we ensure tessface */
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);

	if (!psession) {
		psession = ob->paint = MEM_callocN(sizeof(PaintSession), "sculpt session");
	}

	psession->pbvh = dm->getPBVH(ob, dm);
	
//	BKE_sculpt_update_mesh_elements(scene, scene->toolsettings->sculpt, ob, 0, false);
}

static VPaint *new_vpaint(int wpaint)
{
	VPaint *vp = MEM_callocN(sizeof(VPaint), "VPaint");

	vp->flag = (wpaint) ? 0 : VP_SPRAY;
	vp->paint.flags |= PAINT_SHOW_BRUSH;

	return vp;
}

static int vpaint_mode_toggle_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	const int mode_flag = OB_MODE_VERTEX_PAINT;
	const bool is_mode_set = (ob->mode & mode_flag) != 0;
	Scene *scene = CTX_data_scene(C);
	VPaint *vp = scene->toolsettings->vpaint;
	Mesh *me;
	MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);
	int flush_recalc = 0;

	if (!is_mode_set) {
		if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	/* multires in sculpt mode could have different from object mode subdivision level */
	flush_recalc |= mmd && mmd->sculptlvl != mmd->lvl;

	me = BKE_mesh_from_object(ob);

	/* toggle: end vpaint */
	if (is_mode_set) {
		ob->mode &= ~mode_flag;

		if (mmd)
			multires_force_update(ob);

		if (flush_recalc)
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

		if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
			BKE_mesh_flush_select_from_polys(me);
		}

		BKE_free_paintsession(ob);

		paint_cursor_delete_textures();
	}
	else {
		ob->mode |= mode_flag;

		if (me->mloopcol == NULL) {
			make_vertexcol(ob);
		}

		if (vp == NULL) {
			vp = scene->toolsettings->vpaint = new_vpaint(0);

			/* Turn on X plane mirror symmetry by default */
			vp->paint.symmetry_flags |= PAINT_SYMM_X;
			vp->paint.flags |= PAINT_SHOW_BRUSH;
		}

		/* Create sculpt mode session data */
		if (ob->paint)
			BKE_free_paintsession(ob);

		vpaint_init_session(scene, ob);

		paint_cursor_start(C, vertex_paint_poll);

		BKE_paint_init(&scene->toolsettings->unified_paint_settings, &vp->paint, PAINT_CURSOR_VERTEX_PAINT);
	}

	/* update modifier stack for mapping requirements */
	DAG_id_tag_update(&me->id, 0);

	WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

	return OPERATOR_FINISHED;
}

/* for switching to/from mode */
static int paint_poll_test(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	if (ob == NULL || ob->type != OB_MESH)
		return 0;
	if (!ob->data || ((ID *)ob->data)->lib)
		return 0;
	if (CTX_data_edit_object(C))
		return 0;
	return 1;
}

void PAINT_OT_vertex_paint_pbvh_toggle(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Vertex Paint Mode";
	ot->idname = "PAINT_OT_vertex_paint_pbvh_toggle";
	ot->description = "Toggle the vertex paint mode in 3D view";

	/* api callbacks */
	ot->exec = vpaint_mode_toggle_exec;
	ot->poll = paint_poll_test;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
