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
 * The Original Code is Copyright (C) 2015, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Brush based operators for editing Grease Pencil strokes
 */

/** \file blender/editors/gpencil/gpencil_edit.c
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

#include "BLF_translation.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* General Brush Editing Context */

/* Context for brush operators */
typedef struct tGP_BrushEditData {
	/* Brush Settings */
	GP_BrushEdit_Settings *settings;
	GP_EditBrush_Data *brush;
	
	eGP_EditBrush_Types brush_type;
	eGP_EditBrush_Flag  flag;
	
	/* Space Conversion Data */
	GP_SpaceConversion gsc;
	
	
	/* Is the brush currently painting? */
	bool is_painting;
	
	/* Start of new sculpt stroke */
	bool first;
	
	
	/* Brush Runtime Data: */
	/* - position and pressure
	 * - the *_prev variants are the previous values
	 */
	int   mval[2], mval_prev[2];
	float pressure, pressure_prev;
	
	/* brush geometry (bounding box) */
	rcti brush_rect;
	
	/* Custom data for certain brushes */
	void *customdata;
} tGP_BrushEditData;


/* Callback for performing some brush operation on a single point */
typedef bool (*GP_BrushApplyCb)(tGP_BrushEditData *gso, bGPDstroke *gps, int i,
                                const int mx, const int my, const int radius,
                                const int x0, const int y0);

/* ************************************************ */
/* Utility Functions */

/* Context ---------------------------------------- */

/* Get the sculpting settings */
static GP_BrushEdit_Settings *gpsculpt_get_settings(Scene *scene)
{
	return &scene->toolsettings->gp_sculpt;
}

/* Get the active brush */
static GP_EditBrush_Data *gpsculpt_get_brush(Scene *scene)
{
	GP_BrushEdit_Settings *gset = &scene->toolsettings->gp_sculpt;
	return &gset->brush[gset->brushtype];
}

/* Brush Operations ------------------------------- */

/* Invert behaviour of brush? */
// XXX: Maybe this should work the way that pose sculpt did it instead?
static bool gp_brush_invert_check(tGP_BrushEditData *gso)
{
	/* The basic setting is the brush's setting (from the panel) */
	bool invert = ((gso->brush->flag & GP_EDITBRUSH_FLAG_INVERT) != 0);
	
	/* During runtime, the user can hold down the Ctrl key to invert the basic behaviour */
	if (gso->flag & GP_EDITBRUSH_FLAG_INVERT) {
		invert ^= true;
	}
		
	return invert;
}

/* Compute strength of effect */
static float gp_brush_influence_calc(tGP_BrushEditData *gso, const int radius,
                                     const int mx, const int my,
                                     const int x0, const int y0)
{
	GP_EditBrush_Data *brush = gso->brush;
	
	/* basic strength factor from brush settings */
	float influence = brush->strength;
	
	/* use pressure? */
	if (brush->flag & GP_EDITBRUSH_FLAG_USE_PRESSURE) {
		influence *= gso->pressure;
	}
	
	/* distance fading */
	if (brush->flag & GP_EDITBRUSH_FLAG_USE_FALLOFF) {
		float distance = sqrtf((mx - x0) * (mx - x0) + (my - y0) * (my - y0));
		float fac    = 1.0f - (distance / (float)radius); 
		
		influence *= fac;
	}
	
	/* return influence */
	return influence;
}

/* ************************************************ */
/* Brush Callbacks */
/* This section defines the callbacks used by each brush to perform their magic.
 * These are called on each point within the brush's radius.
 */

/* ----------------------------------------------- */
/* Smooth Brush */

#if 0

/* initialise custom data for handling this stroke */
static bool gp_brush_smooth_stroke_init(tGP_BrushEditData *gso, bGPDstroke *gps)
{
	
}

/* compute average values for each point */
static bool gp_brush_calc_average_co(tGP_BrushEditData *gso, bGPDstroke *gps, int i,
                                     const int mx, const int my, const int radius,
                                     const int x0, const int y0)
{
	bGPDspoint *pt = &gps->points[i];
	
	
	return true;
}

/* apply smoothing by blending between the average coordinates and the current coordinates */
static bool gp_brush_smooth_apply(tGP_BrushEditData *gso, bGPDstroke *gps, int i,
                                  const int mx, const int my, const int radius,
                                  const int x0, const int y0)
{
	bGPDspoint *pt = &gps->points[i];
	float inf = gp_brush_influence_calc(gso, radius, mx, my, x0, y0);
	float *sco;
	
	/* get smoothed coordinate */
	//sco = 
	
	return true;
}

