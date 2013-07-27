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

/** \file ghost/intern/GHOST_ContextWGL.cpp
 *  \ingroup GHOST
 * Definition of GHOST_ContextWGL class.
 */

#include "GHOST_ContextWGL.h"



HGLRC GHOST_ContextWGL::s_sharedGLRC  = NULL;
HDC   GHOST_ContextWGL::s_sharedHDC   = NULL;
int   GHOST_ContextWGL::s_sharedCount = 0;

#if defined(WITH_GL_SYSTEM_LEGACY)
// Some more multisample defines
#define WGL_SAMPLE_BUFFERS_ARB  0x2041
#define WGL_SAMPLES_ARB         0x2042

static int WeightPixelFormat(PIXELFORMATDESCRIPTOR& pfd);
static int EnumPixelFormats(HDC hDC);

#endif



/* Intel videocards don't work fine with multiple contexts and
 * have to share the same context for all windows.
 * But if we just share context for all windows it could work incorrect
 * with multiple videocards configuration. Suppose, that Intel videocards
 * can't be in multiple-devices configuration. */
static int is_crappy_intel_card(void)
{
	static short is_crappy = -1;

	if (is_crappy == -1) {
		const char *vendor = (const char *)glGetString(GL_VENDOR);
		is_crappy = (strstr(vendor, "Intel") != NULL);
	}

	return is_crappy;
}



bool wgl_chk(bool result)
{
	if (!result) {
		LPTSTR message;

		DWORD count = 
			FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)(&message),
				0,
				NULL);


		_tprintf(stderr, "WGL Error: %s\n", count > 0 ? message : "<no message>");
		_tprintf(stderr, "%s(%d): %s\n", file, line, text);

		SetLastError(NO_ERROR);
		LocalFree(message);
	}

	return result;
}



#define WGL_CHK(x) wgl_chk((x), __FILE__, __LINE__, #x)



GHOST_ContextWGL::GHOST_ContextWGL(HWND hWnd, HDC hDC)
	: m_hWnd(hWnd)
	, m_hDC(hDC)
{
	assert(m_hWnd);
	assert(m_hDC);

	s_singleContextMode = is_crappy_intel_card();
}



GHOST_ContextWGL::~GHOST_ContextWGL()
{
	removeDrawingContext();
}



