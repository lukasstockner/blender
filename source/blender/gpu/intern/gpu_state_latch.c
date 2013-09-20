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

/* my interface */
#include "GPU_state_latch.h"

/* my library */
#include "GPU_profile.h"
#include "GPU_safety.h"

/* standard */
#include <string.h>



// XXX jwilkins: this needs to be made to save these values from different contexts



static GLdouble depth_range[2] = { 0, 1 };

void gpuDepthRange(GLdouble near, GLdouble far)
{
	GPU_ASSERT(near != far);

	depth_range[0] = near;
	depth_range[1] = far;

#if !defined(GLEW_ES_ONLY)
	if (!GPU_PROFILE_ES20) {
		GPU_CHECK_NO_ERROR(glDepthRange(near, far));
		return;
	}
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
	memcpy(out, depth_range, sizeof(depth_range));
}



GLfloat gpuFeedbackDepthRange(GLfloat z)
{
	GLfloat depth;

	depth = depth_range[1] - depth_range[0];

	if (depth != 0) {
		return z / depth;
	}
	else {
		GPU_ABORT();
		return z;
	}
}



static GLuint texture_binding_2D = 0;

void gpuBindTexture(GLenum target, GLuint name)
{
	switch(target)
	{
		case GL_TEXTURE_2D:
			texture_binding_2D = name;
			break;

		default:
			/* a target we don't care to latch */
			break;
	}

	GPU_CHECK(glBindTexture(target, name));
}



GLuint gpuGetTextureBinding2D(void)
{
	return texture_binding_2D;
}



static GLboolean depth_writemask = GL_TRUE;

void gpuDepthMask(GLboolean flag)
{
	depth_writemask = flag;
	GPU_CHECK(glDepthMask(flag));
}



GLboolean gpuGetDepthWritemask(void)
{
	return depth_writemask;
}



static GLint viewport[4];

void gpuViewport(int x, int y, unsigned int width, unsigned int height)
{
	viewport[0] = x;
	viewport[1] = y;
	viewport[2] = width;
	viewport[3] = height;

	GPU_CHECK(glViewport(x, y, width, height));
}



void GPU_feedback_viewport_2fv(GLfloat x, GLfloat y, GLfloat out[2])
{
	const GLfloat halfw = (GLfloat)viewport[2] / 2.0f;
	const GLfloat halfh = (GLfloat)viewport[3] / 2.0f;

	out[0] = halfw*x + halfw + (GLfloat)viewport[0];
	out[1] = halfh*y + halfh + (GLfloat)viewport[1];
}


