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

/** \file gpu_lighting.c
 *  \ingroup gpu
 */

#if defined(WITH_GL_PROFILE_COMPAT)

#include "gpu_lighting_internal.h"


void gpu_material_fv_gl11(GLenum face, GLenum pname, const GLfloat *params)
{
	glMaterialfv(face, pname, params);
}



void gpu_material_i_gl11(GLenum face, GLenum pname, GLint param)
{
	glMateriali(face, pname, param);
}



void gpu_get_material_fv_gl11(GLenum face, GLenum pname, GLfloat *params)
{
	glGetMaterialfv(face, pname, params);
}



void gpu_color_material_gl11(GLenum face, GLenum mode)
{
	glColorMaterial(face, mode);
}



void gpu_enable_color_material_gl11(void)
{
	glEnable(GL_COLOR_MATERIAL);
}



void gpu_disable_color_material_gl11(void)
{
	glDisable(GL_COLOR_MATERIAL);
}



void gpu_light_f_gl11(GLint light, GLenum pname, GLfloat param)
{
	glLightf(GL_LIGHT0 + light, pname, param);
}



void gpu_light_fv_gl11(GLint light, GLenum pname, const GLfloat* params)
{
	glLightfv(GL_LIGHT0 + light, pname, params);
}



void gpu_enable_light_gl11(GLint light)
{
	glEnable(GL_LIGHT0 + light);
}



void gpu_disable_light_gl11(GLint light)
{
	glEnable(GL_LIGHT0 + light);
}



GLboolean gpu_is_light_enabled_gl11(GLint light)
{
	return glIsEnabled(GL_LIGHT0 + light);
}



void gpu_light_model_i_gl11(GLenum pname, GLint param)
{
	glLightModeli(pname, param);
}



void gpu_light_model_fv_gl11(GLenum pname, const GLfloat* params)
{
	glLightModelfv(pname, params);
}



void gpu_enable_lighting_gl11(void)
{
	glEnable(GL_LIGHTING);
}



void gpu_disable_lighting_gl11(void)
{
	glDisable(GL_LIGHTING);
}



GLboolean gpu_is_lighting_enabled_gl11(void)
{
	return glIsEnabled(GL_LIGHTING);
}



#endif
