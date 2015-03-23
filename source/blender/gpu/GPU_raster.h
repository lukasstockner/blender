#ifndef _GPU_RASTER_H_
#define _GPU_RASTER_H_

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

/** \file source/blender/gpu/GPU_raster.h
 *  \ingroup gpu
 */

#include "GPU_glew.h"



#ifdef __cplusplus
extern "C" {
#endif



/* OpenGL stipple defines */
extern const GLubyte GPU_stipple_halftone        [128];
extern const GLubyte GPU_stipple_quarttone       [128];
extern const GLubyte GPU_stipple_diag_stripes_pos[128];
extern const GLubyte GPU_stipple_diag_stripes_neg[128];
extern const GLubyte stipple_checker_8px         [128];



typedef enum GPURasterShaderOption {
	GPU_RASTER_STIPPLE = (1<<0), /* polygon or line stippling */
	GPU_RASTER_AA      = (1<<1), /* anti-aliasing             */
	GPU_RASTER_POLYGON = (1<<2), /* choose polygon or line    */

	GPU_RASTER_OPTIONS_NUM         = 3,
	GPU_RASTER_OPTION_COMBINATIONS = (1<<GPU_RASTER_OPTIONS_NUM)
} GPURasterShaderOption;



void GPU_raster_begin(void);
void GPU_raster_end(void);

void GPU_raster_set_line_style(int factor);

void gpuPolygonStipple(const GLubyte *mask);

void gpuLineStipple(GLint factor, GLushort pattern);

void gpuLineWidth(GLfloat width);
GLfloat gpuGetLineWidth(void);

void gpuPolygonMode(GLenum mode);
GLenum gpuGetPolygonMode(void);



#if defined(GLEW_ES_ONLY)

/* ES 2.0 doesn't define LINE and FILL, but the immediate mode replacement library emulates PolygonMode
 * (GL core has deprecated PolygonMode, but it should still be in the header) */

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

#endif /* _GPU_RASTER_H_ */
