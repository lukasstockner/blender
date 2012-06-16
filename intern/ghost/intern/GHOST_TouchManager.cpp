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

#include "GHOST_TouchManager.h"
#include "GHOST_EventTouch.h"
#include "GHOST_WindowManager.h"

GHOST_TouchManager::GHOST_TouchManager(GHOST_System& sys)
	: m_system(sys)
{
}

void GHOST_TouchManager::sendTouchEvent(GHOST_TUns8 index, GHOST_TProgress state, GHOST_TInt32 x,
                                        GHOST_TInt32 y, GHOST_TUns64 time)
{
	GHOST_IWindow *window = m_system.getWindowManager()->getActiveWindow();

	if (window == NULL) {
		return;
	}

	GHOST_EventTouch *event = new GHOST_EventTouch(time, window);
	GHOST_TEventTouchData *data = (GHOST_TEventTouchData *) event->getData();

	data->index = index;
	data->state = state;
	data->x = x;
	data->y = y;

	m_system.pushEvent(event);
}
