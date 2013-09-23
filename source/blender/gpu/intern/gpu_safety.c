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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_safety.c
 *  \ingroup gpu
 */

/* my interface */
#include "GPU_safety.h"

#if GPU_SAFETY

#include "GPU_utility.h"

#include <stdio.h>



void gpu_check(const char* file, int line, const char* text)
{
	GLboolean no_gl_error   = GL_TRUE;
	GLenum    last_gl_error = GL_NO_ERROR;

	for (;;) {
		GLenum gl_error = glGetError();

		if (gl_error == GL_NO_ERROR) {
			break;
		}
		else {
			/* glGetError should have cleared the error flag, so if we get the
			   same flag twice that means glGetError itself probably triggered
			   the error. This happens on Windows if the GL context is invalid.
			 */
			BLI_assert(gl_error != last_gl_error);

			no_gl_error = GL_FALSE;

			if (text == NULL)
				fprintf(stderr, "%s(%d): GL Error (0x%04X - %s): %s\n", file, line, gl_error, gpuErrorSymbol(gl_error), gpuErrorString(gl_error));
			else
				fprintf(stderr, "%s(%d):[%s] -> GL Error (0x%04X - %s): %s\n", file, line, text, gl_error, gpuErrorSymbol(gl_error), gpuErrorString(gl_error));

			last_gl_error = gl_error;
		}
	}

	if (!no_gl_error)
		abort();
}

#endif
