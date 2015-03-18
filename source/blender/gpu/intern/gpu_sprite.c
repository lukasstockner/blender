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
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/intern/gpu_sprite.c
 *  \ingroup gpu
 */

#define GPU_MANGLE_DEPRECATED 0

#include "BLI_sys_types.h"

#include "GPU_aspect.h"
#include "GPU_aspect.h"
#include "GPU_immediate.h"
#include "GPU_debug.h"
#include "GPU_sprite.h"

#include "intern/gpu_private.h"

static float point_size = 1;

static float    SPRITE_SIZE    = 1;
static uint32_t SPRITE_OPTIONS = 0;

static bool SPRITE_BEGUN = false;

#if defined(WITH_GL_PROFILE_COMPAT)

static int pointhack = 0;

static GLubyte square_dot[16] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff
};

#endif



void gpu_sprite_init(void)
{
#if defined(WITH_GL_PROFILE_COMPAT) || defined(WITH_GL_PROFILE_CORE)
	glGetFloatv(GL_POINT_SIZE, &point_size);
#else
	point_size = 1;
#endif

	SPRITE_SIZE    = 1;
	SPRITE_OPTIONS = 0;
}



void gpu_sprite_exit(void)
{
#ifdef GPU_SAFETY
	SPRITE_BEGUN = false;
#endif
}



void gpu_sprite_enable(uint32_t options)
{
	SPRITE_OPTIONS |= options;
}



void gpu_sprite_disable(uint32_t options)
{
	SPRITE_OPTIONS &= ~options;
}



void gpu_sprite_bind(void)
{
	BLI_assert(SPRITE_BEGUN);

#if defined(WITH_GL_PROFILE_COMPAT)
	if (pointhack)
		return;

	GPU_ASSERT_NO_GL_ERRORS("gpu_sprite_bind start");

	if (SPRITE_SIZE != point_size)
		glPointSize(SPRITE_SIZE);

	if (SPRITE_OPTIONS & GPU_SPRITE_CIRCULAR)
		glEnable(GL_POINT_SMOOTH);

	if (SPRITE_OPTIONS & GPU_SPRITE_TEXTURE_2D)
		glEnable(GL_POINT_SPRITE);

	GPU_ASSERT_NO_GL_ERRORS("gpu_sprite_bind end");
#endif

	gpu_commit_matrix();
}



void gpu_sprite_unbind(void)
{
	BLI_assert(SPRITE_BEGUN);

#if defined(WITH_GL_PROFILE_COMPAT)
	if (pointhack)
		return;

	GPU_ASSERT_NO_GL_ERRORS("gpu_sprite_unbind start");

	glPointSize(point_size);
	glDisable(GL_POINT_SMOOTH);
	glDisable(GL_POINT_SPRITE);

	GPU_ASSERT_NO_GL_ERRORS("gpu_sprite_unbind end");
#endif
}



void GPU_point_size(float size)
{
	point_size = size;
	glPointSize(point_size);
}



void GPU_sprite_size(float size)
{
	SPRITE_SIZE = size;
}



void GPU_sprite_begin(void)
{
#ifdef GPU_SAFETY
	BLI_assert(!SPRITE_BEGUN);
	SPRITE_BEGUN = true;
#endif

	GPU_aspect_end(); /* assuming was GPU_ASPECT_BASIC */

	GPU_aspect_begin(GPU_ASPECT_SPRITE, NULL);

#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		GLfloat range[4];

		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_begin start");

		glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);

		if (range[1] < 2.0f) {
			GLfloat size[4];
			glGetFloatv(GL_POINT_SIZE, size);

			pointhack = floor(size[0] + 0.5f); // XXX jwilkins: roundf??

			if (pointhack > 4) { //-V112
				pointhack = 4; //-V112
			}
		}

		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_begin end");
	}
	else
#endif
	{
		gpuBegin(GL_POINTS);
	}
}



void GPU_sprite_3fv(const float vec[3])
{
	BLI_assert(SPRITE_BEGUN);

#if defined(WITH_GL_PROFILE_COMPAT)
	if (pointhack) {
		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_3fv start");

		glRasterPos3fv(vec);
		glBitmap(
			pointhack,
			pointhack,
			pointhack / 2.0f, pointhack / 2.0f,
			0,
			0,
			square_dot);

		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_3fv end");
	}
	else
#endif
	{
		gpuVertex3fv(vec);
	}
}



void GPU_sprite_3f(float x, float y, float z)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	if (pointhack) {
		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_3f start");

		glRasterPos3f(x, y, z);
		glBitmap(
			pointhack,
			pointhack,
			pointhack / 2.0f,
			pointhack / 2.0f,
			0,
			0,
			square_dot);

		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_3f end");
	}
	else
#endif
	{
		gpuVertex3f(x, y, z);
	}
}



void GPU_sprite_2f(float x, float y)
{
	BLI_assert(SPRITE_BEGUN);

#if defined(WITH_GL_PROFILE_COMPAT)
	if (pointhack) {
		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_2f start");

		glRasterPos2f(x, y);
		glBitmap(
			pointhack,
			pointhack,
			pointhack / 2.0f,
			pointhack / 2.0f,
			0,
			0,
			square_dot);

		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_2f end");
	}
	else
#endif
	{
		gpuVertex2f(x, y);
	}
}



void GPU_sprite_2fv(const float v[2])
{
	BLI_assert(SPRITE_BEGUN);

#if defined(WITH_GL_PROFILE_COMPAT)
	if (pointhack) {
		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_2fv start");

		glRasterPos2fv(v);
		glBitmap(
			pointhack,
			pointhack,
			pointhack / 2.0f,
			pointhack / 2.0f,
			0,
			0,
			square_dot);

		GPU_ASSERT_NO_GL_ERRORS("GPU_sprite_2f end");
	}
	else
#endif
	{
		gpuVertex2fv(v);
	}
}



void GPU_sprite_end(void)
{
#ifdef GPU_SAFETY
	BLI_assert(SPRITE_BEGUN);
#endif

#if defined(WITH_GL_PROFILE_COMPAT)
	if (pointhack) {
		pointhack = 0;
	}
	else
#endif
	{
		gpuEnd();
	}

	GPU_aspect_end();

#ifdef GPU_SAFETY
	SPRITE_BEGUN = false;
#endif

	GPU_aspect_begin(GPU_ASPECT_BASIC, NULL);
}
