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
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_paint.h"

#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "ED_sculpt.h"

#include "WM_api.h"
#include "WM_keymap.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "paint_intern.h"

#include <string.h>
#include <limits.h>

int paintcurve_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	Paint *p;
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	SpaceImage *sima;

	if (rv3d && !(ob && (ob->mode & OB_MODE_ALL_PAINT)))
		return false;

	sima = CTX_wm_space_image(C);

	if (sima && sima->mode != SI_MODE_PAINT)
		return false;

	p = BKE_paint_get_active_from_context(C);

	if (p && p->brush && (p->brush->flag & BRUSH_CURVE)) {
		return true;
	}

	return false;
}

/* Paint Curve Undo*/

typedef struct UndoCurve {
	struct UndoImageTile *next, *prev;

	PaintCurvePoint *points; /* points of curve */
	int tot_points;
	int active_point;

	char idname[MAX_ID_NAME];  /* name instead of pointer*/
} UndoCurve;

static void paintcurve_undo_restore(bContext *C, ListBase *lb)
{
	Paint *p = BKE_paint_get_active_from_context(C);
	UndoCurve *uc;
	PaintCurve *pc;

	if (p->brush) {
		pc = p->brush->paint_curve;
	}

	if (!pc)
		return;

	uc = (UndoCurve *)lb->first;

	if (strncmp(uc->idname, pc->id.name, BLI_strnlen(uc->idname, sizeof(uc->idname))) == 0) {
		SWAP(PaintCurvePoint *, pc->points, uc->points);
		SWAP(int, pc->tot_points, uc->tot_points);
		SWAP(int, pc->add_index, uc->active_point);
	}
}

static void paintcurve_undo_delete(ListBase *lb)
{
	UndoCurve *uc;
	uc = (UndoCurve *)lb->first;

	if (uc->points)
		MEM_freeN(uc->points);
	uc->points = NULL;
}


static void paintcurve_undo_begin(bContext *C, wmOperator *op, PaintCurve *pc)
{
	PaintMode mode = BKE_paintmode_get_active_from_context(C);
	ListBase *lb = NULL;
	int undo_stack_id;
	UndoCurve *uc;

	switch (mode) {
		case PAINT_TEXTURE_2D:
		case PAINT_TEXTURE_PROJECTIVE:
			undo_stack_id = UNDO_PAINT_IMAGE;
			break;

		case PAINT_SCULPT:
			undo_stack_id = UNDO_PAINT_MESH;
			break;

		default:
			/* do nothing, undo is handled by global */
			return;
	}


	ED_undo_paint_push_begin(undo_stack_id, op->type->name,
	                         paintcurve_undo_restore, paintcurve_undo_delete);
	lb = undo_paint_push_get_list(undo_stack_id);

	uc = MEM_callocN(sizeof(*uc), "Undo_curve");

	lb->first = uc;

	BLI_strncpy(uc->idname, pc->id.name, sizeof(uc->idname));
	uc->tot_points = pc->tot_points;
	uc->active_point = pc->add_index;
	uc->points = MEM_dupallocN(pc->points);

	undo_paint_push_count_alloc(undo_stack_id, sizeof(*uc) + sizeof(*pc->points) * pc->tot_points);

	ED_undo_paint_push_end(undo_stack_id);
}


/******************* Operators *********************************/

static int paintcurve_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	Paint *p = BKE_paint_get_active_from_context(C);
	Main *bmain = CTX_data_main(C);

	if (p && p->brush) {
		p->brush->paint_curve = BKE_paint_curve_add(bmain, "PaintCurve");
	}

	return OPERATOR_FINISHED;
}

void PAINTCURVE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Paint Curve";
	ot->description = "Add new paint curve";
	ot->idname = "PAINTCURVE_OT_new";

	/* api callbacks */
	ot->exec = paintcurve_new_exec;
	ot->poll = paintcurve_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void paintcurve_point_add(bContext *C,  wmOperator *op, const int loc[2])
{
	Paint *p = BKE_paint_get_active_from_context(C);
	Brush *br = p->brush;
	Main *bmain = CTX_data_main(C);
	PaintCurve *pc;
	PaintCurvePoint *pcp;
	wmWindow *window = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	float vec[3] = {loc[0], loc[1], 0.0};
	int add_index;
	int i;

	pc = br->paint_curve;
	if (!pc) {
		br->paint_curve = pc = BKE_paint_curve_add(bmain, "PaintCurve");
	}

	paintcurve_undo_begin(C, op, pc);

	pcp = MEM_mallocN((pc->tot_points + 1) * sizeof(PaintCurvePoint), "PaintCurvePoint");
	add_index = pc->add_index;

	if (pc->points) {
		if (add_index > 0)
			memcpy(pcp, pc->points, add_index * sizeof(PaintCurvePoint));
		if (add_index < pc->tot_points)
			memcpy(pcp + add_index + 1, pc->points + add_index, (pc->tot_points - add_index) * sizeof(PaintCurvePoint));

		MEM_freeN(pc->points);
	}
	pc->points = pcp;
	pc->tot_points++;

	/* initialize new point */
	memset(&pcp[add_index], 0, sizeof(PaintCurvePoint));
	copy_v3_v3(pcp[add_index].bez.vec[0], vec);
	copy_v3_v3(pcp[add_index].bez.vec[1], vec);
	copy_v3_v3(pcp[add_index].bez.vec[2], vec);

	/* last step, clear selection from all bezier handles expect the next */
	for (i = 0; i < pc->tot_points; i++) {
		pcp[i].bez.f1 = pcp[i].bez.f2 = pcp[i].bez.f3 = 0;
	}
	pcp[add_index].bez.f3 = SELECT;
	pcp[add_index].bez.h2 = HD_ALIGN;

	pc->add_index = add_index + 1;

	WM_paint_cursor_tag_redraw(window, ar);
}


