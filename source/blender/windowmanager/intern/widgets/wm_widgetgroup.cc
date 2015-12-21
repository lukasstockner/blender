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

/** \file blender/windowmanager/intern/widgets/wm_widgetgroup.cpp
 *  \ingroup wm
 */

#include "BKE_report.h"

#include "BLI_listbase.h"

#include "DNA_defs.h"
#include "DNA_widget_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_types.h"
#include "BPY_extern.h"

#include "wm.h" // tmp

#include "wm_widget.h"
#include "wm_widgetmap.h"
#include "wm_widgetgroup.h" // own_include

#if 0
wmWidgetGroup::wmWidgetGroup()
{
	
}
#endif

void widgetgroup_remove(bContext *C, wmWidgetMap *wmap, wmWidgetGroup *wgroup)
{
	wmWidget *widget = (wmWidget *)wgroup->widgets.first;

	while (widget) {
		wmWidget *widget_next = widget->next;
		if (widget->flag & WM_WIDGET_HIGHLIGHT) {
			wmap->set_highlighted_widget(C, NULL, 0);
		}
		if (widget->flag & WM_WIDGET_ACTIVE) {
			wmap->set_active_widget(C, NULL, NULL);
		}

		widget_remove(&wgroup->widgets, widget);
		widget = widget_next;
	}

	BLI_remlink(&wmap->widgetgroups, wgroup);

#ifdef WITH_PYTHON
	if (wgroup->py_instance) {
		/* do this first in case there are any __del__ functions or
		 * similar that use properties */
		BPY_DECREF_RNA_INVALIDATE(wgroup->py_instance);
	}
#endif

	if (wgroup->reports && (wgroup->reports->flag & RPT_FREE)) {
		BKE_reports_clear(wgroup->reports);
		MEM_freeN(wgroup->reports);
	}

	delete wgroup;
}

