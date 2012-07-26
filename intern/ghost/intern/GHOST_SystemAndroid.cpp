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

/** \file ghost/intern/GHOST_SystemAndroid.cpp
 *  \ingroup GHOST
 */

#include <assert.h>

#include "GHOST_SystemAndroid.h"

#include "GHOST_WindowManager.h"

#include "GHOST_EventCursor.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventWheel.h"
#include "GHOST_EventSensor.h"

#include <sys/time.h>

#include <unistd.h>
#include <GLES/gl.h>
#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "blender", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "blender", __VA_ARGS__))


GHOST_SystemAndroid::GHOST_SystemAndroid()
    :
	  GHOST_System(),
	  mainwindow(NULL)
{

	// compute the initial time
	timeval tv;
	if (gettimeofday(&tv, NULL) == -1) {
		GHOST_ASSERT(false, "Could not instantiate timer!");
	}

	// Taking care not to overflow the tv.tv_sec*1000
	m_start_time = GHOST_TUns64(tv.tv_sec) * 1000 + tv.tv_usec / 1000;

}

GHOST_SystemAndroid::~GHOST_SystemAndroid()
{

}

GHOST_IWindow *
GHOST_SystemAndroid::createWindow(const STR_String& title,
                              GHOST_TInt32 left,
                              GHOST_TInt32 top,
                              GHOST_TUns32 width,
                              GHOST_TUns32 height,
                              GHOST_TWindowState state,
                              GHOST_TDrawingContextType type,
                              bool stereoVisual,
                              const GHOST_TUns16 numOfAASamples,
                              const GHOST_TEmbedderWindowID parentWindow
                              )
{
	GHOST_WindowAndroid *window = NULL;

	LOGI("New window is created");
	mainwindow = window = new GHOST_WindowAndroid(this, title, left, top, width, height, state, parentWindow, type, stereoVisual, 1);

	if (window) {
		if (window->getValid()) {
			m_windowManager->addWindow(window);
			pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
		}

	}
	return window;
}

GHOST_TSuccess
GHOST_SystemAndroid::init() {
	GHOST_TSuccess success = GHOST_System::init();

	if (success) {
		m_displayManager = new GHOST_DisplayManagerAndroid(this);

		if (m_displayManager) {
			return GHOST_kSuccess;
		}
	}

	return GHOST_kFailure;
}

void
GHOST_SystemAndroid::getMainDisplayDimensions(GHOST_TUns32& width,
                                          GHOST_TUns32& height) const
{

	width = 200;
	height = 200;
	// Almost all android have this resolution, so we can hardcode it :)
	//change
}

GHOST_TUns8
GHOST_SystemAndroid::getNumDisplays() const
{
	return 1;
}

GHOST_TSuccess
GHOST_SystemAndroid::getModifierKeys(GHOST_ModifierKeys& keys) const
{
	int mod = 0;

/*	keys.set(GHOST_kModifierKeyLeftShift,    (mod & KMOD_LSHIFT) != 0);
	keys.set(GHOST_kModifierKeyRightShift,   (mod & KMOD_RSHIFT) != 0);
	keys.set(GHOST_kModifierKeyLeftControl,  (mod & KMOD_LCTRL) != 0);
	keys.set(GHOST_kModifierKeyRightControl, (mod & KMOD_RCTRL) != 0);
	keys.set(GHOST_kModifierKeyLeftAlt,      (mod & KMOD_LALT) != 0);
	keys.set(GHOST_kModifierKeyRightAlt,     (mod & KMOD_RALT) != 0);
	keys.set(GHOST_kModifierKeyOS,           (mod & (KMOD_LGUI | KMOD_RGUI)) != 0);
*/
	return GHOST_kSuccess;
}






GHOST_TSuccess
GHOST_SystemAndroid::getCursorPosition(GHOST_TInt32& x,
                                   GHOST_TInt32& y) const
{

	LOGW("Get Cursor");
	return GHOST_kFailure;
}

GHOST_TSuccess
GHOST_SystemAndroid::setCursorPosition(GHOST_TInt32 x,
                                   GHOST_TInt32 y)
{
	
	return GHOST_kSuccess;
}