#endif

/* ----------------------------------------------- */
/* Line Thickness Brush */

/* Make lines thicker or thinner by the specified amounts */
static bool gp_brush_thickness_apply(tGP_BrushEditData *gso, bGPDstroke *gps, int i,
                                     const int mx, const int my, const int radius,
                                     const int x0, const int y0)
{
	bGPDspoint *pt = gps->points + i;
	float inf = gp_brush_influence_calc(gso, radius, mx, my, x0, y0);
	
	/* apply */
	// XXX: this is much too strong, and it should probably do some smoothing with the surrounding stuff
	if (gp_brush_invert_check(gso)) {
		/* make line thinner - reduce stroke pressure */
		pt->pressure -= inf;
	}
	else {
		/* make line thicker - increase stroke pressure */
		pt->pressure += inf;
	}
	
	/* pressure must stay within [0.0, 1.0] */
	// XXX: volumetric strokes can circumvent this!
	//CLAMP(pt->pressure, 0.0f, 1.0f);
	
	if (pt->pressure < 0.0f)
		pt->pressure = 0.0f;
	
	return true;
}


/* ----------------------------------------------- */
/* Grab Brush */

/* ************************************************ */
/* Cursor drawing */

/* Helper callback for drawing the cursor itself */
static void gp_brush_drawcursor(bContext *C, int x, int y, void *UNUSED(customdata))
{
	GP_EditBrush_Data *brush = gpsculpt_get_brush(CTX_data_scene(C));
	
	if (brush) {
		glPushMatrix();
		
		glTranslatef((float)x, (float)y, 0.0f);
		
		/* TODO: toggle between add and remove? */
		glColor4ub(255, 255, 255, 128);
		
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		
		glutil_draw_lined_arc(0.0, M_PI * 2.0, brush->size, 40);
		
		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);
		
		glPopMatrix();
	}
}

/* Turn brush cursor in on/off */
static void gpencil_toggle_brush_cursor(bContext *C, bool enable)
{
	GP_BrushEdit_Settings *gset = gpsculpt_get_settings(CTX_data_scene(C));
	
	if (gset->paintcursor && !enable) {
		/* clear cursor */
		WM_paint_cursor_end(CTX_wm_manager(C), gset->paintcursor);
		gset->paintcursor = NULL;
	}
	else if (enable) {
		/* enable cursor */
		gset->paintcursor = WM_paint_cursor_activate(CTX_wm_manager(C), 
		                                             NULL, 
		                                             gp_brush_drawcursor, NULL);
	}
}


/* ************************************************ */
/* Grease Pencil Sculpting Operator */

/* Init/Exit ----------------------------------------------- */

static bool gpsculpt_brush_init(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	tGP_BrushEditData *gso;
	
	/* setup operator data */
	gso = MEM_callocN(sizeof(tGP_BrushEditData), "tGP_BrushEditData");
	op->customdata = gso;
	
	/* store state */
	gso->settings = gpsculpt_get_settings(scene);
	gso->brush = gpsculpt_get_brush(scene);
	
	gso->brush_type = gso->settings->brushtype;
	
	gso->is_painting = false;
	gso->first = true;
	
	/* setup space conversions */
	gp_point_conversion_init(C, &gso->gsc);
	
	
	/* update header */
	ED_area_headerprint(CTX_wm_area(C),
	                    IFACE_("Grease Pencil: Stroke Sculptmode | LMB to paint | RMB/Escape to Exit"
	                           " | Ctrl to Invert Action"));
	
	/* setup cursor drawing */
	WM_cursor_modal_set(CTX_wm_window(C), BC_CROSSCURSOR);
	gpencil_toggle_brush_cursor(C, true);
	
	return true;
}

static void gpsculpt_brush_exit(bContext *C, wmOperator *op)
{
	tGP_BrushEditData *gso = op->customdata;
	wmWindow *win = CTX_wm_window(C);
	
#if 0
	/* unregister timer (only used for realtime) */
	if (gso->timer) {
		WM_event_remove_timer(CTX_wm_manager(C), win, gso->timer);
	}
#endif

	/* disable cursor and headerprints */
	ED_area_headerprint(CTX_wm_area(C), NULL);
	WM_cursor_modal_restore(win);
	gpencil_toggle_brush_cursor(C, false);
	
	/* free operator data */
	MEM_freeN(gso);
	op->customdata = NULL;
}

