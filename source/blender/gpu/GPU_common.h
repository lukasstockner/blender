#ifndef _GPU_COMMON_H_
#define _GPU_COMMON_H_

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

/** \file blender/gpu/GPU_common.h
 *  \ingroup gpu
 */

#include "GPU_glew.h"



#ifdef __cplusplus
extern "C" {
#endif

#define GPU_MAX_COMMON_TEXCOORDS   1
#define GPU_MAX_COMMON_SAMPLERS    1
#define GPU_MAX_COMMON_LIGHTS      8
#define GPU_MAX_COMMON_CLIP_PLANES 6

/* for setting up the common vertex attributes */

void GPU_common_enable_vertex_array  (void);
void GPU_common_enable_normal_array  (void);
void GPU_common_enable_color_array   (void);
void GPU_common_enable_texcoord_array(void);

void GPU_common_disable_vertex_array  (void);
void GPU_common_disable_normal_array  (void);
void GPU_common_disable_color_array   (void);
void GPU_common_disable_texcoord_array(void);

void GPU_common_vertex_pointer  (GLint size, GLenum type, GLsizei stride,                       const GLvoid *pointer);
void GPU_common_normal_pointer  (            GLenum type, GLsizei stride, GLboolean normalized, const GLvoid *pointer);
void GPU_common_color_pointer   (GLint size, GLenum type, GLsizei stride,                       const GLvoid *pointer);
void GPU_common_texcoord_pointer(GLint size, GLenum type, GLsizei stride,                       const GLvoid *pointer);

void  GPU_set_common_active_texture(GLint texture);
GLint GPU_get_common_active_texture(void);

void GPU_common_normal_3fv(GLfloat n[3]);

void GPU_common_color_4ubv(GLubyte c[4]);
void GPU_common_color_4fv (GLfloat c[4]);



#ifdef __cplusplus
}
#endif

#endif /* _GPU_COMMON_H_ */
