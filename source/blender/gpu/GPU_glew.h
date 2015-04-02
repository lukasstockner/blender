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

/** \file source/blender/gpu/GPU_glew.h
 *  \ingroup gpu
 */

#ifndef __GPU_GLEW_H__
#define __GPU_GLEW_H__

#include "glew-mx.h"

#ifdef __APPLE__
/* Vertex Array Objects are part of OpenGL 3.0, or these extensions:
 *
 * GL_APPLE_vertex_array_object
 *  ^-- universally supported on Mac, widely supported on Linux (Mesa)
 * GL_ARB_vertex_array_object
 *  ^-- widely supported on Windows & vendors' Linux drivers
 *
 * The ARB extension differs from the APPLE one in that client
 * memory cannot be accessed through a non-zero vertex array object. It also
 * differs in that vertex array objects are explicitly not sharable between
 * contexts.
 * (in other words, the ARB version of VAOs *must* use VBOs for vertex data)
 *
 * Called and used the exact same way, so alias to unify our VAO code.
 *
 * Not needed for GL >= 3.0
 */

#  undef  glIsVertexArray
#  define glIsVertexArray glIsVertexArrayAPPLE

#  undef  glBindVertexArray
#  define glBindVertexArray glBindVertexArrayAPPLE

#  undef  glGenVertexArrays
#  define glGenVertexArrays glGenVertexArraysAPPLE

#  undef  glDeleteVertexArrays
#  define glDeleteVertexArrays glDeleteVertexArraysAPPLE

/* TODO: a better workaround? */

#endif /* __APPLE__ */

#endif /* __GPU_GLEW_H__ */