static int paintcurve_add_point_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int loc[2] = {event->mval[0], event->mval[1]};
	paintcurve_point_add(C, op, loc);
	RNA_int_set_array(op->ptr, "location", loc);
	return OPERATOR_FINISHED;
}

static int paintcurve_add_point_exec(bContext *C, wmOperator *op)
{
	int loc[2];

	if (RNA_struct_property_is_set(op->ptr, "location")) {
		RNA_int_get_array(op->ptr, "location", loc);
		paintcurve_point_add(C, op, loc);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void PAINTCURVE_OT_add_point(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Paint Curve Point";
	ot->description = "Add new paint curve point";
	ot->idname = "PAINTCURVE_OT_add_point";

	/* api callbacks */
	ot->invoke = paintcurve_add_point_invoke;
	ot->exec = paintcurve_add_point_exec;
	ot->poll = paintcurve_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_int_vector(ot->srna, "location", 2, NULL, 0, SHRT_MAX,
						 "Location", "Location of vertex in area space", 0, SHRT_MAX);
}

static int paintcurve_delete_point_exec(bContext *C, wmOperator *op)
{
	Paint *p = BKE_paint_get_active_from_context(C);
	Brush *br = p->brush;
	PaintCurve *pc;
	PaintCurvePoint *pcp;
	wmWindow *window = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	int i;
	int tot_del = 0;
	pc = br->paint_curve;

	if (!pc || pc->tot_points == 0) {
		return OPERATOR_CANCELLED;
	}

	paintcurve_undo_begin(C, op, pc);

#define DELETE_TAG 2

	for (i = 0, pcp = pc->points; i < pc->tot_points; i++, pcp++) {
		if ((pcp->bez.f1 & SELECT) || (pcp->bez.f2 & SELECT) || (pcp->bez.f3 & SELECT)) {
			pcp->bez.f2 |= DELETE_TAG;
			tot_del++;
		}
	}

	if (tot_del > 0) {
		int j = 0;
		int new_tot = pc->tot_points - tot_del;
		PaintCurvePoint *points_new = NULL;
		if (new_tot > 0)
			points_new = MEM_mallocN(new_tot * sizeof(PaintCurvePoint), "PaintCurvePoint");

		for (i = 0, pcp = pc->points; i < pc->tot_points; i++, pcp++) {
			if (!(pcp->bez.f2 & DELETE_TAG)) {
				points_new[j] = pc->points[i];

				if ((i + 1) == pc->add_index) {
					pc->add_index = j + 1;
				}
				j++;
			}
			else if ((i + 1) == pc->add_index) {
				/* prefer previous point */
				pc->add_index = j;
			}
		}
		MEM_freeN(pc->points);

		pc->points = points_new;
		pc->tot_points = new_tot;
	}

#undef DELETE_TAG

	WM_paint_cursor_tag_redraw(window, ar);

	return OPERATOR_FINISHED;
}


void PAINTCURVE_OT_delete_point(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Paint Curve Point";
	ot->description = "Add new paint curve point";
	ot->idname = "PAINTCURVE_OT_delete_point";

	/* api callbacks */
	ot->exec = paintcurve_delete_point_exec;
	ot->poll = paintcurve_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;
}


static void paintcurve_point_select(bContext *C, wmOperator *op, const int loc[2],
                                    bool handle, bool toggle, bool extend)
{
	wmWindow *window = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	Paint *p = BKE_paint_get_active_from_context(C);
	Brush *br = p->brush;
	PaintCurve *pc;
	PaintCurvePoint *pcp;
	int i;
	int select = 0;

	pc = br->paint_curve;

	if (!pc)
		return;

	paintcurve_undo_begin(C, op, pc);

	pcp = pc->points;

	if (toggle) {
		bool selected = false;
		for (i = 0; i < pc->tot_points; i++) {
			if (pcp[i].bez.f1 || pcp[i].bez.f2 || pcp[i].bez.f3) {
				selected = true;
				break;
			}
		}

		if (!selected)
			select = SELECT;
	}

	if (!extend) {
		/* first clear selection from all bezier handles */
		for (i = 0; i < pc->tot_points; i++) {
			pcp[i].bez.f1 = pcp[i].bez.f2 = pcp[i].bez.f3 = select;
		}
	}

	if (!toggle) {
		for (i = 0; i < pc->tot_points; i++, pcp++) {
			/* shift means constrained editing so exclude center handles from collision detection */
			if (!handle) {
				if ((fabs(loc[0] - pcp->bez.vec[1][0]) < PAINT_CURVE_SELECT_THRESHOLD) &&
						(fabs(loc[1] - pcp->bez.vec[1][1]) < PAINT_CURVE_SELECT_THRESHOLD))
				{
					pcp->bez.f2 ^= SELECT;
					pc->add_index = i + 1;
					break;
				}
			}

			if ((fabs(loc[0] - pcp->bez.vec[0][0]) < PAINT_CURVE_SELECT_THRESHOLD) &&
					(fabs(loc[1] - pcp->bez.vec[0][1]) < PAINT_CURVE_SELECT_THRESHOLD))
			{
				pcp->bez.f1 ^= SELECT;
				pc->add_index = i + 1;
				if (handle)
					pcp->bez.h1 = HD_ALIGN;
				break;
			}

			if ((fabs(loc[0] - pcp->bez.vec[2][0]) < PAINT_CURVE_SELECT_THRESHOLD) &&
					(fabs(loc[1] - pcp->bez.vec[2][1]) < PAINT_CURVE_SELECT_THRESHOLD))
			{
				pcp->bez.f3 ^= SELECT;
				pc->add_index = i + 1;
				if (handle)
					pcp->bez.h2 = HD_ALIGN;
				break;
			}
		}
	}

	WM_paint_cursor_tag_redraw(window, ar);
}


static int paintcurve_select_point_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int loc[2] = {event->mval[0], event->mval[1]};
	bool handle = RNA_boolean_get(op->ptr, "handle");
	bool toggle = RNA_boolean_get(op->ptr, "toggle");
	bool extend = RNA_boolean_get(op->ptr, "extend");
	paintcurve_point_select(C, op, loc, handle, toggle, extend);
	RNA_int_set_array(op->ptr, "location", loc);
	return OPERATOR_FINISHED;
}

static int paintcurve_select_point_exec(bContext *C, wmOperator *op)
{
	int loc[2];

	if (RNA_struct_property_is_set(op->ptr, "location")) {
		bool handle = RNA_boolean_get(op->ptr, "handle");
		bool toggle = RNA_boolean_get(op->ptr, "toggle");
		bool extend = RNA_boolean_get(op->ptr, "extend");
		RNA_int_get_array(op->ptr, "location", loc);
		paintcurve_point_select(C, op, loc, handle, toggle, extend);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void PAINTCURVE_OT_select(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Paint Curve Point";
	ot->description = "Select a paint curve point";
	ot->idname = "PAINTCURVE_OT_select";

	/* api callbacks */
	ot->invoke = paintcurve_select_point_invoke;
	ot->exec = paintcurve_select_point_exec;
	ot->poll = paintcurve_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_int_vector(ot->srna, "location", 2, NULL, 0, SHRT_MAX,
						 "Location", "Location of vertex in area space", 0, SHRT_MAX);
	prop = RNA_def_boolean(ot->srna, "toggle", false, "Toggle", "Select/Deselect all");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "handle", false, "Handle", "Prefer handle selection");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}


static int paintcurve_draw_exec(bContext *C, wmOperator *UNUSED(op))
{
	PaintMode mode = BKE_paintmode_get_active_from_context(C);
	const char *name;

	switch (mode) {
		case PAINT_TEXTURE_2D:
		case PAINT_TEXTURE_PROJECTIVE:
			name = "PAINT_OT_image_paint";
			break;
		case PAINT_WEIGHT:
			name = "PAINT_OT_weight_paint";
			break;
		case PAINT_VERTEX:
			name = "PAINT_OT_vertex_paint";
			break;
		case PAINT_SCULPT:
			name = "SCULPT_OT_brush_stroke";
			break;
		default:
			return OPERATOR_PASS_THROUGH;
	}

	return 	WM_operator_name_call(C, name, WM_OP_INVOKE_DEFAULT, NULL);
}

void PAINTCURVE_OT_draw(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Draw Curve";
	ot->description = "Draw curve";
	ot->idname = "PAINTCURVE_OT_draw";

	/* api callbacks */
	ot->exec = paintcurve_draw_exec;
	ot->poll = paintcurve_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;
}
