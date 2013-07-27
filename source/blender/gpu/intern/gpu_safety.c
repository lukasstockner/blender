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

#include "gpu_safety.h"

#if GPU_SAFETY

#include <stdlib.h>



bool gpu_check(const char* file, int line, const char* text)
{
	GLboolean no_error = GL_TRUE;
	int count = 0;

	for (;;) {
		GLenum error = glGetError();

		if (error == GL_NO_ERROR) {
			break;
		}
		else {
			no_error = GL_FALSE;
			fprintf(stderr, "%s(%d):[%s] -> GL Error (%04X): %s\n", file, line, text, error, gpuErrorString(error));
		}

		/* There should never be so many errors, but it can happen if there isn't a valid context. */
		if (++count > 20)
			abort();
	}

	return no_error;
}

#endif
