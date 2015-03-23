#ifndef _GPU_STATE_LATCH_H_
#define _GPU_STATE_LATCH_H_

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

/** \file blender/gpu/GPU_state_latch.h
 *  \ingroup gpu
 */

#include "GPU_glew.h"



#ifdef __cplusplus
extern "C" {
#endif

/* OpenGL ES does not have state query functions for some state.
 *
 * These functions latch the value passed to them,
 * so that the current value can be queried later by a distant piece of code.
 *
 * Of course, this requires disciplined use of the correct function, so
 * the functions these replace should be included in the gpu_deprecated.h header,
 * even if they aren't officially deprecated.
 *
 * If it makes sense to move one of these functions to another module, then do so;
 * they have nothing else in common besides the need for state query. */

/* the DepthRange state latch also covers the fact the ES 2.0 doesn't accept
 * GLdouble for depth range by doing a conversion. */
void gpuDepthRange(GLdouble near, GLdouble far);
void gpuGetDepthRange(GLdouble range[2]);

void gpuBindTexture(GLenum target, GLuint name);
GLuint gpuGetTextureBinding2D(void);

void gpuDepthMask(GLboolean flag);
GLboolean gpuGetDepthWriteMask(void);

void gpuViewport(int x, int y, int width, int height);
void gpuGetViewport(int out[4]);

/* feedback functions use the latched state to transform coordinates in the
 * same way as the legacy fixed function pipeline */
GLfloat GPU_feedback_depth_range(GLfloat z);
void GPU_feedback_viewport_2fv(GLfloat x, GLfloat y, GLfloat out[2]);

#ifdef __cplusplus
}
#endif

#endif
