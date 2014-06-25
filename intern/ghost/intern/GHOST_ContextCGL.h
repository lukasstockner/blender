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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_ContextCGL.h
 *  \ingroup GHOST
 * Declaration of GHOST_ContextCGL class.
 */

#ifndef __GHOST_CONTEXTCGL_H__
#define __GHOST_CONTEXTCGL_H__


#include "GHOST_Context.h"



@class NSWindow;
@class NSOpenGLView;
@class NSOpenGLContext;



#ifndef GHOST_OPENGL_CGL_CONTEXT_FLAGS
#define GHOST_OPENGL_CGL_CONTEXT_FLAGS 0
#endif

#ifndef GHOST_OPENGL_CGL_RESET_NOTIFICATION_STRATEGY
#define GHOST_OPENGL_CGL_RESET_NOTIFICATION_STRATEGY 0
#endif



class GHOST_ContextCGL : public GHOST_Context
{
public:
	/**
	 * Constructor.
	 */
	GHOST_ContextCGL(
		bool          stereoVisual,
		GHOST_TUns16  numOfAASamples,
		NSWindow     *window,
		NSOpenGLView *openGLView,
		int           contextProfileMask,
		int           contextMajorVersion,
		int           contextMinorVersion,
		int           contextFlags,
		int           contextResetNotificationStrategy
	);


	/**
	 * Destructor.
	 */
	virtual ~GHOST_ContextCGL();

	/**
	 * Swaps front and back buffers of a window.
	 * \return  A boolean success indicator.
	 */
	virtual GHOST_TSuccess swapBuffers();

	/**
	 * Activates the drawing context of this window.
	 * \return  A boolean success indicator.
	 */
	virtual GHOST_TSuccess activateDrawingContext();

	/**
	 * Call immediately after new to initialize.  If this fails then immediately delete the object.
	 * \param stereoVisual		Stereo visual for quad buffered stereo.
	 * \param numOfAASamples	Number of samples used for AA (zero if no AA)
	 * \return Indication as to whether initialization has succeeded.
	 */
	virtual GHOST_TSuccess initializeDrawingContext(bool stereoVisual = false, GHOST_TUns16 numOfAASamples = 0);

	/**
	 * Updates the drawing context of this window. Needed
	 * whenever the window is changed.
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess updateDrawingContext();

	/**
	 * Checks if it is OK for a remove the native display
	 * \return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess releaseNativeHandles();

	/**
	 * Sets the swap interval for swapBuffers.
	 * \param interval The swap interval to use.
	 * \return A boolean success indicator.
	 */
	virtual GHOST_TSuccess setSwapInterval(int interval);

	/**
	 * Gets the current swap interval for swapBuffers.
	 * \param intervalOut Variable to store the swap interval if it can be read.
	 * \return Whether the swap interval can be read.
	 */
	virtual GHOST_TSuccess getSwapInterval(int&);

private:

	/** The window containing the OpenGL view */
	NSWindow *m_window;
	
	/** The openGL view */
	NSOpenGLView *m_openGLView; 

	int m_contextProfileMask;
	int m_contextMajorVersion;
	int m_contextMinorVersion;
	int m_contextFlags;
	int m_contextResetNotificationStrategy;

	/** The opgnGL drawing context */
	NSOpenGLContext *m_openGLContext;
	
	/** The first created OpenGL context (for sharing display lists) */
	static NSOpenGLContext *s_sharedOpenGLContext;	
	static int              s_sharedCount;

};



#endif // __GHOST_CONTEXTCGL_H__
