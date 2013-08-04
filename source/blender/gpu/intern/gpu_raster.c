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

/* my interface */
#include "intern/gpu_raster.h"

/* internal */
#include "gpu_safety.h"



void GPU_init_raster(void)
{
	GPU_CHECK_NO_ERROR();
}



void gpu_init_stipple(void)
{
	int a, x, y;
	GLubyte pat[32*32];
	const GLubyte *patc= pat;

	a= 0;
	for (x=0; x<32; x++) {
		for (y=0; y<4; y++) {
			if ( (x) & 1) pat[a++]= 0x88;
			else pat[a++]= 0x22;
		}
	}
	
	gpuPolygonStipple(patc);
}



void gpuEnablePolygonStipple(void)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glEnable(GL_POLYGON_STIPPLE);
#endif
}



void gpuPolygonStipple(const GLubyte* mask)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glPolygonStipple(mask);
#endif
}



void gpuDisablePolygonStipple(void)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glDisable(GL_POLYGON_STIPPLE);
#endif
}



void gpuEnableLineStipple(void)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glEnable(GL_LINE_STIPPLE);
#endif
}



void gpuLineStipple(GLint factor, GLushort pattern)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glLineStipple(factor, pattern);
#endif
}



void gpuDisableLineStipple(void)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glDisable(GL_LINE_STIPPLE);
#endif
}



void gpuEnableLineSmooth(void)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glEnable(GL_LINE_SMOOTH);
#endif
}



void gpuDisableLineSmooth(void)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glDisable(GL_LINE_SMOOTH);
#endif
}



static GLfloat line_width = 1;

void gpuLineWidth(GLfloat width)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glLineWidth(width);
#endif
}



GLfloat gpuGetLineWidth(void)
{
	GPU_CHECK_NO_ERROR();

	return line_width;
}



static GLenum polygon_mode = GL_FILL;

void gpuPolygonMode(GLenum mode)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
#endif
}



GLenum gpuGetPolygonMode(void)
{
	GPU_CHECK_NO_ERROR();

	return polygon_mode;
}


