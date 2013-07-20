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

/** \file blender/gpu/intern/gpu_raster.c
 *  \ingroup gpu
 */

#include "gpu_raster.h"



void GPU_init_raster(void)
{
}



void gpuEnablePolygonStipple(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glEnable(GL_POLYGON_STIPPLE);
#endif
}



void gpuPolygonStipple(const GLubyte* mask)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glPolygonStipple(mask);
#endif
}



void gpuDisablePolygonStipple(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glDisable(GL_POLYGON_STIPPLE);
#endif
}



void gpuEnableLineStipple(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glEnable(GL_LINE_STIPPLE);
#endif
}



void gpuLineStipple(GLint factor, GLushort pattern)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glLineStipple(factor, pattern);
#endif
}



void gpuDisableLineStipple(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glDisable(GL_LINE_STIPPLE);
#endif
}


static GLfloat line_width = 1;

void gpuLineWidth(GLfloat width)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glLineWidth(width);
#endif
}



GLfloat gpuGetLineWidth(void)
{
	return line_width;
}



static GLenum polygon_mode = GL_FILL;

void gpuPolygonMode(GLenum mode)
{
#if defined(WITH_GL_PROFILE_COMPAT)
#endif
}



GLenum gpuGetPolygonMode(void)
{
	return polygon_mode;
}



