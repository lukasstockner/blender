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

/** \file ghost/intern/GHOST_Context.cpp
 *  \ingroup GHOST
 * Definition of GHOST_Context class.
 */

#include "GHOST_Context.h"

#ifdef _WIN32
#  include <GL/wglew.h> // only for symbolic constants, do not use API functions
#  include <tchar.h>
#endif

#include <cstdio>
#include <cstring>



static const char* get_glew_error_message_string(GLenum error)
{
	switch (error) {
		case GLEW_OK: /* also GLEW_NO_ERROR */
			return "OK";

		case GLEW_ERROR_NO_GL_VERSION:
			return "Unable to determine GL version.";

		case GLEW_ERROR_GL_VERSION_10_ONLY:
			return "OpenGL 1.1 or later is required.";

		case GLEW_ERROR_GLX_VERSION_11_ONLY:
			return "GLX 1.2 or later is required.";

#ifdef WITH_GLEW_ES
		case GLEW_ERROR_NOT_GLES_VERSION:
			return "OpenGL ES is required.";

		case GLEW_ERROR_GLES_VERSION:
			return "A non-ES version of OpenGL is required.";

		case GLEW_ERROR_NO_EGL_VERSION:
			return "Unabled to determine EGL version.";

		case GLEW_ERROR_EGL_VERSION_10_ONLY:
			return "EGL 1.1 or later is required.";
#endif

		default:
			return NULL;
	}
}



static const char* get_glew_error_enum_string(GLenum error)
{
	switch (error) {
		_CASE_CODE_RETURN_STR(GLEW_OK) /* also GLEW_NO_ERROR */
		_CASE_CODE_RETURN_STR(GLEW_ERROR_NO_GL_VERSION)
		_CASE_CODE_RETURN_STR(GLEW_ERROR_GL_VERSION_10_ONLY)
		_CASE_CODE_RETURN_STR(GLEW_ERROR_GLX_VERSION_11_ONLY)
#ifdef WITH_GLEW_ES
		_CASE_CODE_RETURN_STR(GLEW_ERROR_NOT_GLES_VERSION)
		_CASE_CODE_RETURN_STR(GLEW_ERROR_GLES_VERSION)
		_CASE_CODE_RETURN_STR(GLEW_ERROR_NO_EGL_VERSION)
		_CASE_CODE_RETURN_STR(GLEW_ERROR_EGL_VERSION_10_ONLY)
#endif
		default:
			return NULL;
	}
}



GLenum glew_chk(GLenum error, const char* file, int line, const char* text)
{
	if (error != GLEW_OK) {
		const char* code = get_glew_error_enum_string(error);
		const char* msg  = get_glew_error_message_string(error);

#ifndef NDEBUG
		fprintf(
			stderr,
			"%s(%d):[%s] -> GLEW Error (0x%04X): %s: %s\n",
			file,
			line,
			text,
			error,
			code ? code : "<no symbol>",
			msg  ? msg  : "<no message>");
#else
		fprintf(
			stderr,
			"GLEW Error (%04X): %s: %s\n",
			error,
			code ? code : "<no symbol>",
			msg  ? msg  : "<no message>");
#endif
	}

	return error;
}



#ifdef _WIN32

bool win32_chk(bool result, const char* file, int line, const char* text)
{
	if (!result) {
		LPTSTR formattedMsg = NULL;

		DWORD error = GetLastError();

		const char* msg;

		DWORD count = 0;

		switch (error) {
			case ERROR_INVALID_VERSION_ARB:
				msg = "The specified OpenGL version and feature set are either invalid or not supported.\n";
				break;

			case ERROR_INVALID_PROFILE_ARB:
				msg = "The specified OpenGL profile and feature set are either invalid or not supported.\n";
				break;

			case ERROR_INVALID_PIXEL_TYPE_ARB:
				msg = "The specified pixel type is invalid.\n";
				break;

			case ERROR_INCOMPATIBLE_DEVICE_CONTEXTS_ARB:
				msg = "The device contexts specified are not compatible.  This can occur if the device contexts are managed by different drivers or possibly on different graphics adapters.\n";
				break;

#ifdef WITH_GLEW_ES
			case ERROR_INCOMPATIBLE_AFFINITY_MASKS_NV:
				msg = "The device context(s) and rendering context have non-matching affinity masks.\n";
				break;

			case ERROR_MISSING_AFFINITY_MASK_NV:
				msg = "The rendering context does not have an affinity mask set.\n";
				break;
#endif

			case ERROR_PROFILE_DOES_NOT_MATCH_DEVICE:
				msg = "The specified profile is intended for a device of a different type than the specified device.\n";
				break;

			default:
			{
				count =
					FormatMessage(
						(FORMAT_MESSAGE_ALLOCATE_BUFFER |
						 FORMAT_MESSAGE_FROM_SYSTEM     |
						 FORMAT_MESSAGE_IGNORE_INSERTS),
						NULL,
						error,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						(LPTSTR)(&formattedMsg),
						0,
						NULL);

				msg = count > 0 ? formattedMsg : "<no system message>\n";
				break;
			}
		}

#ifndef NDEBUG
		_ftprintf(
			stderr,
			"%s(%d):[%s] -> Win32 Error# (%d): %s",
			file, 
			line,
			text,
			error,
			msg);
#else
		_ftprintf(
			stderr,
			"Win32 Error# (%d): %s",
			error,
			msg);
#endif

		SetLastError(NO_ERROR);

		if (count != 0)
			LocalFree(formattedMsg);
	}

	return result;
}

#endif // _WIN32



void GHOST_Context::initContextGLEW()
{
	mxDestroyContext(m_glewContext); // no-op if m_glewContext is NULL

	mxSetContext(mxCreateContext());

	m_glewContext = mxGetContext();

	GLEW_CHK(glewInit());
}



void GHOST_Context::initClearGL()
{
	glClearColor(0.447, 0.447, 0.447, 0.000);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.000, 0.000, 0.000, 0.000);
}
