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
 * The Original Code is Copyright (C) 2014, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_select.c
 *  \ingroup edgpencil
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"
#include "ED_keyframing.h"

#include "gpencil_intern.h"

/* ********************************************** */
/* Polling callbacks */

static int gpencil_select_poll(bContext *C)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);

	/* only if there's an active layer with an active frame */
	return (gpl && gpl->actframe);
}

/* ********************************************** */
/* Select All Operator */

static int gpencil_select_all_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	int action = RNA_enum_get(op->ptr, "action");
	
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	
	/* for "toggle", test for existing selected strokes */
	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		
		CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
		{
			if (gps->flag & GP_STROKE_SELECT) {
				action = SEL_DESELECT;
				break; // XXX: this only gets out of the inner loop...
			}
		}
		CTX_DATA_END;
	}
	
	/* if deselecting, we need to deselect strokes across all frames
	 *  - Currently, an exception is only given for deselection
	 *    Selecting and toggling should only affect what's visible,
	 *    while deselecting helps clean up unintended/forgotten
	 *    stuff on other frames
	 */
	if (action == SEL_DESELECT) {
		/* deselect strokes across editable layers
		 * NOTE: we limit ourselves to editable layers, since once a layer is "locked/hidden
		 *       nothing should be able to touch it
		 */
		CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
		{
			bGPDframe *gpf;
			
			/* deselect all strokes on all frames */
			for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
				bGPDstroke *gps;
				
				for (gps = gpf->strokes.first; gps; gps = gps->next) {
					bGPDspoint *pt;
					int i;
					
					for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
						pt->flag &= ~GP_SPOINT_SELECT;
					}
					
					gps->flag &= ~GP_STROKE_SELECT;
				}
			}
		}
		CTX_DATA_END;
	}
	else {
		/* select or deselect all strokes */
		CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
		{
			bGPDspoint *pt;
			int i;
			bool selected = false;
			
			/* Change selection status of all points, then make the stroke match */
			for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
				switch (action) {
					case SEL_SELECT:
						pt->flag |= GP_SPOINT_SELECT;
						break;
					//case SEL_DESELECT:
					//	pt->flag &= ~GP_SPOINT_SELECT;
					//	break;
					case SEL_INVERT:
						pt->flag ^= GP_SPOINT_SELECT;
						break;
				}
				
				if (pt->flag & GP_SPOINT_SELECT)
					selected = true;
			}
			
			/* Change status of stroke */
			if (selected)
				gps->flag |= GP_STROKE_SELECT;
			else
				gps->flag &= ~GP_STROKE_SELECT;
		}
		CTX_DATA_END;
	}
	
	/* updates */
	WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All Strokes";
	ot->idname = "GPENCIL_OT_select_all";
	ot->description = "Change selection of all Grease Pencil strokes currently visible";
	
	/* callbacks */
	ot->exec = gpencil_select_all_exec;
	ot->poll = gpencil_select_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	WM_operator_properties_select_all(ot);
}

/* ********************************************** */
/* Circle Select Operator */

/* Helper to check if a given stroke is within the area */
/* NOTE: Code here is adapted (i.e. copied directly) from gpencil_paint.c::gp_stroke_eraser_dostroke()
 *       It would be great to de-duplicate the logic here sometime, but that can wait...
 */
static bool gp_stroke_do_circle_sel(bGPDstroke *gps, ARegion *ar, View2D *v2d, rctf *subrect,
                                    const int mx, const int my, const int radius, 
                                    const bool select, rcti *rect)
{
	bGPDspoint *pt1, *pt2;
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	int i;
	bool changed = false;
	
	if (gps->totpoints == 1) {
		gp_point_to_xy(ar, v2d, subrect, gps, gps->points, &x0, &y0);
		
		/* do boundbox check first */
		if ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(rect, x0, y0)) {
			/* only check if point is inside */
			if (((x0 - mx) * (x0 - mx) + (y0 - my) * (y0 - my)) <= radius * radius) {
				/* change selection */
				if (select) {
					gps->points->flag |= GP_SPOINT_SELECT;
					gps->flag |= GP_STROKE_SELECT;
				}
				else {
					gps->points->flag &= ~GP_SPOINT_SELECT;
					gps->flag &= ~GP_STROKE_SELECT;
				}
				
				return true;
			}
		}
	}
	else {
		/* Loop over the points in the stroke, checking for intersections 
		 *  - an intersection means that we touched the stroke
		 */
		for (i = 0; (i + 1) < gps->totpoints; i++) {
			/* get points to work with */
			pt1 = gps->points + i;
			pt2 = gps->points + i + 1;
			
			gp_point_to_xy(ar, v2d, subrect, gps, pt1, &x0, &y0);
			gp_point_to_xy(ar, v2d, subrect, gps, pt2, &x1, &y1);
			
			/* check that point segment of the boundbox of the selection stroke */
			if (((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(rect, x0, y0)) ||
			    ((!ELEM(V2D_IS_CLIPPED, x1, y1)) && BLI_rcti_isect_pt(rect, x1, y1)))
			{
				int mval[2]  = {mx, my};
				int mvalo[2] = {mx, my}; /* dummy - this isn't used... */
				
				/* check if point segment of stroke had anything to do with
				 * eraser region  (either within stroke painted, or on its lines)
				 *  - this assumes that linewidth is irrelevant
				 */
				if (gp_stroke_inside_circle(mval, mvalo, radius, x0, y0, x1, y1)) {
					/* change selection of stroke, and then of both points 
					 * (as the last point otherwise wouldn't get selected
					 *  as we only do n-1 loops through) 
					 */
					if (select) {
						pt1->flag |= GP_SPOINT_SELECT;
						pt2->flag |= GP_SPOINT_SELECT;
						
						changed = true;
					}
					else {
						pt1->flag &= ~GP_SPOINT_SELECT;
						pt2->flag &= ~GP_SPOINT_SELECT;
						
						changed = true;
					}
				}
			}
		}
		
		/* Ensure that stroke selection is in sync with its points */
		gpencil_stroke_sync_selection(gps);
	}
	
	return changed;
}


