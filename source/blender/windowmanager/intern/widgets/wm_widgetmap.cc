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
 * Contributor(s): Julian Eisel, Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/widgets/wm_widgetmap.cpp
 *  \ingroup wm
 */

#include "BKE_context.h"

#include "BLI_math.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_glew.h"
#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "wm_cursors.h"
#include "WM_types.h"
#include "wm.h" // tmp

#include "wm_widgetgroup.h"
#include "wm_widgetgrouptype.h"
#include "wm_widgetmaptype.h"
#include "wm_widget.h"
#include "wm_widgets_c_api.h"
#include "wm_widgetmap.h" // own include


wmWidgetMap::wmWidgetMap()
{
	
}

void wmWidgetMap::find_from_type(
        wmWidgetMap *wmap, const char *idname,
        const int spaceid, const int regionid,
        const bool is_3d)
{
	wmWidgetMapType *wmaptype = WM_widgetmaptype_find(idname, spaceid, regionid, is_3d, true);

	wmap->type = wmaptype;

	/* create all widgetgroups for this widgetmap. We may create an empty one
	 * too in anticipation of widgets from operators etc */
	for (wmWidgetGroupType *wgrouptype = (wmWidgetGroupType *)wmaptype->widgetgrouptypes.first;
	     wgrouptype;
	     wgrouptype = wgrouptype->next)
	{
		wmWidgetGroup *wgroup = new wmWidgetGroup;
		wgroup->type_cxx = wgrouptype;
		BLI_addtail(&wmap->widgetgroups, wgroup);
	}
}

static int widgetmap_find_highlighted_widget_3d_intern(
        ListBase *visible_widgets, const bContext *C, const wmEvent *event,
        const float hotspot)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = (View3D *)sa->spacedata.first;
	RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
	rctf rect, selrect;
	GLuint buffer[64];      // max 4 items per select, so large enuf
	short hits;
	const bool do_passes = GPU_select_query_check_active();


	rect.xmin = event->mval[0] - hotspot;
	rect.xmax = event->mval[0] + hotspot;
	rect.ymin = event->mval[1] - hotspot;
	rect.ymax = event->mval[1] + hotspot;

	selrect = rect;

	ED_view3d_winmatrix_set(ar, v3d, &rect);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);

	if (do_passes)
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_NEAREST_FIRST_PASS, 0);
	else
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_ALL, 0);
	/* do the drawing */
	widget_find_active_3D_loop(C, visible_widgets);

	hits = GPU_select_end();

	if (do_passes) {
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_NEAREST_SECOND_PASS, hits);
		widget_find_active_3D_loop(C, visible_widgets);
		GPU_select_end();
	}

	ED_view3d_winmatrix_set(ar, v3d, NULL);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);

	if (hits == 1) {
		return buffer[3];
	}
	/* find the widget the value belongs to */
	else if (hits > 1) {
		GLuint val, dep, mindep = 0, minval = -1;
		int a;

		/* we compare the hits in buffer, but value centers highest */
		/* we also store the rotation hits separate (because of arcs) and return hits on other widgets if there are */

		for (a = 0; a < hits; a++) {
			dep = buffer[4 * a + 1];
			val = buffer[4 * a + 3];

			if (minval == -1 || dep < mindep) {
				mindep = dep;
				minval = val;
			}
		}

		return minval;
	}

	return -1;
}

static void widgetmap_prepare_visible_widgets_3d(wmWidgetMap *wmap, ListBase *visible_widgets, bContext *C)
{
	wmWidget *widget;

	for (wmWidgetGroup *wgroup = (wmWidgetGroup *)wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
		if (wgroup->type_cxx->poll_check(C)) {
			for (widget = (wmWidget *)wgroup->widgets.first; widget; widget = widget->next) {
				if (widget->render_3d_intersection && (widget->flag & WM_WIDGET_HIDDEN) == 0) {
					BLI_addhead(visible_widgets, BLI_genericNodeN(widget));
				}
			}
		}
	}
}