/* poll callback for stroke sculpting operator(s) */
static int gpsculpt_brush_poll(bContext *C)
{
	/* NOTE: this is a bit slower, but is the most accurate... */
	return CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0;
}

/* Apply ----------------------------------------------- */

/* Apply brush operation to points in this stroke */
static bool gpsculpt_brush_do_stroke(tGP_BrushEditData *gso, bGPDstroke *gps, GP_BrushApplyCb apply)
{
	GP_SpaceConversion *gsc = &gso->gsc;
	rcti *rect = &gso->brush_rect;
	
	const int radius = gso->brush->size;
	const int mx = gso->mval[0];
	const int my = gso->mval[1];
	
	bGPDspoint *pt1, *pt2;
	int x0 = 0, y0 = 0;
	int x1 = 0, y1 = 0;
	int i;
	bool changed = false;
	
	if (gps->totpoints == 1) {
		gp_point_to_xy(gsc, gps, gps->points, &x0, &y0);
		
		/* do boundbox check first */
		if ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(rect, x0, y0)) {
			/* only check if point is inside */
			if (((x0 - mx) * (x0 - mx) + (y0 - my) * (y0 - my)) <= radius * radius) {
				/* apply operation to this point */
				changed = apply(gso, gps, 0, mx, my, radius, x0, y0);
				// XXX: Should we report "success" even if technically nothing happened?
			}
		}
	}
	else {
		/* Loop over the points in the stroke, checking for intersections 
		 *  - an intersection means that we touched the stroke
		 */
		for (i = 0; (i + 1) < gps->totpoints; i++) {
			/* Get points to work with */
			pt1 = gps->points + i;
			pt2 = gps->points + i + 1;
			
			/* Skip if neither one is selected (and we are only allowed to edit/consider selected points) */
			if (gso->settings->flag & GP_BRUSHEDIT_FLAG_SELECT_MASK) {
				if (!(pt1->flag & GP_SPOINT_SELECT) && !(pt2->flag & GP_SPOINT_SELECT))
					continue;
			}
			
			gp_point_to_xy(gsc, gps, pt1, &x0, &y0);
			gp_point_to_xy(gsc, gps, pt2, &x1, &y1);
			
			/* Check that point segment of the boundbox of the selection stroke */
			if (((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(rect, x0, y0)) ||
			    ((!ELEM(V2D_IS_CLIPPED, x1, y1)) && BLI_rcti_isect_pt(rect, x1, y1)))
			{
				/* Check if point segment of stroke had anything to do with
				 * eraser region  (either within stroke painted, or on its lines)
				 *  - this assumes that linewidth is irrelevant
				 */
				if (gp_stroke_inside_circle(gso->mval, gso->mval_prev, radius, x0, y0, x1, y1)) {
					/* Apply operation to these points */
					bool ok = false;
					
					/* To each point individually... */
					ok = apply(gso, gps, i, mx, my, radius, x0, y0);
					
					/* Only do the second point if this is the last segment,
					 * and it is unlikely that the point will get handled
					 * otherwise. 
					 * 
					 * NOTE: There is a small risk here that the second point wasn't really
					 *       actually in-range, but rather, that it only got in because
					 *       the line linking the points was!
					 */
					if (i + 1 == gps->totpoints - 1) {
						ok |= apply(gso, gps, i + 1, mx, my, radius, x1, y1);
					}
					
					changed |= ok;
				}
			}
		}
	}
	
	return changed;
}

