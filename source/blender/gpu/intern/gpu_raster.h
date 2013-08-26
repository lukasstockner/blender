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

#include "BLI_sys_types.h" // for uint32_t



#ifdef __cplusplus
extern "C" {
#endif



typedef enum GPURasterShaderOption {
	GPU_RASTER_STIPPLE = (1<<0), /*  */
	GPU_RASTER_AA      = (1<<1), /*  */
	GPU_RASTER_POLYGON = (1<<2), /*  */

	GPU_RASTER_OPTIONS_NUM         = 3,
	GPU_RASTER_OPTION_COMBINATIONS = (1<<GPU_RASTER_OPTIONS_NUM)
} GPURasterShaderOption;



void GPU_raster_shader_init(void);
void GPU_raster_shader_exit(void);

void GPU_raster_begin(void);
void GPU_raster_end  (void);

void GPU_raster_shader_enable (uint32_t options);
void GPU_raster_shader_disable(uint32_t options);

void GPU_raster_shader_bind  (void);
void GPU_raster_shader_unbind(void);

void gpuPolygonStipple(const GLubyte* mask);

void gpuLineStipple(GLint factor, GLushort pattern);

void    gpuLineWidth(GLfloat width);
GLfloat gpuGetLineWidth(void);

void   gpuPolygonMode(GLenum mode);
GLenum gpuGetPolygonMode(void);



void gpu_init_stipple(void);



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
