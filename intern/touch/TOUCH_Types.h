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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Nicholas Rishel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file touch/touch_types.h
 *  \ingroup TOUCH
 */

#ifndef __TOUCH_TYPES_H__
#define __TOUCH_TYPES_H__

#include "STR_String.h"

typedef enum TOUCH_state {
	TOUCH_DOWN = 0,
	TOUCH_MOVE = 1,
	TOUCH_UP = 2
} TOUCH_state;

typedef struct TOUCH_registered_context {
	TOUCH_registered_context *prev, *next;
	STR_String context;
	char encoding;
} TOUCH_registered_context;

typedef struct TOUCH_registered_context TOUCH_area;
typedef struct TOUCH_registered_context TOUCH_region;
typedef struct TOUCH_registered_context TOUCH_data;

typedef struct TOUCH_context_full {
	TOUCH_area area;
	TOUCH_region region;
	TOUCH_data data;
} TOUCH_context;

typedef struct TOUCH_event_info {
	TOUCH_event_info *prev, *next;

	int position_x, position_y;
	char id;
	TOUCH_state state;

	STR_String area, region, data;
} TOUCH_event_info;

#endif /* TOUCH_TYPES_H */
