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

/** \file blender/gpu/intern/gpu_glew.h
*  \ingroup gpu
*/

#ifndef __GPU_GLEW_H__
#define __GPU_GLEW_H__



#include <GL/glew.h>



#if defined(GLEW_ES_ONLY)

// XXX jwilkins: need to check ALL of these to make sure you didn't cover an unguarded use of an extension/version

#ifndef GLEW_VERSION_1_1
#define GLEW_VERSION_1_1 0
#endif

#ifndef GLEW_VERSION_1_2
#define GLEW_VERSION_1_2 0
#endif

#ifndef GLEW_VERSION_1_3
#define GLEW_VERSION_1_3 0
#endif

#ifndef GLEW_VERSION_1_4
#define GLEW_VERSION_1_4 0
#endif

#ifndef GLEW_VERSION_1_5
#define GLEW_VERSION_1_5 0
#endif

#ifndef GLEW_VERSION_2_0
#define GLEW_VERSION_2_0 0
#endif

#ifndef GLEW_VERSION_3_0
#define GLEW_VERSION_3_0 0
#endif

#ifndef GLEW_ARB_shader_objects
#define GLEW_ARB_shader_objects 0
#endif

#ifndef GLEW_ARB_vertex_shader
#define GLEW_ARB_vertex_shader 0
#endif

#ifndef GLEW_ARB_vertex_program
#define GLEW_ARB_vertex_program 0
#endif

#ifndef GLEW_ARB_fragment_program
#define GLEW_ARB_fragment_program 0
#endif

#ifndef GLEW_ARB_vertex_buffer_object
#define GLEW_ARB_vertex_buffer_object 0
#endif

#ifndef GLEW_ARB_framebuffer_object
#define GLEW_ARB_framebuffer_object 0
#endif

#ifndef GLEW_ARB_multitexture
#define GLEW_ARB_multitexture 0
#endif

#ifndef GLEW_EXT_framebuffer_object
#define GLEW_EXT_framebuffer_object 0
#endif

#ifndef GLEW_ARB_depth_texture
#define GLEW_ARB_depth_texture 0
#endif

#ifndef GLEW_ARB_shadow
#define GLEW_ARB_shadow 0
#endif

#ifndef GL_ARB_texture_float
#define GL_ARB_texture_float 0
#endif

#ifndef GLEW_ARB_texture_non_power_of_two
#define GLEW_ARB_texture_non_power_of_two 0
#endif

#ifndef GLEW_ARB_texture3D
#define GLEW_ARB_texture3D 0
#endif

#ifndef GLEW_EXT_texture3D
#define GLEW_EXT_texture3D 0
#endif

#ifndef GLEW_ARB_texture_rg
#define GLEW_ARB_texture_rg 0
#endif

#ifndef GLEW_ARB_texture_query_lod
#define GLEW_ARB_texture_query_lod 0
#endif

#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D GL_TEXTURE_3D_OES
#endif

#ifndef glTexImage3D
#define glTexImage3D glTexImage3DOES
#endif

#ifndef glTexSubImage3D
#define glTexSubImage3D glTexSubImage3DOES
#endif

#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R GL_TEXTURE_WRAP_R_OES
#endif

#ifndef GL_TEXTURE_COMPARE_MODE
#define GL_TEXTURE_COMPARE_MODE GL_TEXTURE_COMPARE_MODE_EXT
#endif

#ifndef GL_COMPARE_REF_TO_TEXTURE
#define GL_COMPARE_REF_TO_TEXTURE GL_COMPARE_REF_TO_TEXTURE_EXT
#endif

#ifndef GL_TEXTURE_COMPARE_FUNC
#define GL_TEXTURE_COMPARE_FUNC GL_TEXTURE_COMPARE_FUNC_EXT
#endif

#ifndef GL_RGBA8
#define GL_RGBA8 GL_RGBA8_OES
#endif

#ifndef GL_RGBA16F
#define GL_RGBA16F GL_RGBA16F_EXT
#endif

#ifndef GL_RGB8
#define GL_RGB8 GL_RGB8_OES
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_FORMATS
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_OES
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_OES
#endif

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY GL_WRITE_ONLY_OES // XXX jwilkins: similar to GLdouble
#endif

#ifndef GLdouble
#define GLdouble double // XXX jwilkins: what to do about this?
#endif

#endif /* GLEW_ES_ONLY */



#if defined(GLEW_NO_ES)

#ifndef GL_OES_framebuffer_object
#define GLEW_OES_framebuffer_object 0
#endif

#ifndef GL_OES_mapbuffer
#define GLEW_OES_mapbuffer 0
#endif

#ifndef GL_OES_framebuffer_object
#define GLEW_OES_framebuffer_object 0
#endif

#ifndef GLEW_ES_VERSION_2_0
#define GLEW_ES_VERSION_2_0 0
#endif

#endif /* defined(GLEW_NO_ES) */



#endif /* GPU_GLEW_H */
