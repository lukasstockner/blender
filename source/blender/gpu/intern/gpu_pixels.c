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

#include "gpu_pixels.h"

#include "gpu_profile.h"
#include "gpu_safety.h"



void gpuCacheBitmap(GPUbitmap* bitmap)
{
}



void gpuCachePixels(GPUpixels* pixels)
{
}



void gpuUncacheBitmap(GPUbitmap* bitmap)
{
}



void gpuUncachePixels(GPUbitmap* bitmap)
{
}



static GLuint non_default_flags = 0;

#define NON_DEFAULT_FACTOR            (1 <<  0)
#define NON_DEFAULT_RED_SCALE         (1 <<  1)
#define NON_DEFAULT_RED_BIAS          (1 <<  2)
#define NON_DEFAULT_GREEN_SCALE       (1 <<  3)
#define NON_DEFAULT_GREEN_BIAS        (1 <<  4)
#define NON_DEFAULT_BLUE_SCALE        (1 <<  5)
#define NON_DEFAULT_BLUE_BIAS         (1 <<  6)
#define NON_DEFAULT_ALPHA_SCALE       (1 <<  7)
#define NON_DEFAULT_ALPHA_BIAS        (1 <<  8)
#define NON_DEFAULT_UNPACK_ROW_LENGTH (1 <<  9)
#define NON_DEFAULT_UNPACK_SWAP_BYTES (1 << 10)
#define NON_DEFAULT_UNPACK_ALIGNMENT  (1 << 11)



static GLint     format_unpack_row_length = 0;
static GLboolean format_unpack_swap_bytes = GL_FALSE;
static GLint     format_unpack_alignment  = 4;

void gpuPixelFormat(GLenum pname, GLint param)
{
	switch(pname) {
	case GL_UNPACK_ROW_LENGTH:
		format_unpack_row_length = param;
		if (param == 0) 
			non_default_flags ^= NON_DEFAULT_UNPACK_ROW_LENGTH;
		else
			non_default_flags |= NON_DEFAULT_UNPACK_ROW_LENGTH;

		break;

	case GL_UNPACK_SWAP_BYTES:
		format_unpack_swap_bytes = param;
		if (param == 0) 
			non_default_flags ^= NON_DEFAULT_UNPACK_SWAP_BYTES;
		else
			non_default_flags |= NON_DEFAULT_UNPACK_SWAP_BYTES;

		break;

	case GL_UNPACK_ALIGNMENT:
		format_unpack_alignment = param;
		if (param == 0) 
			non_default_flags ^= NON_DEFAULT_UNPACK_ALIGNMENT;
		else
			non_default_flags |= NON_DEFAULT_UNPACK_ALIGNMENT;

		break;

		default:
			GPU_ABORT();
			break;
	}
}



static GLfloat pixel_zoom_xfactor = 1;
static GLfloat pixel_zoom_yfactor = 1;

void gpuPixelZoom(GLfloat xfactor, GLfloat yfactor)
{
	pixel_zoom_xfactor = xfactor;
	pixel_zoom_yfactor = yfactor;

	if (xfactor == 1 || yfactor == 1)
		non_default_flags ^= NON_DEFAULT_FACTOR;
	else
		non_default_flags |= NON_DEFAULT_FACTOR;
}



void gpuGetPixelZoom(GLfloat* xfactor_out, GLfloat *yfactor_out)
{
	*xfactor_out = pixel_zoom_xfactor;
	*yfactor_out = pixel_zoom_yfactor;
}



static GLfloat pixel_red_scale   = 1;
static GLfloat pixel_red_bias    = 0;
static GLfloat pixel_green_scale = 1;
static GLfloat pixel_green_bias  = 0;
static GLfloat pixel_blue_scale  = 1;
static GLfloat pixel_blue_bias   = 0;
static GLfloat pixel_alpha_scale = 1;
static GLfloat pixel_alpha_bias  = 0;

// XXX jwilkins: this would be a lot shorter if you made a table