static wmWidget *widgetmap_find_highlighted_widget_3d(
        bContext *C, wmWidgetMap *wmap, const wmEvent *event,
        unsigned char *part)
{
	wmWidget *result = NULL;
	ListBase visible_widgets = {0};
	const float hotspot = 14.0f;
	int ret;

	widgetmap_prepare_visible_widgets_3d(wmap, &visible_widgets, C);

	*part = 0;
	/* set up view matrices */
	view3d_operator_needs_opengl(C);

	ret = widgetmap_find_highlighted_widget_3d_intern(&visible_widgets, C, event, 0.5f * hotspot);

	if (ret != -1) {
		LinkData *link;
		int retsec;
		retsec = widgetmap_find_highlighted_widget_3d_intern(&visible_widgets, C, event, 0.2f * hotspot);

		if (retsec != -1)
			ret = retsec;

		link = (LinkData *)BLI_findlink(&visible_widgets, ret >> 8);
		*part = ret & 255;
		result = (wmWidget *)link->data;
	}

	BLI_freelistN(&visible_widgets);

	return result;
}

static wmWidget *widgetmap_find_highlighted_widget(
        bContext *C, wmWidgetMap *wmap, const wmEvent *event,
        unsigned char *part)
{
	wmWidget *widget;

	for (wmWidgetGroup *wgroup = (wmWidgetGroup *)wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
		if (wgroup->type_cxx->poll_check(C)) {
			for (widget = (wmWidget *)wgroup->widgets.first; widget; widget = widget->next) {
				if (widget->intersect) {
					if ((*part = widget->intersect(C, event, widget)))
						return widget;
				}
			}
		}
	}

	return NULL;
}

wmWidget *wmWidgetMap::find_highlighted_widget(bContext *C, const wmEvent *event, unsigned char *part)
{
	if (type->is_3d) {
		return widgetmap_find_highlighted_widget_3d(C, this, event, part);
	}
	else {
		return widgetmap_find_highlighted_widget(C, this, event, part);
	}
}

void wmWidgetMap::set_highlighted_widget(bContext *C, wmWidget *widget, unsigned char part)
{
	if ((widget != wmap_context.highlighted_widget) || (widget && part != widget->highlighted_part)) {
		if (wmap_context.highlighted_widget) {
			wmap_context.highlighted_widget->flag &= ~WM_WIDGET_HIGHLIGHT;
			wmap_context.highlighted_widget->highlighted_part = 0;
		}

		wmap_context.highlighted_widget = widget;

		if (widget) {
			widget->flag |= WM_WIDGET_HIGHLIGHT;
			widget->highlighted_part = part;
			wmap_context.activegroup = widget->wgroup;

			if (C && widget->get_cursor) {
				wmWindow *win = CTX_wm_window(C);
				WM_cursor_set(win, widget->get_cursor(widget));
			}
		}
		else {
			wmap_context.activegroup = NULL;
			if (C) {
				wmWindow *win = CTX_wm_window(C);
				WM_cursor_set(win, CURSOR_STD);
			}
		}

		/* tag the region for redraw */
		if (C) {
			ARegion *ar = CTX_wm_region(C);
			ED_region_tag_redraw(ar);
		}
	}
}

void wmWidgetMap::set_active_widget(bContext *C, const wmEvent *event, wmWidget *widget)
{
	if (widget) {
		if (widget->opname) {
			wmOperatorType *ot = WM_operatortype_find(widget->opname, 0);

			if (ot) {
				/* first activate the widget itself */
				if (widget->invoke && widget->handler) {
					widget->flag |= WM_WIDGET_ACTIVE;
					widget->invoke(C, event, widget);
				}
				wmap_context.active_widget = widget;

				WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &widget->opptr);

				/* we failed to hook the widget to the operator handler or operator was cancelled, return */
				if (!wmap_context.active_widget) {
					widget->flag &= ~WM_WIDGET_ACTIVE;
					/* first activate the widget itself */
					if (widget->interaction_data) {
						MEM_freeN(widget->interaction_data);
						widget->interaction_data = NULL;
					}
				}
				return;
			}
			else {
				printf("Widget error: operator not found\n");
				wmap_context.active_widget = NULL;
				return;
			}
		}
		else {
			if (widget->invoke && widget->handler) {
				widget->flag |= WM_WIDGET_ACTIVE;
				widget->invoke(C, event, widget);
				wmap_context.active_widget = widget;
			}
		}
	}
	else {
		widget = wmap_context.active_widget;

		/* deactivate, widget but first take care of some stuff */
		if (widget) {
			widget->flag &= ~WM_WIDGET_ACTIVE;
			/* first activate the widget itself */
			if (widget->interaction_data) {
				MEM_freeN(widget->interaction_data);
				widget->interaction_data = NULL;
			}
		}
		wmap_context.active_widget = NULL;

		ED_region_tag_redraw(CTX_wm_region(C));
		WM_event_add_mousemove(C);
	}
}

