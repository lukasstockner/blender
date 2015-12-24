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

/** \file blender/windowmanager/widgets/wm_widgetgrouptype.cc
 *  \ingroup wm
 */

#include <stdlib.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_main.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "wm_event_system.h"

#include "wm_widgetmap.h"
#include "wm_widgetmaptype.h"
#include "wm_widgetgroup.h"
#include "wm_widgetgrouptype.h" // own include


wmWidgetGroupType::wmWidgetGroupType(
        wmWidgetMapType *wmaptype,
        int (*poll)(const bContext *, wmWidgetGroupType *),
        void (*create)(const bContext *, wmWidgetGroup *),
        wmKeyMap *(*keymap_init)(wmKeyConfig *, const char *),
        const Main *bmain,
        const char *mapidname_,
        const char *name_,
        const short spaceid,
        const short regionid,
        const bool is_3d)
    : poll(poll),
      create(create),
      keymap_init(keymap_init),
      spaceid(spaceid),
      regionid(regionid),
      is_3d(is_3d)
{
	idname[0] = '\0';
	BLI_strncpy(name, name_, MAX_NAME);
	BLI_strncpy(mapidname, mapidname_, MAX_NAME);

	/* add the type for future created areas of the same type  */
	BLI_addtail(&wmaptype->widgetgrouptypes, this);

	/* Main is missing on startup when we create new areas.
	 * So this is only called for widgets initialized on runtime */
	if (!bmain)
		return;


	/* init keymap - on startup there's an extra call to init keymaps for 'permanent' widget-groups */
	keymap_init_do(((wmWindowManager *)bmain->wm.first)->defaultconf);

	/* now create a widget for all existing areas */
	for (bScreen *sc = (bScreen *)bmain->screen.first; sc; sc = (bScreen *)sc->id.next) {
		for (ScrArea *sa = (ScrArea *)sc->areabase.first; sa; sa = sa->next) {
			for (SpaceLink *sl = (SpaceLink *)sa->spacedata.first; sl; sl = sl->next) {
				ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
				for (ARegion *ar = (ARegion *)lb->first; ar; ar = ar->next) {
					for (wmWidgetMap *wmap = (wmWidgetMap *)ar->widgetmaps.first; wmap; wmap = wmap->next) {
						if (wmap->type == wmaptype) {
							wmWidgetGroup *wgroup = new wmWidgetGroup;

							wgroup->type = this;
							wgroup->widgets = ListBase_NULL;

							/* just add here, drawing will occur on next update */
							BLI_addtail(&wmap->widgetgroups, wgroup);
							wmap->set_highlighted_widget(NULL, NULL, 0);
							ED_region_tag_redraw(ar);
						}
					}
				}
			}
		}
	}
}

#if 0 /* not needed yet */
wmWidgetGroupType::~wmWidgetGroupType()
{
	
}
#endif

void wmWidgetGroupType::unregister(bContext *C, Main *bmain)
{
	for (bScreen *sc = (bScreen *)bmain->screen.first; sc; sc = (bScreen *)sc->id.next) {
		for (ScrArea *sa = (ScrArea *)sc->areabase.first; sa; sa = sa->next) {
			for (SpaceLink *sl = (SpaceLink *)sa->spacedata.first; sl; sl = sl->next) {
				ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
				for (ARegion *ar = (ARegion *)lb->first; ar; ar = ar->next) {
					for (wmWidgetMap *wmap = (wmWidgetMap *)ar->widgetmaps.first; wmap; wmap = wmap->next) {
						wmWidgetGroup *wgroup, *wgroup_next;

						for (wgroup = (wmWidgetGroup *)wmap->widgetgroups.first; wgroup; wgroup = wgroup_next) {
							wgroup_next = wgroup->next;
							if (wgroup->type == this) {
								widgetgroup_remove(C, wmap, wgroup);
								ED_region_tag_redraw(ar);
							}
						}
					}
				}
			}
		}
	}


	wmWidgetMapType *wmaptype = WM_widgetmaptype_find(mapidname, spaceid,
	                                                  regionid, is_3d, false);
	BLI_remlink(&wmaptype->widgetgrouptypes, this);
}

void wmWidgetGroupType::keymap_init_do(wmKeyConfig *keyconf)
{
	keymap = keymap_init(keyconf, name);
}

void wmWidgetGroupType::attach_to_handler(bContext *C, wmEventHandler *handler, wmOperator *op_)
{
	/* now instantiate the widgetmap */
	op = op_;

	if (handler->op_region && !BLI_listbase_is_empty(&handler->op_region->widgetmaps)) {
		for (wmWidgetMap *wmap = (wmWidgetMap *)handler->op_region->widgetmaps.first; wmap; wmap = wmap->next) {
			wmWidgetMapType *wmaptype = wmap->type;

			if (wmaptype->spaceid == spaceid && wmaptype->regionid == regionid) {
				handler->widgetmap = wmap;
			}
		}
	}

	WM_event_add_mousemove(C);
}

void wmWidgetGroupType::set_idname(const char *idname_)
{
	strcpy(idname, idname_);
}

size_t wmWidgetGroupType::get_idname(char *r_idname)
{
	if (idname[0]) {
		strcpy(r_idname, idname);
		return sizeof(idname);
	}

	return 0;
}

wmOperator *wmWidgetGroupType::get_operator()
{
	return op;
}

wmKeyMap *wmWidgetGroupType::get_keymap()
{
	return keymap;
}

