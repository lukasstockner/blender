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
#include <vector>
#include "STR_String.h"

#ifdef INPUT_TOUCH_DEBUG
#include <stdio.h>
#endif

struct TOUCH_Context
{
	TOUCH_Context();
	STR_String external_id;
	char internal_encoding;
};

class TOUCH_Manager
{
public:
	/**
	 * Constructor
	 */
	TOUCH_Manager();

	/**
	 * Destructor
	 */
	~TOUCH_Manager();

	void RegisterContext(std::vector<TOUCH_Context> * context_type, const char * context_id);

	void AddTouchEvent(std::vector<TOUCH_event_info> event);

	static void CreateManager();
	static void DestroyManager();
	static TOUCH_Manager * GetManager();

private:
	char checkRegisteredContext(std::vector<TOUCH_Context> * context_type, const char * context_id);

	STR_String input_string;
	//std::vector<TOUCH_position> touch_position_begin; XXX
	//std::vector<TOUCH_position> touch_position_last; XXX

	std::vector<TOUCH_Context> registered_area; //pass with &registered_area
	std::vector<TOUCH_Context> registered_region;
	std::vector<TOUCH_Context> registered_data;

	static TOUCH_Manager * manager;

};

#endif /* __TOUCH_TOUCH_H__ */
