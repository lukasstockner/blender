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
 * Contributor(s):
 *   Nicholas Rishel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __GHOST_TOUCHMANAGERWIN32_H__
#define __GHOST_TOUCHMANAGERWIN32_H__

#include "GHOST_TouchManager.h"

class GHOST_TouchManagerWin32 : public GHOST_TouchManager
{
public:
	GHOST_TouchManagerWin32(GHOST_System&);
};

#endif // __GHOST_TOUCHMANAGERWIN32_H__
