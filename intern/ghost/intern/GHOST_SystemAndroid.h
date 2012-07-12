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

/** \file ghost/intern/GHOST_SystemAndroid.h
 *  \ingroup GHOST
 * Declaration of GHOST_SystemAndroid class.
 */

#ifndef __GHOST_SYSTEMANDROID_H__
#define __GHOST_SYSTEMANDROID_H__

#include "GHOST_System.h"
#include "GHOST_DisplayManagerAndroid.h"
#include "GHOST_TimerManager.h"
#include "GHOST_WindowAndroid.h"
#include "GHOST_Event.h"

#include <aEvent.h>

class GHOST_WindowAndroid;


class GHOST_SystemAndroid : public GHOST_System {
public:

	void addDirtyWindow(GHOST_WindowAndroid *bad_wind);

	GHOST_SystemAndroid();
	~GHOST_SystemAndroid();

	bool
	processEvents(bool waitForEvent);

	int
	toggleConsole(int action) { return 0; }

	GHOST_TSuccess
	getModifierKeys(GHOST_ModifierKeys& keys) const;

	GHOST_TSuccess
	getButtons(GHOST_Buttons& buttons) const;

	GHOST_TUns8 *
	getClipboard(bool selection) const;

	void
	putClipboard(GHOST_TInt8 *buffer, bool selection) const;

	GHOST_TUns64
	getMilliSeconds();

	GHOST_TUns8
	getNumDisplays() const;

	GHOST_TSuccess
	getCursorPosition(GHOST_TInt32& x,
	                  GHOST_TInt32& y) const;

	GHOST_TSuccess
	setCursorPosition(GHOST_TInt32 x,
	                  GHOST_TInt32 y);

	void
	getMainDisplayDimensions(GHOST_TUns32& width,
	                         GHOST_TUns32& height) const;

private:

	/// The vector of windows that need to be updated.
	std::vector<GHOST_WindowAndroid *> m_dirty_windows;

	GHOST_TSuccess
	init();

	GHOST_IWindow *
	createWindow(const STR_String& title,
	             GHOST_TInt32 left,
	             GHOST_TInt32 top,
	             GHOST_TUns32 width,
	             GHOST_TUns32 height,
	             GHOST_TWindowState state,
	             GHOST_TDrawingContextType type,
	             bool stereoVisual,
	             const GHOST_TUns16 numOfAASamples,
	             const GHOST_TEmbedderWindowID parentWindow
	             );

	

	bool
	generateWindowExposeEvents();

	/// Start time at initialization.
	GHOST_TUns64 m_start_time;

	void processEvent(eEventAllTypes *ae);

	GHOST_WindowAndroid *mainwindow;
};

#endif
