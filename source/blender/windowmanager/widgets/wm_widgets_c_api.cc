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

/** \file blender/windowmanager/widgets/wm_widgets_c_api.cc
 *  \ingroup wm
 *
 * C-API for wmWidget types.
 */

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "wm_widgetmap.h"
#include "wm_widgetmaptype.h"
#include "wm_widgetgrouptype.h"
#include "wm_widget.h"
#include "wm_widgets_c_api.h" // own include


/* -------------------------------------------------------------------- */
/* wmWidgetMap */

wmWidgetMap *WM_widgetmap_new(const char *idname, const int spaceid, const int regionid, const bool is_3d)
{
	return new wmWidgetMap(idname, spaceid, regionid, is_3d);
}
void WM_widgetmap_remove(wmWidgetMap *wmap, ListBase *widgetmaps)
{
	wmap->unregister(widgetmaps);
	delete wmap;
}

void WM_widgetmap_widgets_update(wmWidgetMap *wmap, const bContext *C)
{
	BLI_assert(wmap != NULL);
	wmap->update(C);
}
void WM_widgetmap_widgets_draw(
        const bContext *C, wmWidgetMap *wmap,
        const bool in_scene, const bool free_draw_widgets)
{
	BLI_assert(wmap != NULL);
	wmap->draw(C, in_scene, free_draw_widgets);
}

bool WM_widgetmap_cursor_set(wmWidgetMap *wmap, wmWindow *win)
{
	return wmap->cursor_update(win);
}

void wm_widgetmap_highlighted_widget_set(bContext *C, wmWidgetMap *wmap, wmWidget *widget, unsigned char part)
{
	wmap->set_highlighted_widget(C, widget, part);
}
wmWidget *wm_widgetmap_highlighted_widget_get(wmWidgetMap *wmap)
{
	return wmap->wmap_context.highlighted_widget;
}
wmWidget *wm_widgetmap_highlighted_widget_find(
        wmWidgetMap *wmap, bContext *C, const wmEvent *event,
        unsigned char *part)
{
	return wmap->find_highlighted_widget(C, event, part);
}

void wm_widgetmap_active_widget_set(wmWidgetMap *wmap, bContext *C, const wmEvent *event, wmWidget *widget)
{
	wmap->set_active_widget(C, event, widget);
}
wmWidget *wm_widgetmap_active_widget_get(wmWidgetMap *wmap)
{
	return wmap->wmap_context.active_widget;
}

bool WM_widgetmap_select_all(wmWidgetMap *wmap, bContext *C, const int action)
{
	return wmap->select_all(C, action);
}

wmWidgetGroup *wm_widgetmap_active_group_get(wmWidgetMap *wmap)
{
	return wmap->get_active_group();
}


/* -------------------------------------------------------------------- */
/* wmWidgetGroupType */

/**
 * Create and register a new widget-group-type
 */
wmWidgetGroupType *WM_widgetgrouptype_new(
        int (*poll)(const bContext *, wmWidgetGroupType *),
        void (*create)(const bContext *, wmWidgetGroup *),
        wmKeyMap *(*keymap_init)(wmKeyConfig *, const char *),
        const Main *bmain, const char *mapidname, const char *name,
        const short spaceid, const short regionid, const bool is_3d)
{
	wmWidgetMapType *wmaptype = WM_widgetmaptype_find(mapidname, spaceid, regionid, is_3d, false);

	if (!wmaptype) {
		fprintf(stderr, "widgetgrouptype creation: widgetmap type does not exist\n");
		return NULL;
	}

	return new wmWidgetGroupType(
	            wmaptype, poll, create, keymap_init,
	            bmain, mapidname, name, spaceid,
	            regionid, is_3d);
}

void WM_widgetgrouptype_remove(bContext *C, Main *bmain, wmWidgetGroupType *wgrouptype)
{
	wgrouptype->unregister(C, bmain);
	delete wgrouptype;
}

void WM_widgetgrouptype_attach_to_handler(
        bContext *C, wmWidgetGroupType *wgrouptype,
        wmEventHandler *handler, wmOperator *op)
{
	wgrouptype->attach_to_handler(C, handler, op);
}

void WM_widgetgrouptype_idname_set(wmWidgetGroupType *wgrouptype, const char *idname)
{
	return wgrouptype->set_idname(idname);
}
/**
 * \returns sizeof(wgrouptype->idname).
 */
size_t WM_widgetgrouptype_idname_get(wmWidgetGroupType *wgrouptype, char *r_idname)
{
	return wgrouptype->get_idname(r_idname);
}

wmOperator *WM_widgetgrouptype_operator_get(wmWidgetGroupType *wgrouptype)
{
	return wgrouptype->get_operator();
}

wmKeyMap *WM_widgetgrouptype_user_keymap_get(wmWidgetGroupType *wgrouptype)
{
	return wgrouptype->get_keymap();
}


/* -------------------------------------------------------------------- */
/* wmWidget */

wmWidget *wm_widget_new(wmWidgetGroup *wgroup, const char *name)
{
	return new wmWidget(wgroup, name);
}
void WM_widget_remove(wmWidget *widget, ListBase *widgetlist)
{
	widget->unregister(widgetlist);
	OBJECT_GUARDED_DELETE(widget, wmWidget);
}

void wm_widget_handle(wmWidget *widget, bContext *C, const wmEvent *event, const int handle_flag)
{
	widget->handle(C, event, handle_flag);
}

void wm_widget_tweak_cancel(wmWidget *widget, bContext *C)
{
	widget->tweak_cancel(C);
}

void wm_widget_select(wmWidget *widget, wmWidgetMap *wmap, bContext *C)
{
	widget->add_to_selection(wmap, C);
}

void wm_widget_deselect(wmWidget *widget, wmWidgetMap *wmap, const bContext *C)
{
	widget->remove_from_selection(wmap, C);
}

const char *wm_widget_idname_get(wmWidget *widget)
{
	return widget->idname_get();
}

void WM_widget_set_property(wmWidget *widget, const int slot, PointerRNA *ptr, const char *propname)
{
	widget->property_set(slot, ptr, propname);
}

PointerRNA *WM_widget_set_operator(wmWidget *widget, const char *opname)
{
	return widget->operator_set(opname);
}

const char *WM_widget_get_operatorname(wmWidget *widget)
{
	return widget->operatorname_get();
}

void WM_widget_set_func_handler(wmWidget *widget, int (*handler)(bContext *, const wmEvent *, wmWidget *, const int ))
{
	widget->func_handler_set(handler);
}

void WM_widget_set_func_select(wmWidget *widget, void (*select)(bContext *, wmWidget *, const int action))
{
	widget->func_select_set(select);
}

void WM_widget_set_origin(wmWidget *widget, const float origin[3])
{
	widget->origin_set(origin);
}

void WM_widget_set_offset(struct wmWidget *widget, const float offset[3])
{
	widget->offset_set(offset);
}

void WM_widget_set_flag(struct wmWidget *widget, const int flag, const bool enable)
{
	widget->flag_set(flag, enable);
}
bool WM_widget_flag_is_set(wmWidget *widget, const int flag)
{
	return widget->flag_is_set(flag);
}

void WM_widget_set_scale(struct wmWidget *widget, float scale)
{
	widget->scale_set(scale);
}

void WM_widget_set_line_width(struct wmWidget *widget, const float line_width)
{
	widget->line_width_set(line_width);
}

void WM_widget_set_colors(struct wmWidget *widget, const float col[4], const float col_hi[4])
{
	widget->colors_set(col, col_hi);
}

