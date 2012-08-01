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

/** \file gpu_lighting_glsl.c
 *  \ingroup gpu
 */

#include "gpu_lighting_internal.h"



void gpu_material_fv_glsl(GLenum face, GLenum pname, const GLfloat *params)
{
}



void gpu_material_i_glsl(GLenum face, GLenum pname, GLint param)
{
}



void gpu_get_material_fv_glsl(GLenum face, GLenum pname, GLfloat *params)
{
}



void gpu_color_material_glsl(GLenum face, GLenum mode)
{
}



void gpu_enable_color_material_glsl(void)
{
}



void gpu_disable_color_material_glsl(void)
{
}



void gpu_light_f_glsl(GLint light, GLenum pname, GLfloat param)
{
}



void gpu_light_fv_glsl(GLint light, GLenum pname, const GLfloat* params)
{
}



void gpu_enable_light_glsl(GLint light)
{
}



void gpu_disable_light_glsl(GLint light)
{
}



GLboolean gpu_is_light_enabled_glsl(GLint light)
{
	return GL_FALSE;
}



void gpu_light_model_i_glsl(GLenum pname, GLint param)
{
}



void gpu_light_model_fv_glsl(GLenum pname, const GLfloat* params)
{
}



void gpu_enable_lighting_glsl(void)
{
}



void gpu_disable_lighting_glsl(void)
{
}



GLboolean gpu_is_lighting_enabled_glsl(void)
{
	return GL_FALSE;
}
