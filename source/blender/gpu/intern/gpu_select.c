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
 * Contributor(s): Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/intern/gpu_select.c
 *  \ingroup gpu
 */

#include "GPU_select.h"



void GPU_select_buffer(GLsizei size, GLuint* buffer)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glSelectBuffer(size, buffer);
#endif
}



void GPU_select_begin(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glRenderMode(GL_SELECT);
#endif
}



GLsizei GPU_select_end(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	return glRenderMode(GL_RENDER);
#else
	return 0;
#endif
}



void GPU_select_clear(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glInitNames();
#endif
}



void GPU_select_pop(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glPopName();
#endif
}



void GPU_select_push(GLuint name)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glPushName(name);
#endif
}



void GPU_select_load(GLuint name)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glLoadName(name);
#endif
}