/* Calculate settings for applying brush */
// TODO: Add in "substeps" stuff for finer application of brush effects
static void gpsculpt_brush_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
	tGP_BrushEditData *gso = op->customdata;
	const int radius = gso->brush->size;
	float mousef[2];
	int mouse[2];
	bool changed = false;
	
	/* Get latest mouse coordinates */
	RNA_float_get_array(itemptr, "mouse", mousef);
	gso->mval[0] = mouse[0] = (int)(mousef[0]);
	gso->mval[1] = mouse[1] = (int)(mousef[1]);
	
	gso->pressure = RNA_float_get(itemptr, "pressure");
	
	if (RNA_boolean_get(itemptr, "pen_flip"))
		gso->flag |= GP_EDITBRUSH_FLAG_INVERT;
	else
		gso->flag &= ~GP_EDITBRUSH_FLAG_INVERT;
	
	
	/* Store coordinates as reference, if operator just started running */
	if (gso->first) {
		gso->mval_prev[0]  = gso->mval[0];
		gso->mval_prev[1]  = gso->mval[1];
		gso->pressure_prev = gso->pressure;
	}
	
	/* Update brush_rect, so that it represents the bounding rectangle of brush */
	gso->brush_rect.xmin = mouse[0] - radius;
	gso->brush_rect.ymin = mouse[1] - radius;
	gso->brush_rect.xmax = mouse[0] + radius;
	gso->brush_rect.ymax = mouse[1] + radius;
	
	
	/* Find visible strokes, and perform operations on those if hit */
	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		switch (gso->brush_type) {
			case GP_EDITBRUSH_TYPE_SMOOTH: /* Smooth strokes */
			{
				// init
				// calc average
				// apply
				// cleanup
			}
			break;
			
			case GP_EDITBRUSH_TYPE_THICKNESS: /* Adjust stroke thickness */
			{
				changed |= gpsculpt_brush_do_stroke(gso, gps, gp_brush_thickness_apply);
			}
			break;
			
			case GP_EDITBRUSH_TYPE_GRAB: /* Grab points */
			{
				//changed |= gpsculpt_brush_do_stroke(gso, gps, apply);
			}
			break;
			
			case GP_EDITBRUSH_TYPE_RANDOMISE: /* Apply jitter */
			{
				//changed |= gpsculpt_brush_do_stroke(gso, gps, apply);
			}
			break;
			
			default:
				printf("ERROR: Unknown type of GPencil Sculpt brush - %d\n", gso->brush_type);
				break;
		}
	}
	CTX_DATA_END;
	
	
	/* updates */
	if (changed) {
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	}
	
	/* store values for next step */
	gso->mval_prev[0]  = gso->mval[0];
	gso->mval_prev[1]  = gso->mval[1];
	gso->pressure_prev = gso->pressure;
	gso->first = false;
}

/* Running --------------------------------------------- */

/* helper - a record stroke, and apply paint event */
static void gpsculpt_brush_apply_event(bContext *C, wmOperator *op, const wmEvent *event)
{
	tGP_BrushEditData *gso = op->customdata;
	PointerRNA itemptr;
	float mouse[2];
	int tablet = 0;
	
	VECCOPY2D(mouse, event->mval);
	
	/* fill in stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);
	
	RNA_float_set_array(&itemptr, "mouse", mouse);
	RNA_boolean_set(&itemptr, "pen_flip", event->ctrl != false);
	RNA_boolean_set(&itemptr, "is_start", gso->first);
	
	/* handle pressure sensitivity (which is supplied by tablets) */
	if (event->tablet_data) {
		const wmTabletData *wmtab = event->tablet_data;
		float pressure = wmtab->Pressure;
		
		tablet = (wmtab->Active != EVT_TABLET_NONE);
		
		/* special exception here for too high pressure values on first touch in
		 * windows for some tablets: clamp the values to be sane
		 */
		if (tablet && (pressure >= 0.99f)) {
			pressure = 1.0f;
		}		
		RNA_float_set(&itemptr, "pressure", pressure);
	}
	else {
		RNA_float_set(&itemptr, "pressure", 1.0f);
	}
	
	/* apply */
	gpsculpt_brush_apply(C, op, &itemptr);
}

/* reapply */
static int gpsculpt_brush_exec(bContext *C, wmOperator *op)
{
	if (!gpsculpt_brush_init(C, op))
		return OPERATOR_CANCELLED;
	
	RNA_BEGIN(op->ptr, itemptr, "stroke") 
	{
		gpsculpt_brush_apply(C, op, &itemptr);
	}
	RNA_END;
	
	gpsculpt_brush_exit(C, op);
	
	return OPERATOR_FINISHED;
}


