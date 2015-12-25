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
 * Contributor(s): Blender Foundation, Julian Eisel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/widgets/widget_library/widget_geometry.h
 *  \ingroup wm
 *
 * \name Widget Geometry
 *
 * \brief Prototypes for arrays defining the widget geometry. The actual definitions can be found in files usually
 *        called geom_xxx_widget.cc
 */

#ifndef __WIDGET_GEOMETRY_H__
#define __WIDGET_GEOMETRY_H__

/* arrow widget */
extern int _WIDGET_nverts_arrow;
extern int _WIDGET_ntris_arrow;

extern float _WIDGET_verts_arrow[][3];
extern float _WIDGET_normals_arrow[][3];
extern unsigned short _WIDGET_indices_arrow[];


/* cube widget */
extern int _WIDGET_nverts_cube;
extern int _WIDGET_ntris_cube;

extern float _WIDGET_verts_cube[][3];
extern float _WIDGET_normals_cube[][3];
extern unsigned short _WIDGET_indices_cube[];


/* dial widget */
extern int _WIDGET_nverts_dial;
extern int _WIDGET_ntris_dial;

extern float _WIDGET_verts_dial[][3];
extern float _WIDGET_normals_dial[][3];
extern unsigned short _WIDGET_indices_dial[];

#endif // __WIDGET_GEOMETRY_H__
