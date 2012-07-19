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

/** \file ghost/intern/GHOST_WindowAndroid.h
 *  \ingroup GHOST
 * Declaration of GHOST_WindowAndroid class.
 */

#ifndef __GHOST_WINDOWANDROID_H__
#define __GHOST_WINDOWANDROID_H__

#include "GHOST_Window.h"
#include "GHOST_SystemAndroid.h"



class STR_String;

class GHOST_WindowAndroid : public GHOST_Window
{
private:
	/** Used to concatenate calls to invalidate() on this window. */
	bool m_invalid_window;

	/// A pointer to the typed system class.

	GHOST_SystemAndroid *m_system;
public:

	const GHOST_TabletData *GetTabletData() {
		return NULL;
	}

	GHOST_WindowAndroid(GHOST_SystemAndroid *system,
	                const STR_String& title,
	                GHOST_TInt32 left, GHOST_TInt32 top,
	                GHOST_TUns32 width, GHOST_TUns32 height,
	                GHOST_TWindowState state,
	                const GHOST_TEmbedderWindowID parentWindow,
	                GHOST_TDrawingContextType type,
	                const bool stereoVisual,
	                const GHOST_TUns16 numOfAASamples
	                );

	~GHOST_WindowAndroid();



	GHOST_TSuccess invalidate(void);
	void 	validate();

	/**
	 * called by the X11 system implementation when expose events
	 * for the window have been pushed onto the GHOST queue
	 */

	bool getValid() const;

	void getWindowBounds(GHOST_Rect& bounds) const;
	void getClientBounds(GHOST_Rect& bounds) const;


	void storeWindowSize(GHOST_TInt32 x, GHOST_TInt32 y, GHOST_TInt32 width, GHOST_TInt32 height);
protected:
	GHOST_TSuccess installDrawingContext(GHOST_TDrawingContextType type);
	GHOST_TSuccess removeDrawingContext();

	GHOST_TSuccess
	setWindowCursorGrab(GHOST_TGrabCursorMode mode);

	GHOST_TSuccess
	setWindowCursorShape(GHOST_TStandardCursor shape);

	GHOST_TSuccess
	setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2],
	                           GHOST_TUns8 mask[16][2],
	                           int hotX, int hotY);

	GHOST_TSuccess
	setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
	                           GHOST_TUns8 *mask,
	                           int sizex, int sizey,
	                           int hotX, int hotY,
	                           int fg_color, int bg_color);

	GHOST_TSuccess
	setWindowCursorVisibility(bool visible);

	void
	setTitle(const STR_String& title);

	void
	getTitle(STR_String& title) const;

	GHOST_TSuccess
	setClientWidth(GHOST_TUns32 width);

	GHOST_TSuccess
	setClientHeight(GHOST_TUns32 height);

	GHOST_TSuccess
	setClientSize(GHOST_TUns32 width,
	              GHOST_TUns32 height);

	void
	screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY,
	               GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	void
	clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY,
	               GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	GHOST_TSuccess
	swapBuffers();

	GHOST_TSuccess
	activateDrawingContext();

	GHOST_TSuccess
	setState(GHOST_TWindowState state);

	GHOST_TWindowState
	getState() const;

	GHOST_TSuccess setOrder(GHOST_TWindowOrder order)
	{
		// TODO
		return GHOST_kSuccess;
	}



	GHOST_Rect winrect;

};


#endif // __GHOST_WINDOWANDROID_H__
