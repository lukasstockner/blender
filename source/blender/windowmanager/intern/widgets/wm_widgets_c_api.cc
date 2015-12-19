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

#include "wm_widgetmap.h"
#include "wm_widgetmaptype.h"
#include "wm_widgetgrouptype.h"
#include "wm_widgets_c_api.h" // own include


wmWidgetMap *WM_widgetmap_from_type(
        const char *idname, const int spaceid, const int regionid,
        const bool is_3d)
{
	wmWidgetMap *wmap = new wmWidgetMap;
	wmap->widgetmap_from_type(wmap, idname, spaceid, regionid, is_3d);
	return wmap;
}

wmWidgetGroupType *WM_widgetgrouptype_new(
        int (*poll)(const bContext *, wmWidgetGroupType *),
        void (*create)(const bContext *, wmWidgetGroup *),
        wmKeyMap *(*keymap_init)(wmKeyConfig *, const char *),
        const Main *bmain, const char *mapidname, const char *name,
        const short spaceid, const short regionid, const bool is_3d)
{
	wmWidgetMapType *wmaptype = WM_widgetmaptype_find(mapidname, spaceid, regionid, is_3d, false);

	if (!wmaptype) {
		fprintf(stderr, "widgetgrouptype creation: widgetmap type does not exist");
		return NULL;
	}


	wmWidgetGroupType *wgrouptype = new wmWidgetGroupType;

	/* add the type for future created areas of the same type  */
	BLI_addtail(&wmaptype->widgetgrouptypes, wgrouptype);

	wgrouptype->init(wmaptype, wgrouptype,
	                                    poll, create, keymap_init,
	                                    bmain, mapidname, name,
	                                    spaceid, regionid, is_3d);

	return wgrouptype;
}

void WM_widgetgrouptype_unregister(bContext *C, Main *bmain, wmWidgetGroupType *wgrouptype)
{
	wgrouptype->unregister(C, bmain);
}

void WM_widgetgrouptype_attach_to_handler(
        bContext *C, wmWidgetGroupType *wgrouptype,
        wmEventHandler *handler, wmOperator *op)
{
	wgrouptype->attach_to_handler(C, handler, op);
}

/**
 * \returns sizeof(wgrouptype->idname).
 */
size_t WM_widgetgrouptype_idname_get(wmWidgetGroupType *wgrouptype, char *r_idname)
{
	return wgrouptype->get_idname(r_idname);
}

void fix_linking_widgets(void)
{
	(void)0;
}

