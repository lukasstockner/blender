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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, Julian Eisel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/widgets/widget_library/dial_widget.cc
 *  \ingroup wm
 *
 * \name Facemap Widget
 *
 * 3D Widget
 *
 * \brief Widget representing shape of a face map. Currently no own handling, use with operator only.
 */

#include <new>

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_view3d_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "wm.h"

#include "../wm_widget.h"
#include "widget_geometry.h"

typedef struct FacemapWidget: wmWidget {
	FacemapWidget(wmWidgetGroup *wgroup, const char *name);

	Object *ob;
	int facemap;
	int style;
} FacemapWidget;


FacemapWidget::FacemapWidget(wmWidgetGroup *wgroup, const char *name)
    : wmWidget(wgroup, name)
{
	
}

static void widget_facemap_draw(const bContext *C, wmWidget *widget)
{
	FacemapWidget *fmap_widget = (FacemapWidget *)widget;
	const float *col = (widget->flag & WM_WIDGET_SELECTED) ? widget->col_hi : widget->col;

	glPushMatrix();
	glMultMatrixf(fmap_widget->ob->obmat);
	glTranslate3fv(widget->offset);
	ED_draw_object_facemap(CTX_data_scene(C), fmap_widget->ob, col, fmap_widget->facemap);
	glPopMatrix();
}

static void widget_facemap_render_3d_intersect(const bContext *C, wmWidget *widget, int selectionbase)
{
	GPU_select_load_id(selectionbase);
	widget_facemap_draw(C, widget);
}

#if 0
static int widget_facemap_invoke(bContext *UNUSED(C), const wmEvent *event, wmWidget *widget)
{
	return OPERATOR_PASS_THROUGH;
}

static int widget_facemap_handler(bContext *C, const wmEvent *event, wmWidget *widget)
{
	return OPERATOR_PASS_THROUGH;
}
#endif

/** \name Facemap Widget API
 *
 * \{ */

wmWidget *WIDGET_facemap_new(
        wmWidgetGroup *wgroup, const char *name, const int style,
        Object *ob, const int facemap)
{
	FacemapWidget *fmap_widget = OBJECT_GUARDED_NEW_CALLOC(FacemapWidget, wgroup, name);

	BLI_assert(facemap > -1);

	fmap_widget->draw                    = widget_facemap_draw;
//	fmap_widget->widget.invoke           = widget_facemap_invoke;
//	fmap_widget->widget.bind_to_prop     = NULL;
//	fmap_widget->widget.handler          = widget_facemap_handler;
	fmap_widget->render_3d_intersection  = widget_facemap_render_3d_intersect;
	fmap_widget->ob                      = ob;
	fmap_widget->facemap                 = facemap;
	fmap_widget->style                   = style;

	return (wmWidget *)fmap_widget;
}

bFaceMap *WIDGET_facemap_get_fmap(wmWidget *widget)
{
	FacemapWidget *fmap_widget = (FacemapWidget *)widget;
	return (bFaceMap *)BLI_findlink(&fmap_widget->ob->fmaps, fmap_widget->facemap);
}

/** \} */ // Facemap Widget API


/* -------------------------------------------------------------------- */

void fix_linking_widget_facemap(void)
{
	(void)0;
}
