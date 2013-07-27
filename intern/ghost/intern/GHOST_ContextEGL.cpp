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

/** \file ghost/intern/GHOST_ContextEGL.cpp
 *  \ingroup GHOST
 * Definition of GHOST_ContextEGL class.
 */

#include "GHOST_ContextEGL.h"

#include <cassert>
#include <cstdio>
#include <vector>



static const char* get_egl_error_string(EGLenum error)
{
	switch(error) {
		case EGL_SUCCESS:
			return "EGL_SUCCESS: The last function succeeded without error.";

		case EGL_NOT_INITIALIZED:
			return "EGL_NOT_INITIALIZED: EGL is not initialized, or could not be initialized, for the specified EGL display connection.";

		case EGL_BAD_ACCESS:
			return "EGL_BAD_ALLOC: EGL cannot access a requested resource (for example a context is bound in another thread).";

		case EGL_BAD_ALLOC:
			return "EGL_BAD_ALLOC: EGL failed to allocate resources for the requested operation.";

		case EGL_BAD_ATTRIBUTE:
			return "EGL_BAD_ATTRIBUTE: An unrecognized attribute or attribute value was passed in the attribute list.";

		case EGL_BAD_CONTEXT:
			return "EGL_BAD_CONTEXT: An EGLContext argument does not name a valid EGL rendering context.";

		case EGL_BAD_CONFIG:
			return "EGL_BAD_CONFIG: An EGLConfig argument does not name a valid EGL frame buffer configuration.";

		case EGL_BAD_CURRENT_SURFACE:
			return "EGL_BAD_CURRENT_SURFACE: The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid.";

		case EGL_BAD_DISPLAY:
			return "EGL_BAD_DISPLAY: An EGLDisplay argument does not name a valid EGL display connection.";

		case EGL_BAD_SURFACE:
			return "EGL_BAD_SURFACE: An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering.";

		case EGL_BAD_MATCH:
			return "EGL_BAD_MATCH: Arguments are inconsistent (for example, a valid context requires buffers not supplied by a valid surface).";

		case EGL_BAD_PARAMETER:
			return "EGL_BAD_PARAMETER: One or more argument values are invalid.";

		case EGL_BAD_NATIVE_PIXMAP:
			return "EGL_BAD_NATIVE_PIXMAP: A NativePixmapType argument does not refer to a valid native pixmap.";

		case EGL_BAD_NATIVE_WINDOW:
			return "EGL_BAD_NATIVE_WINDOW: A NativeWindowType argument does not refer to a valid native window.";

		case EGL_CONTEXT_LOST:
			return "EGL_CONTEXT_LOST: A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering.";

		default:
			abort();
	}
}



static inline bool egl_chk(bool result, const char* file, int line, const char* text)
{
	if (!result) {
		fprintf(stderr, "EGL Error: %s\n", get_egl_error_string(eglGetError()));
		fprintf(stderr, "%s(%d): %s\n", file, line, text);
	}

	return result;
}



#define EGL_CHK(x) egl_chk((x), __FILE__, __LINE__, #x)



static inline void bindAPI(EGLenum api)
{
	if (EGLEW_VERSION_1_2)
		EGL_CHK(eglBindAPI(api));
}



#if defined(WITH_ANGLE)
HMODULE GHOST_ContextEGL::s_d3dcompiler = NULL;
#endif



bool GHOST_ContextEGL::s_eglewInitialized = false;



EGLContext GHOST_ContextEGL::s_gl_sharedContext   = NULL;
EGLint     GHOST_ContextEGL::s_gl_sharedCount     = 0;

EGLContext GHOST_ContextEGL::s_gles_sharedContext = NULL;
EGLint     GHOST_ContextEGL::s_gles_sharedCount   = 0;

EGLContext GHOST_ContextEGL::s_vg_sharedContext   = NULL;
EGLint     GHOST_ContextEGL::s_vg_sharedCount     = 0;



template <typename T>
T& choose_api(EGLenum api, T& a, T& b, T& c)
{
	switch(api) {
		case EGL_OPENGL_API:
			return a;
		case EGL_OPENGL_ES_API:
			return b;
		case EGL_OPENVG_API:
			return c;
		default:
			abort();
	}
}



GHOST_ContextEGL::GHOST_ContextEGL(
	EGLNativeWindowType  nativeWindow,
	EGLNativeDisplayType nativeDisplay,
	EGLenum              api,
	EGLint               contextClientVersion
)
	: m_nativeWindow(nativeWindow)
	, m_nativeDisplay(nativeDisplay)
	, m_api(api)
	, m_contextClientVersion(contextClientVersion)
	, m_display(EGL_NO_DISPLAY)
	, m_surface(EGL_NO_SURFACE)
	, m_context(EGL_NO_CONTEXT)
	, m_sharedContext(choose_api(api, s_gl_sharedContext, s_gles_sharedContext, s_vg_sharedContext))
	, m_sharedCount  (choose_api(api, s_gl_sharedCount,   s_gles_sharedCount,   s_vg_sharedCount))
{
	assert(m_nativeWindow  != NULL);
	assert(m_nativeDisplay != NULL);
}



GHOST_ContextEGL::~GHOST_ContextEGL()
{
	removeDrawingContext();

	// Note: If multiple GHOST_ContextEGL are initialized with the same EGLNativeDisplayType then
	// those other GHOST_ContextEGL will become invalid after this call to eglTerminate.
	// See EGL 1.4 spec section 3.2
	EGL_CHK(::eglTerminate(m_display));
}



