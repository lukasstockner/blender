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

/** \file touch/intern/TOUCH_API.cpp
 *  \ingroup TOUCH
 */

#include "TOUCH_API.h"
#include "TOUCH_Manager.h"

extern TOUCH_Handle TOUCH_InitManager()
{
	TOUCH_Manager::CreateManager();
	TOUCH_Manager * manager = TOUCH_Manager::GetManager();

	return (TOUCH_Handle)manager;
}

extern void TOUCH_DestroyManager(TOUCH_Handle* handle) {
	delete handle;
}

extern void TOUCH_RegisterArea(TOUCH_Handle* handle, const char * context)
{
	//TODO
}

extern void TOUCH_RegisterRegion(TOUCH_Handle* handle, const char * context)
{
	//TODO
}

extern void TOUCH_RegisterData(TOUCH_Handle* handle, const char * context)
{
	//TODO
}

extern void TOUCH_AddTouchEvent(TOUCH_Handle* handle, void * event)
{
	std::vector<TOUCH_event_info> * event_vector = (std::vector<TOUCH_event_info> *) event;
	TOUCH_Manager * manager = (TOUCH_Manager *) handle;
	manager->AddTouchEvent(* event_vector);
}
