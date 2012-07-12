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

/** \file ghost/intern/GHOST_SystemPathsAndroid.cpp
 *  \ingroup GHOST
 */


#include "GHOST_SystemPathsAndroid.h"



GHOST_SystemPathsAndroid::GHOST_SystemPathsAndroid()
{
}

GHOST_SystemPathsAndroid::~GHOST_SystemPathsAndroid()
{
}

const GHOST_TUns8 *GHOST_SystemPathsAndroid::getSystemDir(int, const char *versionstr) const
{
	return (GHOST_TUns8 *)"";
}

const GHOST_TUns8 *GHOST_SystemPathsAndroid::getUserDir(int, const char *versionstr) const
{
	return (GHOST_TUns8 *)"";
}

const GHOST_TUns8 *GHOST_SystemPathsAndroid::getBinaryDir() const
{
	return (GHOST_TUns8 *)"";
}

void GHOST_SystemPathsAndroid::addToSystemRecentFiles(const char *filename) const
{
	
}
