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

#include <GL/wglew.h>



class GHOST_ContextWGL : public GHOST_Context
{
public:
	/**
	 * Constructor.
	 */
	GHOST_ContextWGL(HWND hWnd, HDC hDC);

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
	 * Tries to install a rendering context in this window.
	 * \param stereoVisual		Stereo visual for quad buffered stereo.
	 * \param numOfAASamples	Number of samples used for AA (zero if no AA)
	 * \return Indication as to whether installation has succeeded.
	 */
	virtual GHOST_TSuccess installDrawingContext(bool stereoVisual = false, GHOST_TUns16 numOfAASamples = 0);

	/**
	 * Removes the current drawing context.
	 * \return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess removeDrawingContext();

	/**
	 * Checks if it is OK for a remove the native display
	 * \return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess releaseNativeHandles();

private:
	GHOST_TSuccess init_wglew();
	GHOST_TSuccess init_multisample(const PIXELFORMATDESCRIPTOR& preferredPFD, int numOfAASamples);
	int enum_pixel_formats(PIXELFORMATDESCRIPTOR& preferredPFD, int numOfAASamples);

	const HDC  m_hDC;
	const HWND m_hWnd;

	HGLRC m_hGLRC;

	bool m_needSetPixelFormat;

	static HGLRC s_sharedGLRC;
	static HGLRC s_sharedDC;
	static int   s_shareCount;

	static bool s_wglewInitialized;
};



#endif // __GHOST_CONTEXTWGL_H__
