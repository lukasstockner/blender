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

/** \file blender/windowmanager/widgets/wm_widgetmaptype.cpp
 *  \ingroup wm
 */

#include <string.h>

#include "BLI_string.h"

#include "DNA_defs.h"

#include "wm_widgets_c_api.h" // tmp
#include "wm_widgetgrouptype.h"
#include "wm_widgetmaptype.h" // own include


/* store all widgetboxmaps here. Anyone who wants to register a widget for a certain
 * area type can query the widgetbox to do so */
static ListBase widgetmaptypes = ListBase_NULL;


wmWidgetMapType *WM_widgetmaptype_find(
        const char *idname, const int spaceid, const int regionid,
        const bool is_3d, const bool create)
{
	wmWidgetMapType *wmaptype;

	for (wmaptype = (wmWidgetMapType *)widgetmaptypes.first; wmaptype; wmaptype = wmaptype->next) {
		if (wmaptype->spaceid == spaceid &&
		    wmaptype->regionid == regionid &&
		    wmaptype->is_3d == is_3d &&
		    STREQ(wmaptype->idname, idname))
		{
			return wmaptype;
		}
	}

	if (!create) return NULL;

	wmaptype = new wmWidgetMapType;
	wmaptype->spaceid = spaceid;
	wmaptype->regionid = regionid;
	wmaptype->is_3d = is_3d;
	wmaptype->widgetgrouptypes = ListBase_NULL;
	BLI_strncpy(wmaptype->idname, idname, MAX_NAME);
	BLI_addhead(&widgetmaptypes, wmaptype);

	return wmaptype;
}

/**
 * Initialize keymaps for all existing widget-groups
 */
void wm_widgets_keymap(wmKeyConfig *keyconf)
{
	for (wmWidgetMapType *wmaptype = (wmWidgetMapType *)widgetmaptypes.first;
	     wmaptype;
	     wmaptype = wmaptype->next)
	{
		for (wmWidgetGroupType *wgrouptype = (wmWidgetGroupType *)wmaptype->widgetgrouptypes.first;
		     wgrouptype;
		     wgrouptype = wgrouptype->next)
		{
			wgrouptype->keymap_init_do(keyconf);
		}
	}
}

void WM_widgetmaptypes_free()
{
	wmWidgetMapType *wmaptype = (wmWidgetMapType *)widgetmaptypes.first;

	while (wmaptype) {
		wmWidgetMapType *wmaptype_next = wmaptype->next;
		wmWidgetGroupType *wgrouptype = (wmWidgetGroupType *)wmaptype->widgetgrouptypes.first;

		while (wgrouptype) {
			wmWidgetGroupType *wgrouptype_next = wgrouptype->next;

			delete wgrouptype;
			wgrouptype = wgrouptype_next;
		}

		delete wmaptype;
		wmaptype = wmaptype_next;
	}
}

