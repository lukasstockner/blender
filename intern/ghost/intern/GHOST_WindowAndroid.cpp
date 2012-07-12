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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_WindowAndroid.cpp
 *  \ingroup GHOST
 */
#include <EGL/egl.h>
#include <GLES/gl.h>
#include "GHOST_WindowAndroid.h"
#include <assert.h>
#include <aEvent.h>
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "blender", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "blender", __VA_ARGS__))

GHOST_WindowAndroid::GHOST_WindowAndroid(GHOST_SystemAndroid *system,
                                 const STR_String& title,
                                 GHOST_TInt32 left,
                                 GHOST_TInt32 top,
                                 GHOST_TUns32 width,
                                 GHOST_TUns32 height,
                                 GHOST_TWindowState state,
                                 const GHOST_TEmbedderWindowID parentWindow,
                                 GHOST_TDrawingContextType type,
                                 const bool stereoVisual,
                                 const GHOST_TUns16 numOfAASamples
                                 )
	:
	GHOST_Window(width, height, state, type, stereoVisual, numOfAASamples),
	m_invalid_window(false),
	m_system(system)
{
	
	winrect.m_t = 0;
	winrect.m_l = 0;
	winrect.m_r = 200;
	winrect.m_b = 200;





	glClearColor(0.0f, 0.0f, 1.0f, 0.5f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	aSwapBuffers();

    

	//fprintf(stderr, "Ignoring Xlib error: error code %d request code %d\n",
	//	theEvent->error_code, theEvent->request_code);

	setTitle(title);
}

bool GHOST_WindowAndroid::getValid() const
{
	return 1;
}


GHOST_WindowAndroid::~GHOST_WindowAndroid()
{

}


GHOST_TSuccess
GHOST_WindowAndroid::installDrawingContext(GHOST_TDrawingContextType type)
{

	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_WindowAndroid::invalidate(void)
{

	if (m_invalid_window == false) {
		m_system->addDirtyWindow(this);
		m_invalid_window = true;
		LOGW("Added to invalid");
	}
//LOGW("Added to invalid - done");
	return GHOST_kSuccess;
}

void
GHOST_WindowAndroid::
validate()
{
	m_invalid_window = false;
}

GHOST_TSuccess
GHOST_WindowAndroid::swapBuffers()
{
	aSwapBuffers();
	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_WindowAndroid::activateDrawingContext()
{

	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_WindowAndroid::removeDrawingContext()
{

	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_WindowAndroid::setState(GHOST_TWindowState state)
{


	return GHOST_kSuccess;
}


GHOST_TWindowState
GHOST_WindowAndroid::getState() const
{
	return GHOST_kWindowStateFullScreen;
}


void
GHOST_WindowAndroid::setTitle(const STR_String& title)
{

}


void
GHOST_WindowAndroid::getTitle(STR_String& title) const
{

}


void
GHOST_WindowAndroid::getWindowBounds(GHOST_Rect& bounds) const
{
	bounds.m_t = winrect.m_t;
	bounds.m_l = winrect.m_l;
	bounds.m_r = winrect.m_r;
	bounds.m_b = winrect.m_b;
}


void
GHOST_WindowAndroid::getClientBounds(GHOST_Rect& bounds) const
{
	bounds.m_t = winrect.m_t;
	bounds.m_l = winrect.m_l;
	bounds.m_r = winrect.m_r;
	bounds.m_b = winrect.m_b;

}

GHOST_TSuccess
GHOST_WindowAndroid::setClientWidth(GHOST_TUns32 width)
{

	return GHOST_kSuccess;
}

GHOST_TSuccess
GHOST_WindowAndroid::setClientHeight(GHOST_TUns32 height)
{
	return GHOST_kSuccess;
}

GHOST_TSuccess
GHOST_WindowAndroid::setClientSize(GHOST_TUns32 width,
		GHOST_TUns32 height)
{
	return GHOST_kSuccess;
}

void
GHOST_WindowAndroid::screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{

}
void
GHOST_WindowAndroid::clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{

}

void GHOST_WindowAndroid::storeWindowSize(GHOST_TInt32 x, GHOST_TInt32 y, GHOST_TInt32 width, GHOST_TInt32 height)
{

	winrect.m_r = width;
	winrect.m_b = height;
	winrect.m_l = x;
	winrect.m_t = y;

	LOGW("Win Pos %ix%i %ix%i", winrect.m_l, winrect.m_t, winrect.m_r, winrect.m_b);

}


GHOST_TSuccess
GHOST_WindowAndroid::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_WindowAndroid::setWindowCursorShape(GHOST_TStandardCursor shape)
{

	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_WindowAndroid::setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2],
                                            GHOST_TUns8 mask[16][2],
                                            int hotX,
                                            int hotY)
{
	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_WindowAndroid::setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
                                            GHOST_TUns8 *mask,
                                            int sizex, int sizey,
                                            int hotX, int hotY,
                                            int fg_color, int bg_color)
{

	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_WindowAndroid::setWindowCursorVisibility(bool visible)
{
	return GHOST_kSuccess;
}
