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

/** \file blender/windowmanager/widgets/wm_widgetmaptype.h
 *  \ingroup wm
 *
 * \name wmWidgetMapType
 *
 * This is a container for all widget types that can be instantiated in a region.
 * (similar to dropboxes).
 *
 * \note There is only ever one of these for every (area, region) combination.
 */

#ifndef __WM_WIDGETMAPTYPE_H__
#define __WM_WIDGETMAPTYPE_H__

#include "BLI_listbase.h"


/* struct for now */
typedef struct wmWidgetMapType {
	wmWidgetMapType *next, *prev;
	char idname[64];
	short spaceid, regionid;
	/**
	 * Check if widgetmap does 3D drawing
	 * (uses a different kind of interaction),
	 * - 3d: use glSelect buffer.
	 * - 2d: use simple cursor position intersection test. */
	bool is_3d;
	/* types of widgetgroups for this widgetmap type */
	ListBase widgetgrouptypes;
} wmWidgetMapType;

#endif // __WM_WIDGETMAPTYPE_H__
