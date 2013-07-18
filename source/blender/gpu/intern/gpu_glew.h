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



#if !defined(WITH_GLEW_ES)

#if !GL_OES_framebuffer_object
#define GLEW_OES_framebuffer_object 0
#define glGenerateMipmapOES glGenerateMipmap
#endif

#if !GL_OES_mapbuffer
#define GLEW_OES_mapbuffer 0
#define glMapBufferOES glMapBuffer
#define glUnmapBufferOES glUnmapBuffer
#endif

#if !GL_OES_framebuffer_object
#define GLEW_OES_framebuffer_object 0
#define glGenFramebuffersOES glGenFramebuffers
#define glBindFramebufferOES glBindFramebuffer
#define glDeleteFramebuffersOES glDeleteFramebuffers
#endif

#define GLEW_ES_VERSION_2_0 0

#endif



#endif /* GPU_GLEW_H */
