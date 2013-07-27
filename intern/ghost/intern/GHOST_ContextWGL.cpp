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

#include <tchar.h>

#include <cstdio>
#include <cassert>
#include <vector>



HGLRC GHOST_ContextWGL::s_sharedHGLRC = NULL;
HDC   GHOST_ContextWGL::s_sharedHDC   = NULL;
int   GHOST_ContextWGL::s_sharedCount = 0;


#ifndef NDEBUG
static const char* extensionRenderer = NULL;
#endif

static bool singleContextMode = false;




/* Intel videocards don't work fine with multiple contexts and
 * have to share the same context for all windows.
 * But if we just share context for all windows it could work incorrect
 * with multiple videocards configuration. Suppose, that Intel videocards
 * can't be in multiple-devices configuration. */
static bool is_crappy_intel_card()
{
	return strstr((const char*)glGetString(GL_VENDOR), "Intel") != NULL;
}



bool win32_chk(bool result, const char* file = NULL, int line = 0, const char* text = NULL)
{
	if (!result) {
		LPTSTR message;
		DWORD error = GetLastError();

		DWORD count = 
			FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				error,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)(&message),
				0,
				NULL);

		const char* formattedMsg = count > 0 ? message : "<FormatMessage Failed>";

#ifndef NDEBUG
		_ftprintf(stderr, "%s(%d):%s -> Windows Error (%04X): %s\n", file, line, text, error, formattedMsg);
#else
		_ftprintf(stderr, "Windows Error (%4X): %s\n", error, formattedMsg);
#endif

		SetLastError(NO_ERROR);

		LocalFree(message);
	}

	return result;
}

#ifndef NDEBUG
#define WIN32_CHK(x) win32_chk((x), __FILE__, __LINE__, #x)
#else
#define WIN32_CHK(x) win32_chk(x)
#endif



GHOST_ContextWGL::GHOST_ContextWGL(
	HWND hWnd,
	HDC  hDC,
	int  contextProfileMask,
	int  contextFlags,
	int  contextMajorVersion,
	int  contextMinorVersion
)
	: m_hWnd(hWnd)
	, m_hDC(hDC)
	, m_contextProfileMask(contextProfileMask)
	, m_contextFlags(contextFlags)
	, m_contextMajorVersion(contextMajorVersion)
	, m_contextMinorVersion(contextMinorVersion)
{
	assert(m_hWnd);
	assert(m_hDC);
}



GHOST_ContextWGL::~GHOST_ContextWGL()
{
	if (m_hGLRC != NULL) {
		if (m_hGLRC == ::wglGetCurrentContext())
			WIN32_CHK(::wglMakeCurrent(NULL, NULL));

		if (m_hGLRC != s_sharedHGLRC || s_sharedCount == 1) {
			assert(s_sharedCount > 0);

			s_sharedCount--;

			if (s_sharedCount == 0) {
				s_sharedHGLRC = NULL;
				s_sharedHDC   = NULL;
			}

			if (WIN32_CHK(::wglDeleteContext(m_hGLRC)))
				m_hGLRC = NULL;
		}
	}
}



