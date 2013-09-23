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

/* my interface */
#include "intern/gpu_select_intern.h"

/* my library */
#include "GPU_safety.h"

/* internal */
#include "intern/gpu_matrix_intern.h"
#include "intern/gpu_aspect_intern.h"



static bool IS_SELECT_MODE = false;



void gpu_select_init(void)
{
	IS_SELECT_MODE = false;
}



void gpu_select_exit(void)
{
}



bool gpu_default_select_begin(void* UNUSED(param), const void* UNUSED(object))
{
#if defined(WITH_GPU_PROFILE_COMPAT)
	return true; /* nothing to do, allow this pass to start */
#else
	return false; /* not implemented, so cancel this pass before it starts if possible */
#endif
}



bool gpu_default_select_end(void* UNUSED(param), const void* UNUSED(object))
{
	return true; /* only one pass, 'true' means 'done' */
}



bool gpu_default_select_commit(void* UNUSED(param))
{
#if defined(WITH_GPU_PROFILE_COMPAT)
	gpu_set_common(NULL);
	gpu_glUseProgram(0);
	gpu_commit_matrix();

	return true;
#else
	return false; /* cancel drawing, since select mode isn't implemented */
#endif
}



bool gpu_is_select_mode(void)
{
	return IS_SELECT_MODE;
}



void GPU_select_buffer(GLsizei size, GLuint* buffer)
{
	GPU_ASSERT(!IS_SELECT_MODE);

	if (!IS_SELECT_MODE) {
#if defined(WITH_GL_PROFILE_COMPAT)
		glSelectBuffer(size, buffer);
#endif
	}
}



void GPU_select_begin(void)
{
	GPU_ASSERT(!gpu_aspect_active());
	GPU_ASSERT(!IS_SELECT_MODE);

	IS_SELECT_MODE = true;

#if defined(WITH_GL_PROFILE_COMPAT)
	glRenderMode(GL_SELECT);
#endif
}



GLsizei GPU_select_end(void)
{
	GPU_ASSERT(!gpu_aspect_active());
	GPU_ASSERT(IS_SELECT_MODE);

	IS_SELECT_MODE = false;

#if defined(WITH_GL_PROFILE_COMPAT)
	return glRenderMode(GL_RENDER);
#else
	return 0;
#endif
}



void GPU_select_clear(void)
{
	if (IS_SELECT_MODE) {
#if defined(WITH_GL_PROFILE_COMPAT)
		glInitNames();
#endif
	}
}



void GPU_select_pop(void)
{
	if (IS_SELECT_MODE) {
#if defined(WITH_GL_PROFILE_COMPAT)
		glPopName();
#endif
	}
}



void GPU_select_push(GLuint name)
{
	if (IS_SELECT_MODE) {
#if defined(WITH_GL_PROFILE_COMPAT)
		glPushName(name);
#endif
	}
}



void GPU_select_load(GLuint name)
{
	if (IS_SELECT_MODE) {
#if defined(WITH_GL_PROFILE_COMPAT)
		glLoadName(name);
#endif
	}
}
