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

/** \file blender/gpu/intern/gpu_utility.c
 *  \ingroup gpu
 */

#include "GPU_utility.h"



const char* gpuErrorString(GLenum err)
{
	switch(err) {
		case GL_NO_ERROR:
			return "No Error";

		case GL_INVALID_ENUM:
			return "Invalid Enum";

		case GL_INVALID_VALUE:
			return "Invalid Value";

		case GL_INVALID_OPERATION:
			return "Invalid Operation";

		case GL_STACK_OVERFLOW:
			return "Stack Overflow";

		case GL_STACK_UNDERFLOW:
			return "Stack Underflow";

		case GL_OUT_OF_MEMORY:
			return "Out of Memory";

#if GL_ARB_imagining
		case GL_TABLE_TOO_LARGE:
			return "Table Too Large";
#endif

#if defined(WITH_GLU)
		case GLU_INVALID_ENUM:
			return "Invalid Enum (GLU)";

		case GLU_INVALID_VALUE:
			return "Invalid Value (GLU)";

		case GLU_OUT_OF_MEMORY:
			return "Out of Memory (GLU)";
#endif

		default:
			return "<unknown error>";
	}
}
