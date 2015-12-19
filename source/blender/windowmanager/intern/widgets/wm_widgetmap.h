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

/** \file blender/windowmanager/intern/widgets/wm_widgetmap.h
 *  \ingroup wm
 */

#ifndef __WM_WIDGETMAP_H__
#define __WM_WIDGETMAP_H__

#include "BLI_listbase.h"

struct wmWidget;
struct wmWidgetGroup;
struct wmWidgetMapType;


class wmWidgetMap
{
public:
	wmWidgetMap();
	struct wmWidgetMap *next, *prev;

	wmWidgetMapType *type;
	ListBase widgetgroups;

	void widgetmap_from_type(
	        wmWidgetMap *wmap, const char *idname,
	        const int spaceid, const int regionid,
	        const bool is_3d);
	void set_highlighted_widget(struct bContext *C, wmWidget *widget, unsigned char part);
	wmWidget *find_highlighted_widget(bContext *C, const struct wmEvent *event, unsigned char *part);

	/**
	 * \brief Widget map runtime context
	 *
	 * Contains information about this widget map. Currently
	 * highlighted widget, currently selected widgets, ...
	 */
	struct {
		/* we redraw the widgetmap when this changes */
		wmWidget *highlighted_widget;
		/* user has clicked this widget and it gets all input */
		wmWidget *active_widget;
		/* array for all selected widgets
		 * TODO  check on using BLI_array */
		wmWidget **selected_widgets;
		int tot_selected;

		/* set while widget is highlighted/active */
		wmWidgetGroup *activegroup;
	} wmap_context;
};

#endif // __WM_WIDGETMAP_H__
