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

/** \file blender/gpu/intern/gpu_aspectfuncs.c
*  \ingroup gpu
*/

#define GPU_ASPECT_INTERN
#include "gpu_aspectfuncs.h"

#include "gpu_object_gles.h"
#include "gpu_extension_wrapper.h"
#include "gpu_profile.h"



static GLboolean begin_font(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
		gpu_set_shader_es(&shader_alphatexture_info, 0);
		gpu_glUseProgram(shader_alphatexture);
	}
#endif

	return GL_TRUE;
}

static GLboolean end_font(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
		gpu_set_shader_es(&shader_main_info, 0);
		gpu_glUseProgram(shader_main);
	}
#endif

	return GL_TRUE;
}

GPUaspectfuncs GPU_ASPECTFUNCS_FONT = { begin_font, end_font };



static GLboolean begin_texture(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
		gpu_set_shader_es(&shader_rgbatexture_info, 0);
		gpu_glUseProgram(shader_rgbatexture);
	}
#endif

	return GL_TRUE;
}

static GLboolean end_texture(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
		gpu_set_shader_es(&shader_main_info, 0);
		gpu_glUseProgram(shader_main);
	}
#endif

	return GL_TRUE;
}

GPUaspectfuncs GPU_ASPECTFUNCS_TEXTURE = { begin_texture, end_texture };



static GLboolean begin_pixels(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
		gpu_set_shader_es(&shader_pixels_info, 0);
		gpu_glUseProgram(shader_pixels);
	}
#endif

	return GL_TRUE;
}

static GLboolean end_pixels(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
		gpu_set_shader_es(&shader_main_info, 0);
		gpu_glUseProgram(shader_main);
	}
#endif

	return GL_TRUE;
}

GPUaspectfuncs GPU_ASPECTFUNCS_PIXELS = { begin_pixels, end_pixels };



void gpuInitializeAspectFuncs()
{
	gpuGenAspects(1, &GPU_ASPECT_FONT);
	gpuGenAspects(1, &GPU_ASPECT_TEXTURE);
	gpuGenAspects(1, &GPU_ASPECT_PIXELS);

	gpuAspectFuncs(GPU_ASPECT_FONT,    &GPU_ASPECTFUNCS_FONT);
	gpuAspectFuncs(GPU_ASPECT_TEXTURE, &GPU_ASPECTFUNCS_TEXTURE);
	gpuAspectFuncs(GPU_ASPECT_PIXELS,  &GPU_ASPECTFUNCS_PIXELS);
}



void gpuShutdownAspectFuncs()
{
	gpuDeleteAspects(1, &GPU_ASPECT_FONT);
	gpuDeleteAspects(1, &GPU_ASPECT_TEXTURE);
	gpuDeleteAspects(1, &GPU_ASPECT_PIXELS);
}