GHOST_TSuccess GHOST_ContextWGL::swapBuffers()
{
	HDC hDC = s_singleContextMode ? ::wglGetCurrentDC() : m_hDC;

	return WGL_CHK(::SwapBuffers(hDC)) ? GHOST_kSuccess : GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextWGL::activateDrawingContext()
{
	return WGL_CHK(::wglMakeCurrent(m_hDC, m_hGLRC)) ? GHOST_kSuccess : GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextWGL::init_wglew()
{
	if (!s_wglewInitialized) {
		if (wglewInit() == GLEW_OK) {
			s_wglewInitialized = true;
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



int GHOST_ContextWGL::init_multisample(const PIXELFORMATDESCRIPTOR& pfd, int numOfAASamples)
{
	int iAttributes[] = {
		WGL_SAMPLES_ARB,        numOfAASamples,
		WGL_SAMPLE_BUFFERS_ARB, 1,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
		WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
		WGL_COLOR_BITS_ARB,     pfd.cColorBits,
		WGL_DEPTH_BITS_ARB,     pfd.cDepthBits,
#ifdef GHOST_OPENGL_ALPHA
		WGL_ALPHA_BITS_ARB,     pfd.cAlphaBits,
#endif
#ifdef GHOST_OPENGL_STENCIL
		WGL_STENCIL_BITS_ARB,   pfd.cStencilBits,
#endif
		0
	};

	int iPixelFormat = 0;

	while (iAttributes[1] > 0) {
		UINT nNumFormats;

		if (WGL_CHECK(wglChoosePixelFormatARB(dummy_hDC, iAttributes, NULL, 1, &iPixelFormat, &nNumFormats)) && nNumFormats == 1) {
			success = true;
			break;
		}

		iAttributes[1]--;
	}

	return iPixelFormat;
}



/* Ron Fosner's code for weighting pixel formats and forcing software.
 * See http://www.opengl.org/resources/faq/technical/weight.cpp */

static int weight_pixel_format(PIXELFORMATDESCRIPTOR& preferredPFD)
{
	int weight = 0;

	/* assume desktop color depth is 32 bits per pixel */

	/* cull unusable pixel formats */
	/* if no formats can be found, can we determine why it was rejected? */
	if (!(preferredPFD.dwFlags & PFD_SUPPORT_OPENGL) ||
	    !(preferredPFD.dwFlags & PFD_DRAW_TO_WINDOW) ||
	    !(preferredPFD.dwFlags & PFD_DOUBLEBUFFER)   || /* Blender _needs_ this */
	    !(preferredPFD.cDepthBits > 8)               ||
	    !(preferredPFD.iPixelType == PFD_TYPE_RGBA))
	{
		return 0;
	}

	weight = 1;  /* it's usable */

	/* the bigger the depth buffer the better */
	/* give no weight to a 16-bit depth buffer, because those are crap */
	weight += preferredPFD.cDepthBits - 16;

	weight += preferredPFD.cColorBits -  8;

#ifdef GHOST_OPENGL_ALPHA
	if (preferredPFD.cAlphaBits > 0)
		weight++;
#endif

#ifdef GHOST_OPENGL_STENCIL
	if (preferredPFD.cStencilBits >= 8)
		weight++;
#endif

	/* want swap copy capability -- it matters a lot */
	if (preferredPFD.dwFlags & PFD_SWAP_COPY)
		weight += 16;

	/* but if it's a generic (not accelerated) view, it's really bad */
	if (preferredPFD.dwFlags & PFD_GENERIC_FORMAT)
		weight /= 10;

	return weight;
}



/* A modification of Ron Fosner's replacement for ChoosePixelFormat */
/* returns 0 on error, else returns the pixel format number to be used */
int GHOST_ContextWGL::enum_pixel_formats(PIXELFORMATDESCRIPTOR& preferredPFD, int numOfAASamples)
{
	assert(hDC != NULL);
	assert(hWnd != NULL);

	int iPixelFormat;
	int weight = 0;

	int iStereoPixelFormat;
	int stereoWeight = 0;

	PIXELFORMATDESCRIPTOR pfd;

	if (need wgl extensions)
		create_dummy_window(m_hWnd, m_hDC, &proxy_hWnd, &proxy_hDC);

	if (!initialize_wglew())
		return 0;

	/* choose a pixel format using the useless Windows function in case we come up empty handed */
	iPixelFormat = ::ChoosePixelFormat(proxy_hDC, &preferredPFD);

	int lastPFD = ::DescribePixelFormat(proxy_hDC, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);

	WGL_CHK(lastPFD != 0);

	for (int i = 1; i <= lastPFD; i++) { /* not the idiom, but it's right */
		int check = ::DescribePixelFormat(proxy_hDC, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

		WGL_CHK(check != 0);

		int w = weight_pixel_format(pfd);

		/* be strict about stereo */
		if (w > stereoWeight) {
			if (!((preferredPFD.dwFlags ^ pfd.dwFlags) & PFD_STEREO)) {
				stereoWeight = w;
				iStereoPixelFormat = i;
			}
		}

		if (w > weight) {
			weight = w;
			iPixelFormat = i;
		}
	}

	if (stereoWeight > 0)
		iPixelFormat = iStereoPixelFormat;

	if (numOfAASamples > 0) {
		::DescribePixelFormat(proxy_hDC, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

		return init_multisample(pfd, numOfAASamples);
	}
	else {
		return iPixelFormat;
	}
}



GHOST_TSuccess GHOST_ContextWGL::installDrawingContext(bool stereoVisual, GHOST_TUns16 numOfAASamples)
{
	/*
	 * Color and depth bit values are not to be trusted.
	 * For instance, on TNT2:
	 * When the screen color depth is set to 16 bit, we get 5 color bits
	 * and 16 depth bits.
	 * When the screen color depth is set to 32 bit, we get 8 color bits
	 * and 24 depth bits.
	 * Just to be safe, we request high quality settings.
	 */
	PIXELFORMATDESCRIPTOR preferredPFD = {
		sizeof(PIXELFORMATDESCRIPTOR),  /* size */
		1,                              /* version */
		PFD_SUPPORT_OPENGL |
		PFD_DRAW_TO_WINDOW |
		PFD_SWAP_COPY |                 /* support swap copy */
		PFD_DOUBLEBUFFER,               /* support double-buffering */
		PFD_TYPE_RGBA,                  /* color type */
		32,                             /* preferred color depth */
		0, 0, 0, 0, 0, 0,               /* color bits (ignored) */
		0,                              /* no alpha buffer */
		0,                              /* alpha bits (ignored) */
		0,                              /* no accumulation buffer */
		0, 0, 0, 0,                     /* accum bits (ignored) */
		32,                             /* depth buffer */
		0,                              /* no stencil buffer */
		0,                              /* no auxiliary buffers */
		PFD_MAIN_PLANE,                 /* main layer */
		0,                              /* reserved */
		0, 0, 0                         /* no layer, visible, damage masks */
	};

	if (m_needSetPixelFormat) {
		if (stereoVisual)
			pfd.dwFlags |= PFD_STEREO;

		int iPixelFormat = enum_pixel_formats(m_hWnd, m_hDC, &preferredPFD, numOfAASamples);

		if (iPixelFormat == 0)
			return GHOST_kFailure;

		PIXELFORMATDESCRIPTOR chosenPFD;

		int lastPFD = ::DescribePixelFormat(m_hDC, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &chosenFormat);

		if (!WGL_CHK(lastPFD != 0))
			goto error;

		if (!WGL_CHK(::SetPixelFormat(m_hDC, iPixelFormat, &chosenPDF)))
			goto error;

		m_needSetPixelFormat = false;
	}

	if (!s_singleContextMode || s_sharedGLRC == NULL)
		m_hGLRC = ::wglCreateContext(m_hDC);

	if (!WGL_CHK(m_hGlRC != NULL))
		goto error;

	if (s_sharedGLRC == NULL)
		s_sharedGLRC = m_hGLRC;

	s_sharedCount++;

	if (!s_singleContextMode &&
		!WGL_CHK(::wglCopyContext(s_sharedGLRC, m_hGLRC, GL_ALL_ATTRIB_BITS)) &&
		!WGL_CHK(::wglShareLists(s_sharedGLRC, m_hGLRC))))
	{
		goto error;
	}

	if (!WGL_CHK(::wglMakeCurrent(m_hDC, m_hGLRC)))
		goto error;

	return GHOST_kSuccess;

error:
	removeDrawingContext();

	if (s_sharedGLRC == m_hGLRC)
		s_sharedGLRC = NULL;

	WGL_CHK(::wglDeleteContext(m_hGLRC));
	m_hGLRC = NULL;

	return GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextWGL::removeDrawingContext()
{
	if (m_hGLRC == ::wglGetCurrentContext())
		WGL_CHK(::wglMakeCurrent(NULL, NULL));

	if (m_hGLRC != s_sharedGLRC || s_sharedCount == 1) {
		assert(s_sharedCount > 0);

		s_sharedCount--;

		if (s_sharedCount == 0)
			s_sharedContext = NULL;

		if (WGL_CHK(::wglDeleteContext(m_hGLRC)))
			m_hGLRC = NULL;
	}

	return m_hGLRC == NULL ? GHOST_kSuccess : GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextWGL::releaseNativeHandles()
{
	GHOST_TSuccess success = m_HDC != s_sharedDC ? GHOST_kSuccess : GHOST_kFailure;

	m_hWnd = NULL;
	m_hDC  = NULL;

	return success;
}