/* start modal painting */
static int gpsculpt_brush_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{	
	//Scene *scene = CTX_data_scene(C);
	
	//GP_BrushEdit_Settings *gset = gpsculpt_get_settings(scene);
	//tGP_BrushEditData *gso = NULL;
	
	/* init painting data */
	if (!gpsculpt_brush_init(C, op))
		return OPERATOR_CANCELLED;
	
	//gso = op->customdata;
	
	/* register timer for increasing influence by hovering over an area */
#if 0
	if (ELEM(gset->brushtype, ...))
	{
		GP_EditBrush_Data *brush = gpsculpt_get_brush(scene);
		gso->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, brush->rate);
	}
#endif
	
	/* register modal handler */
	WM_event_add_modal_handler(C, op);
	
	return OPERATOR_RUNNING_MODAL;
}

/* painting - handle events */
static int gpsculpt_brush_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	tGP_BrushEditData *gso = op->customdata;
	bool redraw_region = false;
	
	/* The operator can be in 2 states: Painting and Idling */
	if (gso->is_painting) {
		/* Painting  */
		switch (event->type) {
			/* Mouse Move = Apply somewhere else */
			case MOUSEMOVE:
			case INBETWEEN_MOUSEMOVE:
				gpsculpt_brush_apply_event(C, op, event);
				break;
			
#if 0
			/* Timer Tick - Only if this was our own timer */
			case TIMER:
				if (event->customdata == gso->timer) {
					gso->timerTick = true;
					gpsculpt_brush_apply_event(C, op, event);
					pso->timerTick = false;
				}
				break;
#endif
			
			/* Painting mbut release = Stop painting (back to idle) */
			case LEFTMOUSE:
				//BLI_assert(event->val == KM_RELEASE);
				gso->is_painting = false;
				break;
				
			/* Abort painting if any of the usual things are tried */
			// XXX: should this be "emergency stop" instead? (i.e. operator_cancelled)
			case MIDDLEMOUSE:
			case RIGHTMOUSE:
			case ESCKEY:
				gpsculpt_brush_exit(C, op);
				return OPERATOR_FINISHED;
		}
	}
	else {
		/* Idling */
		switch (event->type) {
			/* Painting mbut press = Start painting (switch to painting state) */
			case LEFTMOUSE:
				/* do initial "click" apply */
				gso->is_painting = true;
				gso->first = true;
				
				gpsculpt_brush_apply_event(C, op, event);
				break;
				
			/* Exit modal operator, based on the "standard" ops */
			// XXX: should this be "emergency stop" instead? (i.e. operator_cancelled)
			case MIDDLEMOUSE:
			case RIGHTMOUSE:
			case ESCKEY:
				gpsculpt_brush_exit(C, op);
				return OPERATOR_FINISHED;
				
			/* Mouse movements should update the brush cursor - Just redraw the active region */
			case MOUSEMOVE:
			case INBETWEEN_MOUSEMOVE:
				redraw_region = true;
				break;
			
			/* Adjust brush settings */
			/* FIXME: Step increments and modifier keys are hardcoded here! */
			case WHEELUPMOUSE:
			case PADPLUSKEY:
				if (event->shift) {
					/* increase strength */
					gso->brush->strength += 0.05f;
				}
				else {
					/* increase brush size */
					gso->brush->size += 3;
				}
					
				redraw_region = true;
				break;
			
			case WHEELDOWNMOUSE: 
			case PADMINUS:
				if (event->shift) {
					/* decrease strength */
					gso->brush->strength -= 0.05f;
				}
				else {
					/* decrease brush size */
					gso->brush->size -= 3;
				}
					
				redraw_region = true;
				break;
			
			/* Unhandled event */
			default:
				// TODO: allow MMB viewnav to pass through
				break;
		}
	}
	
	/* Redraw region? */
	if (redraw_region) {
		ARegion *ar = CTX_wm_region(C);
		ED_region_tag_redraw(ar);
	}
	
	return OPERATOR_RUNNING_MODAL;
}


/* Operator --------------------------------------------- */

void GPENCIL_OT_brush_paint(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Stroke Sculpt";
	ot->idname = "GPENCIL_OT_brush_paint";
	ot->description = "Apply tweaks to strokes by painting over the strokes"; // XXX
	
	/* api callbacks */
	ot->exec = gpsculpt_brush_exec;
	ot->invoke = gpsculpt_brush_invoke;
	ot->modal = gpsculpt_brush_modal;
	ot->cancel = gpsculpt_brush_exit;
	ot->poll = gpsculpt_brush_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* properties */
	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

/* ************************************************ */
