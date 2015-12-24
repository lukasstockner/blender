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

/** \file blender/windowmanager/widgets/wm_widget.h
 *  \ingroup wm
 */

#ifndef __WM_WIDGET_H__
#define __WM_WIDGET_H__

#if 0
class wmWidget
{
public:
	wmWidget();
};
#endif

void widget_remove(ListBase *widgetlist, wmWidget *widget);
void widget_data_free(wmWidget *widget);
void widget_find_active_3D_loop(const bContext *C, ListBase *visible_widgets);
void widget_calculate_scale(wmWidget *widget, const bContext *C);
bool widget_compare(const wmWidget *a, const wmWidget *b);

#endif // __WM_WIDGET_H__
