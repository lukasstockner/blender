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

/** \file blender/gpu/intern/gpu_state_latch.c
 *  \ingroup gpu
 */

#define GPU_MANGLE_DEPRECATED 0

#include "GPU_debug.h"
#include "GPU_state_latch.h"

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "gpu_private.h"

#include "BLI_math_vector.h"

#include <string.h>



// XXX jwilkins: this needs to be made to save these values from different contexts

static bool      depth_range_valid = false;
static GLdouble  depth_range[2];

static bool      viewport_valid = false;
static GLint     viewport[4];

static bool      texture_binding_2D_valid = false;
static GLuint    texture_binding_2D;

static bool      depth_writemask_valid = false;
static GLboolean depth_writemask;



void gpu_state_latch_init(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	GLint value;

	glGetDoublev(GL_DEPTH_RANGE, depth_range);
	depth_range_valid = true;

	glGetIntegerv(GL_VIEWPORT, viewport);
	viewport_valid = true;

	value = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
	texture_binding_2D       = (GLuint)value;
	texture_binding_2D_valid = true;

	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_writemask);
	depth_writemask_valid    = true;
#else
	depth_range_valid        = false;
	viewport_valid           = false;
	texture_binding_2D_valid = false;
	depth_writemask_valid    = false;
#endif
}



void gpu_state_latch_exit(void)
{
}



void gpuDepthRange(GLdouble near, GLdouble far)
{
	BLI_assert(near != far);

	copy_v2_db2(depth_range, near, far);

	depth_range_valid = true;

#if !defined(GLEW_ES_ONLY)
//	if (!GPU_PROFILE_ES20) {
		GPU_CHECK_ERRORS_AROUND(glDepthRange(near, far));
		return;
//	}
#endif

#if !defined(GLEW_NO_ES)
	if (GPU_PROFILE_ES20) {
		GPU_CHECK(glDepthRangef((GLfloat)near, (GLfloat)far));
		return;
	}
#endif
}



void gpuGetDepthRange(GLdouble out[2])
{
	BLI_assert(depth_range_valid);

	VECCOPY2D(out, depth_range);
}



GLfloat GPU_feedback_depth_range(GLfloat z)
{
	GLfloat depth;

	BLI_assert(depth_range_valid);

	depth = depth_range[1] - depth_range[0];

	if (depth != 0) {
		return z / depth;
	}
	else {
		BLI_assert(0);
		return z;
	}
}



void gpuBindTexture(GLenum target, GLuint name)
{
	switch(target)
	{
		case GL_TEXTURE_2D:
			texture_binding_2D       = name;
			texture_binding_2D_valid = true;
			break;

		default:
			/* a target we don't care to latch */
			break;
	}

	GPU_CHECK_ERRORS_AROUND(glBindTexture(target, name));
}



GLuint gpuGetTextureBinding2D(void)
{
	BLI_assert(texture_binding_2D_valid);

	return texture_binding_2D;
}



void gpuDepthMask(GLboolean flag)
{
	depth_writemask       = flag;
	depth_writemask_valid = true;

	GPU_CHECK_ERRORS_AROUND(glDepthMask(flag));
}



GLboolean gpuGetDepthWriteMask(void)
{
	BLI_assert(depth_writemask_valid);

	return depth_writemask;
}



void gpuViewport(int x, int y, int width, int height)
{
	viewport[0] = x;
	viewport[1] = y;
	viewport[2] = width;
	viewport[3] = height;

	viewport_valid = true;

	GPU_CHECK_ERRORS_AROUND(glViewport(x, y, width, height));
}



void gpuGetViewport(int out[4])
{
	BLI_assert(viewport_valid);

	copy_v4_v4_int(out, viewport);
}



void GPU_feedback_viewport_2fv(GLfloat x, GLfloat y, GLfloat out[2])
{
	const GLfloat halfw = (GLfloat)viewport[2] / 2.0f;
	const GLfloat halfh = (GLfloat)viewport[3] / 2.0f;

	BLI_assert(viewport_valid);

	out[0] = halfw*x + halfw + (GLfloat)viewport[0];
	out[1] = halfh*y + halfh + (GLfloat)viewport[1];
}
