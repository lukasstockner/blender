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

/** \file blender/windowmanager/intern/widgets/wm_widgets_c_api.cpp
 *  \ingroup wm
 *
 * C-API for wmWidget types.
 */

#include "DNA_windowmanager_types.h"

#include "wm_widgetmap.h"
#include "wm_widgetmaptype.h"
#include "wm_widgetgrouptype.h"
#include "wm_widgets_c_api.h" // own include


wmWidgetMap *WM_widgetmap_new(const char *idname, const int spaceid, const int regionid, const bool is_3d)
{
	return new wmWidgetMap(idname, spaceid, regionid, is_3d);
}
void WM_widgetmap_delete(wmWidgetMap *wmap)
{
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

GHash *wm_widgetmap_widget_hash_new(
        const bContext *C, wmWidgetMap *wmap,
        bool (*poll)(const wmWidget *, void *),
        void *data, const bool include_hidden)
{
	return wmap->widget_hash_new(C, poll, data, include_hidden);
}

void wm_widgetmap_highlighted_widget_set(bContext *C, wmWidgetMap *wmap, wmWidget *widget, unsigned char part)
{
	wmap->set_highlighted_widget(C, widget, part);
}
wmWidget *wm_widgetmap_highlighted_widget_get(wmWidgetMap *wmap)
{
	return wmap->wmap_context.highlighted_widget;
}

void wm_widgetmap_active_widget_set(wmWidgetMap *wmap, bContext *C, const wmEvent *event, wmWidget *widget)
{
	wmap->set_active_widget(C, event, widget);
}
wmWidget *wm_widgetmap_active_widget_get(wmWidgetMap *wmap)
{
	return wmap->wmap_context.active_widget;
}


wmWidget *wm_widgetmap_highlighted_widget_find(
        wmWidgetMap *wmap, bContext *C, const wmEvent *event,
        unsigned char *part)
{
	return wmap->find_highlighted_widget(C, event, part);
}

wmWidgetGroup *wm_widgetmap_active_group_get(wmWidgetMap *wmap)
{
	return wmap->get_active_group();
}


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

void WM_widgetgrouptype_delete(bContext *C, Main *bmain, wmWidgetGroupType *wgrouptype)
{
	wgrouptype->free(C, bmain);
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


void fix_linking_widgets(void)
{
	(void)0;
}