void GHOST_SystemAndroid::processEvent(eEventAllTypes *ae)
{
	GHOST_WindowAndroid *window = mainwindow;
	GHOST_Event *g_event = NULL;

	if (!window) {

		return;



	}
		switch (ae->eb.aeventype) {
			case ET_APP:
			{
				switch(ae->app.action)
				{
					case ET_APP_CLOSE:
						LOGW("Close event");

					g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowClose, window);
					break;
				}
				break;
			}

			case ET_WINDOW:
			{
				switch(ae->Window.type)
				{
					case ET_WS_FOCUS:
						LOGW("Update");
						g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowActivate, window);
					break;
					case ET_WS_DEFOCUS:
						LOGW("Update");
						g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowDeactivate, window);
					break;
					case ET_WS_UPDATE:
						LOGW("Update");
						g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, window);
					break;
				}

				break;
			}
			case ET_WINDOWSIZE:
			{

			window->storeWindowSize(ae->WindowSize.pos[0], ae->WindowSize.pos[1],
									ae->WindowSize.size[0], ae->WindowSize.size[1]
									);
				g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window);

				break;
			}

			case ET_MOUSE:
			{
				if(ae->Mouse.mouseevent == 2)
				{
					g_event = new
							  GHOST_EventCursor(
						getMilliSeconds(),
						GHOST_kEventCursorMove,
						window,
						ae->Mouse.coord[0],
						ae->Mouse.coord[1]
						);


					LOGW(" Cursor %i x %i", (int)ae->Mouse.coord[0], (int)ae->Mouse.coord[1]);

				}
				else if(ae->Mouse.mouseevent < 2)
				{
					GHOST_TEventType type = ae->Mouse.mouseevent ? GHOST_kEventButtonUp : GHOST_kEventButtonDown;


					g_event = new GHOST_EventButton(
								getMilliSeconds(),
								type,
								window,
								GHOST_kButtonMaskLeft
								);

				}
				break;
			}
			case ET_SENSOR:
			{
				g_event = new  GHOST_EventSensor(getMilliSeconds(), window, (GHOST_TSensorTypes)ae->Sensor.type, ae->Sensor.sv);
				break;
			}


			default: {
				//pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, window));
				LOGW("Unknown type %i", ae->eb.aeventype);
				break;
			}
		}

	if (g_event) {
		pushEvent(g_event);
	}
}

void
GHOST_SystemAndroid::
addDirtyWindow(
		GHOST_WindowAndroid *bad_wind)
{
	GHOST_ASSERT((bad_wind != NULL), "addDirtyWindow() NULL ptr trapped (window)");

	m_dirty_windows.push_back(bad_wind);
}

bool
GHOST_SystemAndroid::generateWindowExposeEvents()
{
	std::vector<GHOST_WindowAndroid *>::iterator w_start = m_dirty_windows.begin();
	std::vector<GHOST_WindowAndroid *>::const_iterator w_end = m_dirty_windows.end();
	bool anyProcessed = false;

	for (; w_start != w_end; ++w_start) {
		GHOST_Event *g_event = new
							   GHOST_Event(
								   getMilliSeconds(),
								   GHOST_kEventWindowUpdate,
								   *w_start
								   );

		(*w_start)->validate();

		if (g_event) {
			pushEvent(g_event);
			anyProcessed = true;
		}
	}

	m_dirty_windows.clear();
	return anyProcessed;
}


bool
GHOST_SystemAndroid::processEvents(bool waitForEvent)
{
	// Get all the current events -- translate them into
	// ghost events and call base class pushEvent() method.

	bool anyProcessed = false;

	do {
		GHOST_TimerManager *timerMgr = getTimerManager();

		//LOGW("Processing");

		if (waitForEvent && m_dirty_windows.empty() && !::aEventGQueueCheck()) {
			::usleep(1*1000);

		}

		if (timerMgr->fireTimers(getMilliSeconds())) {
			anyProcessed = true;
		}
			anyProcessed = true;
		//glClearColor(0.0f, 1.0f, 1.0f, 0.5f);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//aSwapBuffers();
		// Process all the events waiting for us
		eEventAllTypes event;
		while (aEventGQueueRead(&event)) {

			//DoEvent
			processEvent(&event);
					//pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, mainwindow));

			anyProcessed = true;
		}

		if (generateWindowExposeEvents()) {
			anyProcessed = true;
		}



	} while (waitForEvent && !anyProcessed);

	return anyProcessed;
}



GHOST_TSuccess GHOST_SystemAndroid::getSensorsAvailability(GHOST_TSensorTypes type)
{
	return aGetSensorsAvailability(type) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_SystemAndroid::setSensorsState(GHOST_TSensorTypes type, int enable)
{
	return aSetSensorsState(type, enable) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_SystemAndroid::getButtons(GHOST_Buttons& buttons) const
{


	return GHOST_kFailure;
}

GHOST_TUns8 *
GHOST_SystemAndroid::getClipboard(bool selection) const
{
	return (GHOST_TUns8 *)"";
}

void
GHOST_SystemAndroid::putClipboard(GHOST_TInt8 *buffer, bool selection) const
{

}

GHOST_TUns64
GHOST_SystemAndroid::getMilliSeconds()
{
	timeval tv;
	if (gettimeofday(&tv, NULL) == -1) {
		GHOST_ASSERT(false, "Could not compute time!");
	}

	// Taking care not to overflow the tv.tv_sec*1000
	return GHOST_TUns64(tv.tv_sec) * 1000 + tv.tv_usec / 1000 - m_start_time;
}