void gpuPixelUniform1f(GLenum pname, GLfloat param)
{
	switch(pname) {
		case GL_RED_SCALE:
			pixel_red_scale = param;

			if (param == 1) 
				non_default_flags ^= NON_DEFAULT_RED_SCALE;
			else 
				non_default_flags |= NON_DEFAULT_RED_SCALE;

			break;

		case GL_RED_BIAS:
			pixel_red_bias = param;

			if (param != 0) 
				non_default_flags ^= NON_DEFAULT_RED_BIAS;
			else 
				non_default_flags |= NON_DEFAULT_RED_BIAS;

			break;

		case GL_BLUE_SCALE:
			pixel_blue_scale = param;

			if (param != 1) 
				non_default_flags ^= NON_DEFAULT_BLUE_SCALE;
			else 
				non_default_flags |= NON_DEFAULT_BLUE_SCALE;

			break;

		case GL_BLUE_BIAS:
			pixel_blue_bias = param;

			if (param != 0) 
				non_default_flags ^= NON_DEFAULT_BLUE_BIAS;
			else 
				non_default_flags |= NON_DEFAULT_BLUE_BIAS;

			break;

		case GL_GREEN_SCALE:
			pixel_green_scale = param;

			if (param != 1) 
				non_default_flags ^= NON_DEFAULT_GREEN_SCALE;
			else 
				non_default_flags |= NON_DEFAULT_GREEN_SCALE;

			break;

		case GL_GREEN_BIAS:
			pixel_green_bias = param;

			if (param != 0) 
				non_default_flags ^= NON_DEFAULT_GREEN_BIAS;
			else 
				non_default_flags |= NON_DEFAULT_GREEN_BIAS;

			break;

		case GL_ALPHA_SCALE:
			pixel_alpha_scale = param;

			if (param != 1) 
				non_default_flags ^= NON_DEFAULT_ALPHA_SCALE;
			else 
				non_default_flags |= NON_DEFAULT_ALPHA_SCALE;

			break;

		case GL_ALPHA_BIAS:
			pixel_alpha_bias = param;

			if (param != 0) 
				non_default_flags ^= NON_DEFAULT_ALPHA_BIAS;
			else 
				non_default_flags |= NON_DEFAULT_ALPHA_BIAS;

			break;

		default:
			GPU_ABORT();
			break;
	}
}



void gpuPixelsBegin()
{
#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		if (non_default_flags & NON_DEFAULT_RED_SCALE)   glPixelTransferf(GL_RED_SCALE,   pixel_red_scale);
		if (non_default_flags & NON_DEFAULT_RED_BIAS)    glPixelTransferf(GL_RED_BIAS,    pixel_red_bias);
		if (non_default_flags & NON_DEFAULT_GREEN_SCALE) glPixelTransferf(GL_BLUE_SCALE,  pixel_blue_scale);
		if (non_default_flags & NON_DEFAULT_GREEN_BIAS)  glPixelTransferf(GL_BLUE_BIAS,   pixel_blue_bias);
		if (non_default_flags & NON_DEFAULT_BLUE_SCALE)  glPixelTransferf(GL_GREEN_SCALE, pixel_green_scale);
		if (non_default_flags & NON_DEFAULT_BLUE_BIAS)   glPixelTransferf(GL_GREEN_BIAS,  pixel_green_bias);
		if (non_default_flags & NON_DEFAULT_ALPHA_SCALE) glPixelTransferf(GL_ALPHA_SCALE, pixel_alpha_scale);
		if (non_default_flags & NON_DEFAULT_ALPHA_BIAS)  glPixelTransferf(GL_ALPHA_BIAS,  pixel_alpha_bias);

		if (non_default_flags & NON_DEFAULT_FACTOR) glPixelZoom(zoom_xfactor, zoom_yfactor);

		if (non_default_flags & NON_DEFAULT_UNPACK_ROW_LENGTH) glPixelStorei(GL_UNPACK_ROW_LENGTH, format_unpack_row_length);
		if (non_default_flags & NON_DEFAULT_UNPACK_SWAP_BYTES) glPixelStorei(GL_UNPACK_SWAP_BYTES, format_unpack_swap_bytes);
		if (non_default_flags & NON_DEFAULT_UNPACK_ALIGNMENT)  glPixelStorei(GL_UNPACK_ALIGNMENT,  format_unpack_alignment);
	}