GHOST_TSuccess GHOST_ContextWGL::swapBuffers()
{
	return WIN32_CHK(::SwapBuffers(m_hDC)) ? GHOST_kSuccess : GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextWGL::activateDrawingContext()
{
	return WIN32_CHK(::wglMakeCurrent(m_hDC, m_hGLRC)) ? GHOST_kSuccess : GHOST_kFailure;
}



/* Ron Fosner's code for weighting pixel formats and forcing software.
 * See http://www.opengl.org/resources/faq/technical/weight.cpp */

static int weight_pixel_format(PIXELFORMATDESCRIPTOR& pfd)
{
	int weight = 0;

	/* assume desktop color depth is 32 bits per pixel */

	/* cull unusable pixel formats */
	/* if no formats can be found, can we determine why it was rejected? */
	if (!(pfd.dwFlags & PFD_SUPPORT_OPENGL)  ||
	    !(pfd.dwFlags & PFD_DRAW_TO_WINDOW)  ||
	    !(pfd.dwFlags & PFD_DOUBLEBUFFER)    || /* Blender _needs_ this */
	    !(pfd.iPixelType == PFD_TYPE_RGBA)   ||
	     (pfd.cDepthBits < 16)               ||
	     (pfd.dwFlags & PFD_GENERIC_FORMAT))    /* no software renderers */
	{
		return 0;
	}

	weight = 1;  /* it's usable */

	/* the bigger the depth buffer the better */
	/* give no weight to a 16-bit depth buffer, because those are crap */
	weight += pfd.cDepthBits - 16;

	weight += pfd.cColorBits -  8;

#ifdef GHOST_OPENGL_ALPHA
	if (pfd.cAlphaBits > 0)
		weight++;
#endif

#ifdef GHOST_OPENGL_STENCIL
	if (pfd.cStencilBits >= 8)
		weight++;
#endif

	/* want swap copy capability -- it matters a lot */
	if (pfd.dwFlags & PFD_SWAP_COPY)
		weight += 16;

	return weight;
}



/*
 * A modification of Ron Fosner's replacement for ChoosePixelFormat
 * returns 0 on error, else returns the pixel format number to be used
 */
static int choose_pixel_format_legacy(HDC hDC, PIXELFORMATDESCRIPTOR& preferredPFD)
{
	int iPixelFormat = 0;
	int weight = 0;

	int iStereoPixelFormat = 0;
	int stereoWeight = 0;

	/* choose a pixel format using the useless Windows function in case we come up empty handed */
	iPixelFormat = ::ChoosePixelFormat(hDC, &preferredPFD);

	int lastPFD = ::DescribePixelFormat(hDC, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);

	WIN32_CHK(lastPFD != 0);

	for (int i = 1; i <= lastPFD; i++) {
		PIXELFORMATDESCRIPTOR pfd;
		int check = ::DescribePixelFormat(hDC, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

		WIN32_CHK(check == lastPFD);

		float w = weight_pixel_format(pfd);

		if (w > weight) {
			weight = w;
			iPixelFormat = i;
		}

		if (w > stereoWeight && (preferredPFD.dwFlags & pfd.dwFlags & PFD_STEREO)) {
			stereoWeight = w;
			iStereoPixelFormat = i;
		}
	}

	/* choose any available stereo format over a non-stereo format */
	return iStereoPixelFormat != 0 ? iStereoPixelFormat : iPixelFormat;
}



/*
 * Clone a window for the purpose of creating a temporary context to initialize WGL extensions.
 * There is no generic way to clone the lpParam parameter, so the caller is responsible for cloning it themselves.
 */

static HWND clone_window(HWND hWnd, LPVOID lpParam)
{
	int count;

	SetLastError(NO_ERROR);

	DWORD dwExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
	WIN32_CHK(GetLastError() == NO_ERROR);

	WCHAR lpClassName[100] = L"";
	count = GetClassNameW(hWnd, lpClassName, sizeof(lpClassName));
	WIN32_CHK(GetLastError() == NO_ERROR);

	WCHAR lpWindowName[100] = L"";
	count = GetWindowTextW(hWnd, lpWindowName, sizeof(lpWindowName));
	WIN32_CHK(GetLastError() == NO_ERROR);

	DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);
	WIN32_CHK(GetLastError() == NO_ERROR);

	RECT rect;
	GetWindowRect(hWnd, &rect);
	WIN32_CHK(GetLastError() == NO_ERROR);

	HWND hWndParent = (HWND)GetWindowLong(hWnd, GWL_HWNDPARENT);
	WIN32_CHK(GetLastError() == NO_ERROR);

	HMENU hMenu = GetMenu(hWnd);
	WIN32_CHK(GetLastError() == NO_ERROR);

	HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE);
	WIN32_CHK(GetLastError() == NO_ERROR);

	HWND hwndCloned =
		CreateWindowExW(
			dwExStyle,
			lpClassName,
			lpWindowName,
			dwStyle,
			rect.left,
			rect.top,
			rect.right - rect.left,
			rect.bottom - rect.top,
			hWndParent,
			hMenu,
			hInstance,
			lpParam);

	WIN32_CHK(hwndCloned != NULL);

	return hwndCloned;
}



static bool init_wglew(HWND hWnd, HDC hDC, PIXELFORMATDESCRIPTOR& preferredPFD)
{
	static bool wglewInitialized = false;

	if (!wglewInitialized) {
		HWND  dummyHWND  = NULL;
		HDC   dummyHDC   = NULL;
		HGLRC dummyHGLRC = NULL;

		SetLastError(NO_ERROR);

		HDC   prevHDC   = ::wglGetCurrentDC();
		WIN32_CHK(GetLastError() == NO_ERROR);

		HGLRC prevHGLRC = ::wglGetCurrentContext();
		WIN32_CHK(GetLastError() == NO_ERROR);

		dummyHWND = clone_window(hWnd, NULL);

		if (!WIN32_CHK(dummyHWND != NULL))
			goto finalize;

		dummyHDC = GetDC(dummyHWND);

		if (!WIN32_CHK(dummyHDC != NULL))
			goto finalize;

		int iPixelFormat = choose_pixel_format_legacy(dummyHDC, preferredPFD);

		if (iPixelFormat == 0)
			goto finalize;

		PIXELFORMATDESCRIPTOR chosenPFD;
		if (!WIN32_CHK(::DescribePixelFormat(dummyHDC, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &chosenPFD)))
			goto finalize;

		if (!WIN32_CHK(::SetPixelFormat(dummyHDC, iPixelFormat, &chosenPFD)))
			goto finalize;

		dummyHGLRC = ::wglCreateContext(dummyHDC);

		if (!WIN32_CHK(dummyHGLRC != NULL))
			goto finalize;

		if (!WIN32_CHK(::wglMakeCurrent(dummyHDC, dummyHGLRC)))
			goto finalize;

		if (wglewInit() == GLEW_OK)
			wglewInitialized = true;

		// the following are not technially WGLEW, but they also require a context to work

#ifndef NDEBUG
		if (extensionRenderer == NULL)
			extensionRenderer = _strdup((const char *)glGetString(GL_RENDERER));
#endif

		singleContextMode = is_crappy_intel_card();

finalize:
		WIN32_CHK(::wglMakeCurrent(prevHDC, prevHGLRC));

		if (dummyHGLRC != NULL)
			WIN32_CHK(::wglDeleteContext(dummyHGLRC));

		if (dummyHWND != NULL) {
			if (dummyHDC != NULL)
				WIN32_CHK(::ReleaseDC(dummyHWND, dummyHDC));

			WIN32_CHK(::DestroyWindow(dummyHWND));
		}
	}

	return wglewInitialized;
}



