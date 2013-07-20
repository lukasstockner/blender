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

/** \file blender/gpu/intern/gpu_raster.h
 *  \ingroup gpu
 */

#ifndef __GPU_RASTER_H__
#define __GPU_RASTER_H__

#include "gpu_glew.h"



#ifdef __cplusplus
extern "C" {
#endif



void GPU_init_raster(void);

void gpuEnablePolygonStipple(void);
void gpuPolygonStipple(const GLubyte* mask);
void gpuDisablePolygonStipple(void);

void gpuEnableLineStipple(void);
void gpuLineStipple(GLint factor, GLushort pattern);
void gpuDisableLineStipple(void);

void gpuLineWidth(GLfloat width);
GLfloat gpuGetLineWidth(void);

void gpuPolygonMode(GLenum mode);
GLenum gpuGetPolygonMode(void);



#if defined(GLEW_ES_ONLY)

/* ES 2.0 doesn't define LINE and FILL, but the immediate mode replacement library emulates PolygonMode */
/* (GL core has deprecated PolygonMode, but it should still be in the header) */

#ifndef GL_LINE
#define GL_LINE 0x1B01
#endif

#ifndef GL_FILL
#define GL_FILL 0x1B02
#endif

#endif



#ifdef __cplusplus
}
#endif

#endif