GHOST_TSuccess GHOST_ContextEGL::swapBuffers()
{
	return EGL_CHK(eglSwapBuffers(m_display, m_surface)) ? GHOST_kSuccess : GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextEGL::activateDrawingContext()
{
	bindAPI(m_api);

	return EGL_CHK(::eglMakeCurrent(m_display, m_surface, m_surface, m_context)) ? GHOST_kSuccess : GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextEGL::init_eglew()
{
	if (!s_eglewInitialized) {
		if (eglewInit() == GLEW_OK) {
			s_eglewInitialized = true;
			return GHOST_kSuccess;
		}
		else {
			return GHOST_kFailure;
		}
	}
	else {
		return GHOST_kSuccess;
	}
}



GHOST_TSuccess GHOST_ContextEGL::installDrawingContext(bool stereoVisual, GHOST_TUns16 numOfAASamples)
{
	std::vector<EGLint> attrib_list;

	assert(m_nativeWindow  != NULL);
	assert(m_nativeDisplay != NULL);

	if (stereoVisual) {
		fprintf(stderr, "Stereo OpenGL ES contexts are not supported.\n");
		return GHOST_kFailure;
	}

#if defined(WITH_ANGLE)
	// d3dcompiler_XX.dll needs to be loaded before ANGLE will work
	if (s_d3dcompiler == NULL) {
		s_d3dcompiler = LoadLibrary(D3DCOMPILER);

		if (s_d3dcompiler == NULL) {
			printf("LoadLibrary(\"" D3DCOMPILER "\") failed!\n");
			return GHOST_kFailure;
		}
	}
#endif

	EGLDisplay prev_display = eglGetCurrentDisplay();
	EGLSurface prev_draw    = eglGetCurrentSurface(EGL_DRAW);
	EGLSurface prev_read    = eglGetCurrentSurface(EGL_READ);
	EGLContext prev_context = eglGetCurrentContext();

	m_display = ::eglGetDisplay(m_nativeDisplay);

	if (!EGL_CHK(m_display != EGL_NO_DISPLAY))
		return GHOST_kFailure;

	EGLint major, minor;

	if (!EGL_CHK(::eglInitialize(m_display, &major, &minor)))
		goto error;

	printf("Initialized EGL %d.%d\n", major, minor);

	if (!EGL_CHK(::eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)))
		goto error;

	if (!init_eglew())
		goto error;

	bindAPI(m_api);

	attrib_list.reserve(20);

	attrib_list.push_back(EGL_RED_SIZE);
	attrib_list.push_back(8);

	attrib_list.push_back(EGL_GREEN_SIZE);
	attrib_list.push_back(8);

	attrib_list.push_back(EGL_BLUE_SIZE);
	attrib_list.push_back(8);

#ifdef GHOST_OPENGL_ALPHA
	attrib_list.push_back(EGL_ALPHA_SIZE);
	attrib_list.push_back(8);
#endif

	attrib_list.push_back(EGL_DEPTH_SIZE);
	attrib_list.push_back(24);

#ifdef GHOST_OPENGL_STENCIL
	attrib_list.push_back(EGL_STENCIL_SIZE);
	attrib_list.push_back(8);
#endif

	if (numOfAASamples > 0) {
		attrib_list.push_back(EGL_SAMPLE_BUFFERS);
		attrib_list.push_back(1);

		attrib_list.push_back(EGL_SAMPLES);
		attrib_list.push_back(numOfAASamples);
	}

	attrib_list.push_back(EGL_NONE);

	EGLConfig config;
	EGLint    num_config = 0;

	if (!EGL_CHK(::eglChooseConfig(m_display, &(attrib_list[0]), &config, 1, &num_config)))
		goto error;

	// common error is to assume that ChooseConfig worked because it returned EGL_TRUE
	if (num_config != 1)
		goto error;

	m_surface = ::eglCreateWindowSurface(m_display, config, m_nativeWindow, NULL);

	if (!EGL_CHK(m_surface != EGL_NO_SURFACE))
		goto error;

	attrib_list.clear();

	if (m_api == EGL_OPENGL_ES_API) {
		attrib_list.push_back(EGL_CONTEXT_CLIENT_VERSION);
		attrib_list.push_back(m_contextClientVersion);
	}

	attrib_list.push_back(EGL_NONE);

	m_context = ::eglCreateContext(m_display, config, m_sharedContext, &(attrib_list[0]));

	if (!EGL_CHK(m_context != EGL_NO_CONTEXT))
		goto error;

	if (!EGL_CHK(m_sharedContext != EGL_NO_CONTEXT))
		m_sharedContext = m_context;

	m_sharedCount++;

	if (!EGL_CHK(::eglMakeCurrent(m_display, m_surface, m_surface, m_context)))
		goto error;

	return GHOST_kSuccess;

error:
	removeDrawingContext();

	EGL_CHK(eglTerminate(m_display));
	m_display = EGL_NO_DISPLAY;

	eglMakeCurrent(prev_display, prev_draw, prev_read, prev_context);

	return GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextEGL::removeDrawingContext()
{
	bindAPI(m_api);

	if (m_context == ::eglGetCurrentContext())
		EGL_CHK(::eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

	if (m_context != m_sharedContext || m_sharedCount == 1) {
		assert(m_sharedCount > 0);

		m_sharedCount--;

		if (m_sharedCount == 0)
			m_sharedContext = EGL_NO_CONTEXT;

		if (EGL_CHK(::eglDestroyContext(m_display, m_context)))
			m_context = EGL_NO_CONTEXT;
	}

	if (EGL_CHK(::eglDestroySurface(m_display, m_surface)))
		m_surface = EGL_NO_SURFACE;

	return m_surface == EGL_NO_SURFACE && m_context == EGL_NO_CONTEXT ? GHOST_kSuccess : GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextEGL::releaseNativeHandles()
{
	m_nativeWindow  = NULL;
	m_nativeDisplay = NULL;

	return GHOST_kSuccess;
}
