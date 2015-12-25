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

/** \file blender/windowmanager/widgets/widget_library/widget_library.h
 *  \ingroup wm
 *
 * \name Widget Library API
 */

#ifndef __WIDGET_LIBRARY_H__
#define __WIDGET_LIBRARY_H__

struct wmWidgetGroup;
struct wmWidget;


struct wmWidget *WIDGET_arrow_new(struct wmWidgetGroup *wgroup, const char *name, const int style);
void WIDGET_arrow_set_direction(struct wmWidget *widget, const float direction[3]);
void WIDGET_arrow_set_up_vector(struct wmWidget *widget, const float direction[3]);
void WIDGET_arrow_set_line_len(struct wmWidget *widget, const float len);
void WIDGET_arrow_set_ui_range(struct wmWidget *widget, const float min, const float max);
void WIDGET_arrow_set_range_fac(struct wmWidget *widget, const float range_fac);
void WIDGET_arrow_cone_set_aspect(struct wmWidget *widget, const float aspect[2]);

struct wmWidget *WIDGET_dial_new(struct wmWidgetGroup *wgroup, const char *name, const int style);
void WIDGET_dial_set_up_vector(struct wmWidget *widget, const float direction[3]);

struct wmWidget *WIDGET_plane_new(struct wmWidgetGroup *wgroup, const char *name, const int style);
void WIDGET_plane_set_direction(struct wmWidget *widget, const float direction[3]);
void WIDGET_plane_set_up_vector(struct wmWidget *widget, const float direction[3]);

struct wmWidget *WIDGET_rect_transform_new(
        struct wmWidgetGroup *wgroup, const char *name, const int style,
        const float width, const float height);

struct wmWidget *WIDGET_facemap_new(
        struct wmWidgetGroup *wgroup, const char *name, const int style,
        struct Object *ob, const int facemap);
struct bFaceMap *WIDGET_facemap_get_fmap(struct wmWidget *widget);

#endif // __WIDGET_LIBRARY_H__

