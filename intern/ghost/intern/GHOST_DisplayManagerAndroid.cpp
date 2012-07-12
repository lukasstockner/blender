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
 * Contributor(s): Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_DisplayManagerAndroid.cpp
 *  \ingroup GHOST
 */

#include "GHOST_SystemAndroid.h"
#include "GHOST_DisplayManagerAndroid.h"

#include "GHOST_WindowManager.h"

GHOST_DisplayManagerAndroid::GHOST_DisplayManagerAndroid(GHOST_SystemAndroid *system)
    :
	  GHOST_DisplayManager()
{
	//memset(&m_mode, 0, sizeof m_mode);
}

GHOST_TSuccess
GHOST_DisplayManagerAndroid::getNumDisplays(GHOST_TUns8& numDisplays) const
{

	//numDisplays =  1;
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_DisplayManagerAndroid::getNumDisplaySettings(GHOST_TUns8 display,
                                                              GHOST_TInt32& numSettings) const
{

	return GHOST_kFailure;
}


GHOST_TSuccess
GHOST_DisplayManagerAndroid::getDisplaySetting(GHOST_TUns8 display,
                                           GHOST_TInt32 index,
                                           GHOST_DisplaySetting& setting) const
{




	return GHOST_kFailure;
}

GHOST_TSuccess
GHOST_DisplayManagerAndroid::getCurrentDisplaySetting(GHOST_TUns8 display,
                                                  GHOST_DisplaySetting& setting) const
{

	return GHOST_kFailure;
}


GHOST_TSuccess
GHOST_DisplayManagerAndroid:: setCurrentDisplaySetting(GHOST_TUns8 display,
                                                   const GHOST_DisplaySetting& setting)
{
return GHOST_kFailure;
}
