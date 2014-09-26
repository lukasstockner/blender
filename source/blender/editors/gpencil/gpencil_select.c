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
 * The Original Code is Copyright (C) 2014, Blender Foundation, Joshua Leung
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

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_tracking.h"

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
	bGPDlayer *gpl;
	int action = RNA_enum_get(op->ptr, "action");
	
	/* for "toggle", test for existing selected strokes */
	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			if (!(gpl->flag & GP_LAYER_HIDE) && (gpl->actframe)) {
				bGPDframe *gpf = gpl->actframe;
				bGPDstroke *gps;
				
				for (gps = gpf->strokes.first; gps; gps = gps->next) {
					if (gps->flag & GP_STROKE_SELECT) {
						action = SEL_DESELECT;
						break;
					}
				}
			}
			
			if (action != SEL_SELECT)
				break;
		}
	}
	
	/* select or deselect all strokes */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		if (!(gpl->flag & GP_LAYER_HIDE) && (gpl->actframe)) {
			bGPDframe *gpf = gpl->actframe;
			bGPDstroke *gps;
			
			for (gps = gpf->strokes.first; gps; gps = gps->next) {
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
		}
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

 