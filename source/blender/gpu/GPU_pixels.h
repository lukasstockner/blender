#ifndef _GPU_PIXELS_H_
#define _GPU_PIXELS_H_

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

/** \file blender/gpu/GPU_pixels.h
 *  \ingroup gpu
 */

#include "GPU_glew.h"



#ifdef __cplusplus
extern "C" {
#endif



typedef struct GPUbitmap {
	GLsizei width, height;
	GLsizei xorig, yorig;
	const GLubyte *bitmap;
} GPUbitmap;

typedef struct GPUpixels {
	GLsizei width, height;
	GLenum format;
	GLenum type;
	const GLvoid *pixels;
} GPUpixels;



void GPU_bitmap_cache(GPUbitmap *bitmap);
void GPU_pixels_cache(GPUpixels *pixels);

void GPU_bitmap_uncache(GPUbitmap *bitmap);
void GPU_pixels_uncache(GPUpixels *pixels);

void GPU_pixels_zoom(GLfloat xfactor, GLfloat yfactor);
void GPU_get_pixels_zoom(GLfloat *xfactor_out, GLfloat *yfactor_out);

void GPU_pixels_format(GLenum pname, GLint param);

void GPU_pixels_uniform_1f(GLenum pname, GLfloat param);

void GPU_pixels_pos_2f(GLfloat x, GLfloat y);
void GPU_pixels_pos_3f(GLfloat x, GLfloat y, GLfloat z);

void GPU_bitmap(GPUbitmap *bitmap);
void GPU_pixels(GPUpixels *pixels);

void GPU_pixels_begin(void);
void GPU_pixels_end(void);



#if defined(GLEW_ES_ONLY)

/* ES 2.0 doesn't define pixel transfer pnames, but the draw pixel replacement library emulates them */

#define GL_RED_SCALE   0x0D14
#define GL_RED_BIAS    0x0D15
#define GL_ZOOM_X      0x0D16
#define GL_ZOOM_Y      0x0D17
#define GL_GREEN_SCALE 0x0D18
#define GL_GREEN_BIAS  0x0D19
#define GL_BLUE_SCALE  0x0D1A
#define GL_BLUE_BIAS   0x0D1B
#define GL_ALPHA_SCALE 0x0D1C
#define GL_ALPHA_BIAS  0x0D1D

#endif



#if defined(GLEW_ES_ONLY)

/* ES 2.0 doesn't define all pixel unpack parameters, but the drawPixel replacement library emulates them */

#ifndef GL_UNPACK_SWAP_BYTES
#define GL_UNPACK_SWAP_BYTES 0x0CF0
#endif

#endif



#ifdef __cplusplus
}
#endif

#endif /* _GPU_PIXELS_H_ */
