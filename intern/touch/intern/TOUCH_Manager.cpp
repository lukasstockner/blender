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

/** \file touch/intern/TOUCH_Manager.cpp
 *  \ingroup TOUCH
 */

#include "TOUCH_Manager.h"

TOUCH_Manager::TOUCH_Manager()
{
	//TODO
}

TOUCH_Manager::~TOUCH_Manager()
{
	//TODO
}

void TOUCH_Manager::TOUCH_RegisterArea(STR_String context)
{
	char encoding = checkRegisteredArea(context);
	if(encoding) {
		TOUCH_area area = {context, encoding};
		registered_data.push_back(area);
	}
}

void TOUCH_Manager::TOUCH_RegisterRegion(STR_String context)
{
	char encoding = checkRegisteredRegion(context);
	if(encoding) {
		TOUCH_region region = {context, encoding};
		registered_data.push_back(region);
	}
}

void TOUCH_Manager::TOUCH_RegisterData(STR_String context)
{
	char encoding = checkRegisteredData(context);
	if(encoding) {
		TOUCH_data data = {context, encoding};
		registered_data.push_back(data);
	}
}

void TOUCH_Manager::TOUCH_AddTouchEvent(std::vector<TOUCH_event_info> event)
{
	for(int i = 0; i < event.size(); i++){
		/* if index 1 is touching down for the first time, clear the input string */
		if(event[i].state == TOUCH_DOWN) {
			if(event[i].index == 1) {
				input_string.Clear();
			}
			//touch_position_begin[i] = event[i].position; XXX
		}

		//touch_position_last[i] = event[i].position; XXX

		switch(event[i].state){
			case TOUCH_DOWN:
				input_string += 'd';
				break;
			case TOUCH_MOVE:
				input_string += 'm';
				break;
			case TOUCH_UP:
				input_string += 'u';
				break;
			default:
				input_string += '\0'; // XXX avoid null
				break;
		}

		input_string += event[i].index;

		input_string += checkRegisteredArea(event[i].area);
		input_string += checkRegisteredRegion(event[i].region);
		input_string += checkRegisteredData(event[i].data);
	}
}

char TOUCH_Manager::checkRegisteredArea(STR_String area)
{
	for(int i = 0; i < registered_area.size(); i++) {
		if(area == registered_area[i].context) {
			return registered_area[i].encoding;
		}
	}
	return '\0'; // XXX avoid null
}

char TOUCH_Manager::checkRegisteredRegion(STR_String region)
{
	for(int i = 0; i < registered_region.size(); i++) {
		if(region == registered_region[i].context) {
			return registered_region[i].encoding;
		}
	}
	return '\0'; // XXX avoid null
}

char TOUCH_Manager::checkRegisteredData(STR_String data)
{
	for(int i = 0; i < registered_data.size(); i++) {
		if(data == registered_data[i].context) {
			return registered_data[i].encoding;
		}
	}
	return '\0'; // XXX avoid null
}
