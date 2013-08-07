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

/** \file blender/gpu/intern/gpu_pixels.h
 *  \ingroup gpu
 */

#ifndef __GPU_PIXELS_H__
#define __GPU_PIXELS_H__

#include "gpu_glew.h"



#ifdef __cplusplus
extern "C" {
#endif



typedef struct GPUbitmap {
	GLsizei        width, height;
	GLsizei        xorig, yorig;
	const GLubyte* bitmap;
} GPUbitmap;

typedef struct GPUpixels {
	GLsizei       width, height;
	GLenum        format;
	GLenum        type;
	const GLvoid* pixels;
} GPUpixels;



void gpuCacheBitmap(GPUbitmap* bitmap);
void gpuCachePixels(GPUpixels* pixels);

void gpuUncacheBitmap(GPUbitmap* bitmap);
void gpuUncachePixels(GPUbitmap* bitmap);

void gpuPixelZoom(GLfloat xfactor, GLfloat yfactor);
void gpuGetPixelZoom(GLfloat* xfactor_out, GLfloat *yfactor_out);

void gpuPixelFormat(GLenum pname, GLint param);

void gpuPixelUniform1f(GLenum pname, GLfloat param);

void gpuPixelsBegin();
void gpuPixelPos2f(GLfloat x, GLfloat y);
void gpuPixelPos3f(GLfloat x, GLfloat y, GLfloat z);
void gpuBitmap(GPUbitmap* bitmap);
void gpuPixels(GPUpixels* pixels);
void gpuPixelsEnd();

void GPU_pixels_shader_init  (void);
void GPU_pixels_shader_exit  (void);
void GPU_pixels_shader_bind  (void);
void GPU_pixels_shader_unbind(void);



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

#endif
