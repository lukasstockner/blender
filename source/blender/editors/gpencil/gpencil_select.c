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
	
	/* for "toggle", test for existing selected strokes */
	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		
		GP_VISIBLE_STROKES_ITER_BEGIN(gpd, gps)
		{
			if (gps->flag & GP_STROKE_SELECT) {
				action = SEL_DESELECT;
				
				gpl = NULL; /* XXX: hack to stop iterating further, since we've found our target... */
				break;
			}
		}
		GP_STROKES_ITER_END;
	}
	
	/* select or deselect all strokes */
	GP_VISIBLE_STROKES_ITER_BEGIN(gpd, gps)
	{
		switch (action) {
			case SEL_SELECT:	
				gps->flag |= GP_STROKE_SELECT;
				break;
			case SEL_DESELECT:
				gps->flag &= ~GP_STROKE_SELECT;
				break;
			case SEL_INVERT:
				gps->flag ^= GP_STROKE_SELECT;
				break;
		}
	}
	GP_STROKES_ITER_END;
	
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
	
	if (gps->totpoints == 1) {
		gp_point_to_xy(ar, v2d, subrect, gps, gps->points, &x0, &y0);
		
		/* do boundbox check first */
		if ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(rect, x0, y0)) {
			/* only check if point is inside */
			if (((x0 - mx) * (x0 - mx) + (y0 - my) * (y0 - my)) <= radius * radius) {
				/* change selection */
				if (select)
					gps->flag |= GP_STROKE_SELECT;
				else
					gps->flag &= ~GP_STROKE_SELECT;
				
				return true;
			}
		}
	}
	else {
		/* loop over the points in the stroke, checking for intersections 
		 *  - an intersection means that we touched the stroke
		 */
		for (i = 0; (i + 1) < gps->totpoints; i++) {
			/* get points to work with */
			pt1 = gps->points + i;
			pt2 = gps->points + i + 1;
			
			gp_point_to_xy(ar, v2d, subrect, gps, pt1, &x0, &y0);
			gp_point_to_xy(ar, v2d, subrect, gps, pt2, &x1, &y1);
			
			/* check that point segment of the boundbox of the eraser stroke */
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
					/* change selection */
					if (select)
						gps->flag |= GP_STROKE_SELECT;
					else
						gps->flag &= ~GP_STROKE_SELECT;
						
					/* we only need to change the selection once... when we detect a hit! */
					return true;
				}
			}
		}
	}
	
	return false;
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
	
	
	/* for 3D View, init depth buffer stuff used for 3D projections... */
	if (sa == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active area");
		return OPERATOR_CANCELLED;
	}
	
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
	GP_VISIBLE_STROKES_ITER_BEGIN(gpd, gps)
	{
		changed |= gp_stroke_do_circle_sel(gps, ar, &ar->v2d, subrect, 
										   mx, my, radius, select, &rect);
	}
	GP_STROKES_ITER_END;
	
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

 