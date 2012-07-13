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

/** \file touch/TOUCH_Manager.h
 *  \ingroup TOUCH
 */

#ifndef __TOUCH_TOUCH_H__
#define __TOUCH_TOUCH_H__

#include "TOUCH_Types.h"

class TOUCH_Manager
{
public:
	/**
	 * Constructor.
	 */
	TOUCH_Manager();

	/**
	 * Destructor.
	 */
	~TOUCH_Manager();

	void TOUCH_RegisterArea(STR_String context);
	void TOUCH_RegisterRegion(STR_String context);
	void TOUCH_RegisterData(STR_String context);

	void TOUCH_AddTouchEvent(TOUCH_event_info event);

private:
	char checkRegisteredArea(STR_String area);
	char checkRegisteredRegion(STR_String region);
	char checkRegisteredData(STR_String data);

};

#endif /* __TOUCH_TOUCH_H__ */