static int _choose_pixel_format_arb(HDC hDC, bool stereoVisual, int numOfAASamples, int swapMethod)
{
#define SAMPLES        iAttributes[1]
#define SAMPLE_BUFFERS iAttributes[3]

	int iAttributes[] = {
		WGL_SAMPLES_ARB,        0,
		WGL_SAMPLE_BUFFERS_ARB, 0,
		WGL_STEREO_ARB,         stereoVisual ? GL_TRUE : GL_FALSE,
		WGL_SWAP_METHOD_ARB,    swapMethod,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
		WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
		WGL_COLOR_BITS_ARB,     8,
		WGL_DEPTH_BITS_ARB,     8,
#ifdef GHOST_OPENGL_ALPHA
		WGL_ALPHA_BITS_ARB,     8,
#endif
#ifdef GHOST_OPENGL_STENCIL
		WGL_STENCIL_BITS_ARB,   8,
#endif
		0
	};

	int iPixelFormat = 0;

	SAMPLES = numOfAASamples > 128 ? 128 : numOfAASamples; /* guard against some insanely high number of samples */

	/* Choose the multisample format closest to what was asked for. */
	while (SAMPLES >= 0) {
		SAMPLE_BUFFERS = SAMPLES > 0 ? 1 : 0;

		UINT nNumFormats;
		WIN32_CHK(wglChoosePixelFormatARB(hDC, iAttributes, NULL, 1, &iPixelFormat, &nNumFormats));

		if (nNumFormats > 0) // total number of formats that match (regardless of size of iPixelFormat array)
			break;

		iPixelFormat = 0; // If not reset, then the state of iPixelFormat is undefined after call to wglChoosePixelFormatARB

		SAMPLES--;
	}

	/* choose any available stereo format over a non-stereo format */
	return iPixelFormat;

#undef SAMPLES
#undef SAMPLE_BUFFERS
}



static int choose_pixel_format_arb(HDC hDC, bool stereoVisual, int numOfAASamples)
{
	int iPixelFormat;

	iPixelFormat = _choose_pixel_format_arb(hDC, stereoVisual, numOfAASamples, WGL_SWAP_COPY_ARB);

	if (iPixelFormat == 0) {
		iPixelFormat = _choose_pixel_format_arb(hDC, stereoVisual, numOfAASamples, WGL_SWAP_UNDEFINED_ARB);
	}

	if (iPixelFormat == 0 && stereoVisual) {
		iPixelFormat = _choose_pixel_format_arb(hDC, false, numOfAASamples, WGL_SWAP_COPY_ARB);

		if (iPixelFormat == 0) {
			iPixelFormat = _choose_pixel_format_arb(hDC, false, numOfAASamples, WGL_SWAP_UNDEFINED_ARB);
		}
	}

	return iPixelFormat;
}



static int choose_pixel_format(HWND hWnd, HDC hDC, bool stereoVisual, int numOfAASamples)
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
		sizeof(PIXELFORMATDESCRIPTOR),   /* size */
		1,                               /* version */
		PFD_SUPPORT_OPENGL |
		PFD_DRAW_TO_WINDOW |
		PFD_SWAP_COPY      |             /* support swap copy */
		PFD_DOUBLEBUFFER   |             /* support double-buffering */
		PFD_TYPE_RGBA      |             /* color type */
		(stereoVisual ? PFD_STEREO : 0), /* support stereo */
		0,                               /* iPixelType */
		32,                              /* preferred color depth */
		0, 0, 0, 0, 0, 0,                /* color bits (ignored) */
		0,                               /* no alpha buffer */
		0,                               /* alpha bits (ignored) */
		0,                               /* no accumulation buffer */
		0, 0, 0, 0,                      /* accum bits (ignored) */
		24,                              /* depth buffer */