static int gpencil_circle_select_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	
	const int mx = RNA_int_get(op->ptr, "x");
	const int my = RNA_int_get(op->ptr, "y");
	const int radius = RNA_int_get(op->ptr, "radius");
	
	const int gesture_mode = RNA_int_get(op->ptr, "gesture_mode");
	const bool select = (gesture_mode == GESTURE_MODAL_SELECT);
	
	rctf *subrect = NULL;       /* for using the camera rect within the 3d view */
	rctf subrect_data = {0.0f};
	rcti rect = {0};            /* for bounding rect around circle (for quicky intersection testing) */
	
	bool changed = false;
	
	/* sanity checks */
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	if (sa == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active area");
		return OPERATOR_CANCELLED;
	}
	
	/* for 3D View, init depth buffer stuff used for 3D projections... */
	if (sa->spacetype == SPACE_VIEW3D) {
		wmWindow *win = CTX_wm_window(C);
		View3D *v3d = (View3D *)CTX_wm_space_data(C);
		RegionView3D *rv3d = ar->regiondata;
		
		/* init 3d depth buffers */
		view3d_operator_needs_opengl(C);
		view3d_region_operator_needs_opengl(win, ar);
		ED_view3d_autodist_init(scene, ar, v3d, 0);
		
		/* for camera view set the subrect */
		if (rv3d->persp == RV3D_CAMOB) {
			ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &subrect_data, true); /* no shift */
			subrect = &subrect_data;
		}
	}
	
	/* rect is rectangle of selection circle */
	rect.xmin = mx - radius;
	rect.ymin = my - radius;
	rect.xmax = mx + radius;
	rect.ymax = my + radius;
	
	
	/* find visible strokes, and select if hit */
	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		changed |= gp_stroke_do_circle_sel(gps, ar, &ar->v2d, subrect, 
										   mx, my, radius, select, &rect);
	}
	CTX_DATA_END;
	
	/* updates */
	if (changed) {
		WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	}
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_circle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Circle Select";
	ot->description = "Select Grease Pencil strokes using brush selection";
	ot->idname = "GPENCIL_OT_select_circle";
	
	/* callbacks */
	ot->invoke = WM_gesture_circle_invoke;
	ot->modal = WM_gesture_circle_modal;
	ot->exec = gpencil_circle_select_exec;
	ot->poll = gpencil_select_poll;
	ot->cancel = WM_gesture_circle_cancel;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 1, 1, INT_MAX, "Radius", "", 1, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Gesture Mode", "", INT_MIN, INT_MAX);
}

/* ********************************************** */
/* Mouse Click to Select */

