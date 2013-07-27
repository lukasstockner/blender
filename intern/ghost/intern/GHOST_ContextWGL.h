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

/** \file ghost/intern/GHOST_ContextWGL.h
 *  \ingroup GHOST
 * Declaration of GHOST_ContextWGL class.
 */

#ifndef __GHOST_CONTEXTWGL_H__
#define __GHOST_CONTEXTWGL_H__

#include "GHOST_Context.h"

#include <GL/glew.h>
#include <GL/wglew.h>



class GHOST_ContextWGL : public GHOST_Context
{
public:
	/**
	 * Constructor.
	 */
	GHOST_ContextWGL(
		HWND hWnd,
		HDC  hDC,
		int  contextProfileMask  = 0,
		int  contextFlags        = 0,
		int  contextMajorVersion = 0,
		int  contextMinorVersion = 0);

	/**
	 * Destructor.
	 */
	virtual ~GHOST_ContextWGL();

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
	 * Checks if it is OK for a remove the native display
	 * \return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess releaseNativeHandles();

	static void setSingleContextMode(bool on);

private:
	HDC  m_hDC;
	HWND m_hWnd;

	int m_contextProfileMask;
	int m_contextFlags;
	int m_contextMajorVersion;
	int m_contextMinorVersion;

	HGLRC m_hGLRC;

	bool m_needSetPixelFormat;

	static HGLRC s_sharedHGLRC;
	static HDC   s_sharedHDC;
	static int   s_sharedCount;
};



#endif // __GHOST_CONTEXTWGL_H__