#ifdef GHOST_OPENGL_STENCIL
		8,                               /* no stencil buffer */
#else
		0,                               /* no stencil buffer */
#endif
		0,                               /* no auxiliary buffers */
		PFD_MAIN_PLANE,                  /* main layer */
		0,                               /* reserved */
		0, 0, 0                          /* no layer, visible, damage masks */
	};

	if (!init_wglew(hWnd, hDC, preferredPFD))
		fprintf(stderr, "WGLEW failed to initialize.\n");

	if (WGLEW_ARB_pixel_format) {
		return choose_pixel_format_arb(hDC, stereoVisual, numOfAASamples);
	}
	else {
		if (numOfAASamples > 0)
			return 0;

		return choose_pixel_format_legacy(hDC, preferredPFD);
	}
}



GHOST_TSuccess GHOST_ContextWGL::initializeDrawingContext(bool stereoVisual, GHOST_TUns16 numOfAASamples)
{
	HGLRC prevHGLRC = ::wglGetCurrentContext();
	WIN32_CHK(GetLastError() == NO_ERROR);

	HDC   prevHDC   = ::wglGetCurrentDC();
	WIN32_CHK(GetLastError() == NO_ERROR);

	if (m_needSetPixelFormat) {
		int iPixelFormat = choose_pixel_format(m_hWnd, m_hDC, stereoVisual, numOfAASamples);

		if (iPixelFormat == 0)
			goto error;

		PIXELFORMATDESCRIPTOR chosenPFD;

		int lastPFD = ::DescribePixelFormat(m_hDC, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &chosenPFD);

		if (!WIN32_CHK(lastPFD != 0))
			goto error;

		if (!WIN32_CHK(::SetPixelFormat(m_hDC, iPixelFormat, &chosenPFD)))
			goto error;

		m_needSetPixelFormat = false;
	}

	if (WGLEW_ARB_create_context) {
		std::vector<int> iAttributes;

		if (m_contextProfileMask != 0) {
			iAttributes.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
			iAttributes.push_back(m_contextProfileMask);
		}

		if (m_contextFlags != 0) {
			iAttributes.push_back(WGL_CONTEXT_FLAGS_ARB);
			iAttributes.push_back(m_contextFlags);
		}

		if (m_contextMajorVersion != 0) {
			iAttributes.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
			iAttributes.push_back(m_contextMajorVersion);
		}

		if (m_contextMinorVersion != 0) {
			iAttributes.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
			iAttributes.push_back(m_contextMinorVersion);
		}

		iAttributes.push_back(0);

		if (!singleContextMode || s_sharedHGLRC == NULL)
			m_hGLRC = ::wglCreateContextAttribsARB(m_hDC, NULL, &(iAttributes[0]));
		else
			m_hGLRC = s_sharedHGLRC;
	}
	else {
		if (m_contextProfileMask  != 0 ||
			m_contextFlags        != 0 ||
			m_contextMajorVersion != 0 ||
			m_contextMinorVersion != 0)
		{
			goto error;
		}

		if (!singleContextMode || s_sharedHGLRC == NULL)
			m_hGLRC = ::wglCreateContext(m_hDC);
		else
			m_hGLRC = s_sharedHGLRC;
	}

	if (!WIN32_CHK(m_hGLRC != NULL))
		goto error;

	if (s_sharedHGLRC == NULL) {
		s_sharedHGLRC = m_hGLRC;
		s_sharedHDC   = m_hDC;
	}

	s_sharedCount++;

	if (!singleContextMode && s_sharedHGLRC != m_hGLRC && !WIN32_CHK(::wglShareLists(s_sharedHGLRC, m_hGLRC)))
		goto error;

	if (!WIN32_CHK(::wglMakeCurrent(m_hDC, m_hGLRC)))
		goto error;

#ifndef NDEBUG
	const char* contextRenderer = (const char*)glGetString(GL_RENDERER);

	if (strcmp(extensionRenderer, contextRenderer) != 0) {
		fprintf(
			stderr,
			"WARNING! WGL extension renderer '%s' does not match current context's renderer '%s'\n",
			extensionRenderer,
			contextRenderer);

		abort();
	}
#endif

	return GHOST_kSuccess;

error:
	::wglMakeCurrent(prevHDC, prevHGLRC);

	return GHOST_kFailure;
}



GHOST_TSuccess GHOST_ContextWGL::releaseNativeHandles()
{
	GHOST_TSuccess success = m_hGLRC != s_sharedHGLRC || s_sharedCount == 1 ? GHOST_kSuccess : GHOST_kFailure;

	m_hWnd = NULL;
	m_hDC  = NULL;

	return success;
}
