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

/** \file touch/TOUCH_Types.h
 *  \ingroup TOUCH
 */

#ifndef __TOUCH_TYPES_H__
#define __TOUCH_TYPES_H__

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

#if defined(WITH_CXX_GUARDEDALLOC) && defined(__cplusplus)
#  define TOUCH_DECLARE_HANDLE(name) typedef struct name##__ { int unused; MEM_CXX_CLASS_ALLOC_FUNCS(#name) } *name
#else
#  define TOUCH_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name
#endif

typedef enum TOUCH_state {
	TOUCH_DOWN = 0,
	TOUCH_MOVE = 1,
	TOUCH_UP = 2
} TOUCH_state;

typedef struct TOUCH_event_base {
	char index;
	int position_x;
	int position_y;
	TOUCH_state state;
} TOUCH_event_base;

#endif /* __TOUCH_TYPES_H__ */
