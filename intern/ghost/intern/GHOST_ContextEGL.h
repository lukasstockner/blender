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

/** \file ghost/intern/GHOST_ContextEGL.h
 *  \ingroup GHOST
 * Declaration of GHOST_ContextEGL class.
 */

#ifndef __GHOST_CONTEXTEGL_H__
#define __GHOST_CONTEXTEGL_H__

#include "GHOST_Context.h"

#define eglewGetContext() eglewContext
#include <GL/eglew.h>
extern "C" EGLEWContext* eglewContext;



class GHOST_ContextEGL : public GHOST_Context
{
public:
	/**
	 * Constructor.
	 */
	GHOST_ContextEGL(
		EGLNativeWindowType  nativeWindow,
		EGLNativeDisplayType nativeDisplay,
		EGLenum              api                  = 0,
		EGLint               contextClientVersion = 0
	);

	/**
	 * Destructor.
	 */
	virtual ~GHOST_ContextEGL();

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
	virtual GHOST_TSuccess initializeDrawingContext(bool stereoVisual = false, GHOST_TUns16 numOfAASamples = 0);

	/**
	 * Removes references to native handles from this context and then returns
	 * GHOST_kSuccess if it is OK for the parent to release the handles
	 * and GHOST_kFailure if releasing the handles will interfere with sharing
	 * \return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess releaseNativeHandles();

private:
	void GHOST_ContextEGL::initEGlew();

	EGLNativeDisplayType m_nativeDisplay;
	EGLNativeWindowType  m_nativeWindow;

	const EGLenum m_api;
	const EGLint  m_contextClientVersion;

	EGLContext m_context;
	EGLSurface m_surface;
	EGLDisplay m_display;

	EGLContext& m_sharedContext;
	EGLint&     m_sharedCount;

	EGLEWContext* m_eglewContext;

	static EGLContext s_gl_sharedContext;
	static EGLint     s_gl_sharedCount;

	static EGLContext s_gles_sharedContext;
	static EGLint     s_gles_sharedCount;

	static EGLContext s_vg_sharedContext;
	static EGLint     s_vg_sharedCount;

#if defined(WITH_ANGLE)
	static HMODULE s_d3dcompiler;
#endif
};



#endif // __GHOST_CONTEXTEGL_H__