static int gpencil_select_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View2D *v2d = &ar->v2d;
	
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	
	/* "radius" is simply a threshold (screen space) to make it easier to test with a tolerance */
	const float radius = 0.75f * U.widget_unit;
	const int radius_squared = (int)(radius * radius);
	
	rctf *subrect = NULL;       /* for using the camera rect within the 3d view */
	rctf subrect_data = {0.0f};
	
	bool extend = RNA_boolean_get(op->ptr, "extend");
	bool deselect = RNA_boolean_get(op->ptr, "deselect");
	bool toggle = RNA_boolean_get(op->ptr, "toggle");
	bool whole = RNA_boolean_get(op->ptr, "entire_strokes");
	
	int location[2] = {0};
	int mx, my;
	
	bGPDstroke *hit_stroke = NULL;
	bGPDspoint *hit_point = NULL;
	
	/* sanity checks */
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	if (sa == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active area");
		return OPERATOR_CANCELLED;
	}
	
	/* for 3D View, init depth buffer stuff used for 3D projections... */
	if (sa->spacetype == SPACE_VIEW3D) {
		wmWindow *win = CTX_wm_window(C);
		View3D *v3d = (View3D *)CTX_wm_space_data(C);
		RegionView3D *rv3d = ar->regiondata;
		
		/* init 3d depth buffers */
		view3d_operator_needs_opengl(C);
		view3d_region_operator_needs_opengl(win, ar);
		ED_view3d_autodist_init(scene, ar, v3d, 0);
		
		/* for camera view set the subrect */
		if (rv3d->persp == RV3D_CAMOB) {
			ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &subrect_data, true); /* no shift */
			subrect = &subrect_data;
		}
	}
	
	/* get mouse location */
	RNA_int_get_array(op->ptr, "location", location);
	
	mx = location[0];
	my = location[1];
	
	/* First Pass: Find stroke point which gets hit */
	/* XXX: maybe we should go from the top of the stack down instead... */
	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		bGPDspoint *pt;
		int i;
		int hit_index = -1;
		
		/* firstly, check for hit-point */
		for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
			int x0, y0;
			
			gp_point_to_xy(ar, v2d, subrect, gps, pt, &x0, &y0);
		
			/* do boundbox check first */
			if (!ELEM(V2D_IS_CLIPPED, x0, x0)) {
				/* only check if point is inside */
				if (((x0 - mx) * (x0 - mx) + (y0 - my) * (y0 - my)) <= radius_squared) {				
					hit_stroke = gps;
					hit_point  = pt;
					break;
				}
			}
		}
		
		/* skip to next stroke if nothing found */
		if (hit_index == -1) 
			continue;
	}
	CTX_DATA_END;
	
	/* Abort if nothing hit... */
	if (ELEM(NULL, hit_stroke, hit_point)) {
		return OPERATOR_CANCELLED;
	}
	
	/* adjust selection behaviour - for toggle option */
	if (toggle) {
		deselect = (hit_point->flag & GP_SPOINT_SELECT) != 0;
	}
	
	/* If not extending selection, deselect everything else */
	if (extend == false) {
		CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
		{			
			/* deselect stroke and its points if selected */
			if (gps->flag & GP_STROKE_SELECT) {
				bGPDspoint *pt;
				int i;
			
				/* deselect points */
				for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
					pt->flag &= ~GP_SPOINT_SELECT;
				}
				
				/* deselect stroke itself too */
				gps->flag &= ~GP_STROKE_SELECT;
			}
		}
		CTX_DATA_END;
	}
	
	/* Perform selection operations... */
	if (whole) {
		bGPDspoint *pt;
		int i;
		
		/* entire stroke's points */
		for (i = 0, pt = hit_stroke->points; i < hit_stroke->totpoints; i++, pt++) {
			if (deselect == false)
				pt->flag |= GP_SPOINT_SELECT;
			else
				pt->flag &= ~GP_SPOINT_SELECT;
		}
		
		/* stroke too... */
		if (deselect == false)
			hit_stroke->flag |= GP_STROKE_SELECT;
		else
			hit_stroke->flag &= ~GP_STROKE_SELECT;
	}
	else {
		/* just the point (and the stroke) */
		if (deselect == false) {
			/* we're adding selection, so selection must be true */
			hit_point->flag  |= GP_SPOINT_SELECT;
			hit_stroke->flag |= GP_STROKE_SELECT;
		}
		else {
			/* deselect point */
			hit_point->flag &= ~GP_SPOINT_SELECT;
			
			/* ensure that stroke is selected correctly */
			gpencil_stroke_sync_selection(hit_stroke);
		}
	}
	
	/* updates */
	if (hit_point != NULL) {
		WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	}
	
	return OPERATOR_FINISHED;
}

static int gpencil_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	RNA_int_set_array(op->ptr, "location", event->mval);
	return gpencil_select_exec(C, op);
}

void GPENCIL_OT_select(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Select";
	ot->description = "Select Grease Pencil strokes and/or stroke points";
	ot->idname = "GPENCIL_OT_select";
	
	/* callbacks */
	ot->invoke = gpencil_select_invoke;
	ot->exec = gpencil_select_exec;
	ot->poll = gpencil_select_poll;
	
	/* flag */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	WM_operator_properties_mouse_select(ot);
	
	RNA_def_boolean(ot->srna, "entire_strokes", false, "Entire Strokes", "Select entire strokes instead of just the nearest stroke vertex");
	
	prop = RNA_def_int_vector(ot->srna, "location", 2, NULL, INT_MIN, INT_MAX, "Location", "Mouse location", INT_MIN, INT_MAX);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* ********************************************** */

 