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

/** \file blender/windowmanager/intern/widgets/wm_widget.h
 *  \ingroup wm
 */

#ifndef __WM_WIDGET_H__
#define __WM_WIDGET_H__


// Workaround until we can remove widgetflags from WM_api.h. We could also
// simply include it, but would like to avoid that for various reasons.
#ifndef __WM_API_H__

/* wmWidget->flag */
enum widgetflags {
	/* states */
	WM_WIDGET_HIGHLIGHT   = (1 << 0),
	WM_WIDGET_ACTIVE      = (1 << 1),
	WM_WIDGET_SELECTED    = (1 << 2),
	/* settings */
	WM_WIDGET_DRAW_HOVER  = (1 << 3),
	WM_WIDGET_DRAW_ACTIVE = (1 << 4), /* draw while dragging */
	WM_WIDGET_SCALE_3D    = (1 << 5),
	WM_WIDGET_SCENE_DEPTH = (1 << 6), /* widget is depth culled with scene objects*/
	WM_WIDGET_HIDDEN      = (1 << 7),
	WM_WIDGET_SELECTABLE  = (1 << 8),
};

#endif // __WM_API_H__

#if 0
class wmWidget
{
public:
	wmWidget();
};
#endif

void widget_delete(ListBase *widgetlist, wmWidget *widget);
void widget_data_free(wmWidget *widget);
void widget_find_active_3D_loop(const bContext *C, ListBase *visible_widgets);
void widget_calculate_scale(wmWidget *widget, const bContext *C);
bool widget_compare(const wmWidget *a, const wmWidget *b);

#endif // __WM_WIDGET_H__