#endif
}



#if defined(WITH_GL_PROFILE_COMPAT)
/**
 * Functions like glRasterPos2i, except ensures that the resulting
 * raster position is valid. \a known_good_x and \a known_good_y
 * should be coordinates of a point known to be within the current
 * view frustum.
 * \attention This routine should be used when the distance of \a x
 * and \a y away from the known good point is small (ie. for small icons
 * and for bitmap characters), when drawing large+zoomed images it is
 * possible for overflow to occur, the glaDrawPixelsSafe routine should
 * be used instead.
 */
static void raster_pos_safe_2f(float x, float y, float known_good_x, float known_good_y)
{
	GLubyte dummy = 0;

	/* As long as known good coordinates are correct
	 * this is guaranteed to generate an ok raster
	 * position (ignoring potential (real) overflow
	 * issues).
	 */
	glRasterPos2f(known_good_x, known_good_y);

	/* Now shift the raster position to where we wanted
	 * it in the first place using the glBitmap trick.
	 */
	glBitmap(0, 0, 0, 0, x - known_good_x, y - known_good_y, &dummy);
}
#endif



void gpuPixelPos2f(GLfloat x, GLfloat y)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		/* Don't use safe RasterPos (slower) if we can avoid it. */
		if (rast_x >= 0 && rast_y >= 0) {
			glRasterPos2f(rast_x, rast_y);
		}
		else {
			raster_pos_safe_2f(rast_x, rast_y, 0, 0);
		}
	}
#endif
}



void gpuPixelPos3f(GLfloat x, GLfloat y, GLfloat z)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		glRasterPos3f(x, y, z);
	}
#endif
}



void gpuBitmap(GPUbitmap* bitmap)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		glBitmap(
			bitmap->width,
			bitmap->height,
			bitmap->xorig,
			bitmap->yorig
			0,
			0,
			bitmap->bitmap);
	}
#endif
}



void gpuPixels(GPUpixels* pixels)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		glDrawPixels(
			pixels->width,
			pixels->height,
			pixels->format,
			pixels->type
			pixels->pixels);
	}
#endif
}



void gpuPixelsEnd()
{
#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		if (non_default_flags & NON_DEFAULT_RED_SCALE) {
			glPixelTransferf(GL_RED_SCALE, 1);
		}

		if (non_default_flags & NON_DEFAULT_RED_BIAS) {
			glPixelTransferf(GL_RED_BIAS, 0);
		}

		if (non_default_flags & NON_DEFAULT_GREEN_SCALE) {
			glPixelTransferf(GL_BLUE_SCALE, 1);
		}

		if (non_default_flags & NON_DEFAULT_GREEN_BIAS) {
			glPixelTransferf(GL_BLUE_BIAS, 0);
		}

		if (non_default_flags & NON_DEFAULT_BLUE_SCALE) {
			glPixelTransferf(GL_GREEN_SCALE, 1);
		}

		if (non_default_flags & NON_DEFAULT_BLUE_BIAS) {
			glPixelTransferf(GL_GREEN_BIAS, 0);
		}

		if (non_default_flags & NON_DEFAULT_ALPHA_SCALE) {
			glPixelTransferf(GL_ALPHA_SCALE, 1);
		}

		if (non_default_flags & NON_DEFAULT_ALPHA_BIAS) {
			glPixelTransferf(GL_ALPHA_BIAS, 0);
		}

		if (non_default_flags & NON_DEFAULT_FACTOR) {
			glPixelZoom(1, 1);
		}

		if (non_default_flags & NON_DEFAULT_UNPACK_ROW_LENGTH) {
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		}

		if (non_default_flags & NON_DEFAULT_UNPACK_ALIGNMENT) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}

		if (non_default_flags & NON_DEFAULT_UNPACK_ROW_LENGTH) {
			glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		}
	}
#endif
}
