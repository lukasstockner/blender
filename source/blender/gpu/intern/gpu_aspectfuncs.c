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
#include "intern/gpu_aspectfuncs.h"

#include "intern/gpu_object_gles.h"
#include "intern/gpu_extension_wrapper.h"
#include "intern/gpu_profile.h"

#include "GPU_simple_shader.h"



static GLboolean begin_font(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
	GPU_font_shader_bind();

	//#if defined(WITH_GL_PROFILE_CORE)
//	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
//		gpu_set_shader_es(&shader_redtexture_info, 0);
//		gpu_glUseProgram(shader_redtexture);
//	}
//#endif
//
//#if defined(WITH_GL_PROFILE_ES20)
//	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
//		gpu_set_shader_es(&shader_alphatexture_info, 0);
//		gpu_glUseProgram(shader_alphatexture);
//	}
//#endif

	return GL_TRUE;
}

static GLboolean end_font(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
	GPU_font_shader_unbind();

//#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
//	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
//		gpu_set_shader_es(&shader_main_info, 0);
//		gpu_glUseProgram(shader_main);
//	}
//#endif

	return GL_TRUE;
}

GPUaspectfuncs GPU_ASPECTFUNCS_FONT = { begin_font, end_font };



static GLboolean begin_texture(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
	GPU_simple_shader_bind(GPU_SHADER_TEXTURE_2D);

//#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
//	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
//		gpu_set_shader_es(&shader_rgbatexture_info, 0);
//		gpu_glUseProgram(shader_rgbatexture);
//	}
//#endif

	return GL_TRUE;
}

static GLboolean end_texture(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
	GPU_simple_shader_unbind();

//#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
//	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
//		gpu_set_shader_es(&shader_main_info, 0);
//		gpu_glUseProgram(shader_main);
//	}
//#endif

	return GL_TRUE;
}

GPUaspectfuncs GPU_ASPECTFUNCS_TEXTURE = { begin_texture, end_texture };



static GLboolean begin_pixels(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
	GPU_simple_shader_bind(GPU_SHADER_TEXTURE_2D);

//#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
//	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
//		gpu_set_shader_es(&shader_pixels_info, 0);
//		gpu_glUseProgram(shader_pixels);
//	}
//#endif

	return GL_TRUE;
}

static GLboolean end_pixels(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
	GPU_simple_shader_unbind();

//#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
//	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
//		gpu_set_shader_es(&shader_main_info, 0);
//		gpu_glUseProgram(shader_main);
//	}
//#endif

	return GL_TRUE;
}

GPUaspectfuncs GPU_ASPECTFUNCS_PIXELS = { begin_pixels, end_pixels };



static GLboolean begin_simple_shader(GLvoid* UNUSED(param), const GLvoid* object)
{
	uint32_t options = GET_UINT_FROM_POINTER(object);
	GPU_simple_shader_bind(options);
	return GL_TRUE;
}

static GLboolean end_simple_shader(GLvoid* UNUSED(param), const GLvoid* UNUSED(object))
{
	GPU_simple_shader_unbind();
	return GL_TRUE;
}

GPUaspectfuncs GPU_ASPECTFUNCS_SIMPLE_SHADER = { begin_simple_shader, end_simple_shader };



void gpuInitializeAspectFuncs()
{
	gpuGenAspects(1, &GPU_ASPECT_FONT);
	gpuGenAspects(1, &GPU_ASPECT_TEXTURE);
	gpuGenAspects(1, &GPU_ASPECT_PIXELS);
	gpuGenAspects(1, &GPU_ASPECT_SIMPLE_SHADER);

	gpuAspectFuncs(GPU_ASPECT_FONT,          &GPU_ASPECTFUNCS_FONT);
	gpuAspectFuncs(GPU_ASPECT_TEXTURE,       &GPU_ASPECTFUNCS_TEXTURE);
	gpuAspectFuncs(GPU_ASPECT_PIXELS,        &GPU_ASPECTFUNCS_PIXELS);
	gpuAspectFuncs(GPU_ASPECT_SIMPLE_SHADER, &GPU_ASPECTFUNCS_SIMPLE_SHADER);
}



void gpuShutdownAspectFuncs()
{
	gpuDeleteAspects(1, &GPU_ASPECT_FONT);
	gpuDeleteAspects(1, &GPU_ASPECT_TEXTURE);
	gpuDeleteAspects(1, &GPU_ASPECT_PIXELS);
	gpuDeleteAspects(1, &GPU_ASPECT_SIMPLE_SHADER);
}
