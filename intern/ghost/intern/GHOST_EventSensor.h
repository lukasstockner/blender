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
 * Contributor(s): Blender Foundation
 *                 Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_EventSensor.h
 *  \ingroup GHOST
 * Declaration of GHOST_EventSensor class.
 */

#ifndef __GHOST_EVENTSENSOR_H__
#define __GHOST_EVENTSENSOR_H__

#include "GHOST_Event.h"
#include "GHOST_Types.h"

/**
 * Hardware sensor event.
 * Like Acclerometer and gyroscope.
 * Acceleromoter and gyroscope have 3D vector
 * While others have  1D vectors (and data for other fields is not specified)
 */
class GHOST_EventSensor : public GHOST_Event
{
public:
	GHOST_EventSensor(GHOST_TUns64 msec, GHOST_IWindow *window, GHOST_TSensorTypes subtype, float *v)
		: GHOST_Event(msec, GHOST_kEventSensor, window)
	{
		m_SensorEventData.type = subtype;

		m_SensorEventData.dv[0] = v[0];
		m_SensorEventData.dv[1] = v[1];
		m_SensorEventData.dv[2] = v[2];

		m_data = &m_SensorEventData;
	}


	GHOST_EventSensor(GHOST_TUns64 msec, GHOST_IWindow *window, GHOST_TSensorTypes subtype, float v)
		: GHOST_Event(msec, GHOST_kEventSensor, window)
	{
		m_SensorEventData.type = subtype;

		m_SensorEventData.dv[0] = v;

		m_data = &m_SensorEventData;
	}


	GHOST_EventSensor(GHOST_TUns64 msec, GHOST_IWindow *window, GHOST_TSensorTypes subtype, float v1, float v2, float v3)
		: GHOST_Event(msec, GHOST_kEventSensor, window)
	{
		m_SensorEventData.type = subtype;

		m_SensorEventData.dv[0] = v1;
		m_SensorEventData.dv[1] = v2;
		m_SensorEventData.dv[2] = v3;

		m_data = &m_SensorEventData;
	}


protected:

	GHOST_TEventSensorData m_SensorEventData;
};


#endif // __GHOST_EVENTSENSOR_H__

