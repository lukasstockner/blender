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

/** \file source/blender/gpu/intern/gpu_pixels.c
 *  \ingroup gpu
 */

/* my library */
#include "GPU_aspect.h"
#include "GPU_extensions.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_debug.h"
#include "GPU_state_latch.h"
#include "GPU_pixels.h"

/* internal */
#include "intern/gpu_private.h"

/* external */
#include "BLI_dynstr.h"

#include "MEM_guardedalloc.h"


static struct GPUShader *PIXELS_SHADER = NULL;
static struct GPUcommon  PIXELS_COMMON = {0};
static bool              PIXELS_FAILED = false;

static GLfloat PIXELS_POS[3] = { 0, 0, 0 };

static bool PIXELS_BEGUN = false;

void gpu_pixels_init(void)
{
	PIXELS_SHADER = NULL;
}

void gpu_pixels_exit(void)
{
	GPU_shader_free(PIXELS_SHADER);
}

void GPU_bitmap_cache(GPUbitmap *bitmap)
{
}

void GPU_pixels_cache(GPUpixels *pixels)
{
}

void GPU_bitmap_uncache(GPUbitmap *bitmap)
{
}

void GPU_pixels_uncache(GPUpixels *pixels)
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

void GPU_pixels_format(GLenum pname, GLint param)
{
	switch (pname) {
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
			GPU_print_error_debug("GPU_pixels_format: unknown parameter");
			break;
	}
}



static GLfloat pixels_zoom_xfactor = 1;
static GLfloat pixels_zoom_yfactor = 1;

void GPU_pixels_zoom(GLfloat xfactor, GLfloat yfactor)
{
	pixels_zoom_xfactor = xfactor;
	pixels_zoom_yfactor = yfactor;

	if (xfactor == 1 || yfactor == 1)
		non_default_flags ^= NON_DEFAULT_FACTOR;
	else
		non_default_flags |= NON_DEFAULT_FACTOR;
}



void GPU_get_pixels_zoom(GLfloat *xfactor_out, GLfloat *yfactor_out)
{
	*xfactor_out = pixels_zoom_xfactor;
	*yfactor_out = pixels_zoom_yfactor;
}



static GLfloat pixels_scale_red   = 1;
static GLfloat pixels_scale_green = 1;
static GLfloat pixels_scale_blue  = 1;
static GLfloat pixels_scale_alpha = 1;

static GLfloat pixels_bias_red    = 0;
static GLfloat pixels_bias_green  = 0;
static GLfloat pixels_bias_blue   = 0;
static GLfloat pixels_bias_alpha  = 0;

/* XXX jwilkins: this would be a lot shorter if you made a table */

void GPU_pixels_uniform_1f(GLenum pname, GLfloat param)
{
	switch (pname) {
		case GL_RED_SCALE:
			pixels_scale_red = param;

			if (param == 1)
				non_default_flags ^= NON_DEFAULT_RED_SCALE;
			else
				non_default_flags |= NON_DEFAULT_RED_SCALE;

			break;

		case GL_RED_BIAS:
			pixels_bias_red = param;

			if (param != 0)
				non_default_flags ^= NON_DEFAULT_RED_BIAS;
			else
				non_default_flags |= NON_DEFAULT_RED_BIAS;

			break;

		case GL_BLUE_SCALE:
			pixels_scale_blue = param;

			if (param != 1)
				non_default_flags ^= NON_DEFAULT_BLUE_SCALE;
			else
				non_default_flags |= NON_DEFAULT_BLUE_SCALE;

			break;

		case GL_BLUE_BIAS:
			pixels_bias_blue = param;

			if (param != 0)
				non_default_flags ^= NON_DEFAULT_BLUE_BIAS;
			else
				non_default_flags |= NON_DEFAULT_BLUE_BIAS;

			break;

		case GL_GREEN_SCALE:
			pixels_scale_green = param;

			if (param != 1)
				non_default_flags ^= NON_DEFAULT_GREEN_SCALE;
			else
				non_default_flags |= NON_DEFAULT_GREEN_SCALE;

			break;

		case GL_GREEN_BIAS:
			pixels_bias_green = param;

			if (param != 0)
				non_default_flags ^= NON_DEFAULT_GREEN_BIAS;
			else
				non_default_flags |= NON_DEFAULT_GREEN_BIAS;

			break;

		case GL_ALPHA_SCALE:
			pixels_scale_alpha = param;

			if (param != 1)
				non_default_flags ^= NON_DEFAULT_ALPHA_SCALE;
			else
				non_default_flags |= NON_DEFAULT_ALPHA_SCALE;

			break;

		case GL_ALPHA_BIAS:
			pixels_bias_alpha = param;

			if (param != 0)
				non_default_flags ^= NON_DEFAULT_ALPHA_BIAS;
			else
				non_default_flags |= NON_DEFAULT_ALPHA_BIAS;

			break;

		default:
			GPU_print_error_debug("GPU_pixels_uniform_1f unknown argument");
			break;
	}
}



static GLint location_scale;
static GLint location_bias;

static void pixels_init_uniform_locations(void)
{
	location_scale = GPU_shader_get_uniform(PIXELS_SHADER, "b_Pixels.scale");
	location_bias  = GPU_shader_get_uniform(PIXELS_SHADER, "b_Pixels.bias" );
}



static void commit_pixels(void)
{
	GPU_ASSERT_NO_GL_ERRORS("commit_pixels start");
	glUniform4f(location_scale, pixels_scale_red, pixels_scale_green, pixels_scale_blue, pixels_scale_alpha);
	glUniform4f(location_bias,  pixels_bias_red,  pixels_bias_green,  pixels_bias_blue,  pixels_bias_alpha);
	GPU_ASSERT_NO_GL_ERRORS("commit_pixels end");
}



static void gpu_pixels_shader(void)
{
	/* GLSL code */
	extern const char datatoc_gpu_shader_pixels_uniforms_glsl[];
	extern const char datatoc_gpu_shader_pixels_vert_glsl[];
	extern const char datatoc_gpu_shader_pixels_frag_glsl[];

	/* Create shader if it doesn't exist yet. */
	if (PIXELS_SHADER != NULL) {
		GPU_shader_bind(PIXELS_SHADER);
		gpu_set_common(&PIXELS_COMMON);
	}
	else if (!PIXELS_FAILED) {
		DynStr *vert = BLI_dynstr_new();
		DynStr *frag = BLI_dynstr_new();
		DynStr *defs = BLI_dynstr_new();

		char *vert_cstring;
		char *frag_cstring;
		char *defs_cstring;

		gpu_include_common_vert(vert);
		BLI_dynstr_append(vert, datatoc_gpu_shader_pixels_uniforms_glsl);
		BLI_dynstr_append(vert, datatoc_gpu_shader_pixels_vert_glsl);

		gpu_include_common_frag(frag);
		BLI_dynstr_append(frag, datatoc_gpu_shader_pixels_uniforms_glsl);
		BLI_dynstr_append(frag, datatoc_gpu_shader_pixels_frag_glsl);

		gpu_include_common_defs(defs);
		BLI_dynstr_append(defs, "#define USE_TEXTURE_2D\n");

		vert_cstring = BLI_dynstr_get_cstring(vert);
		frag_cstring = BLI_dynstr_get_cstring(frag);
		defs_cstring = BLI_dynstr_get_cstring(defs);

		PIXELS_SHADER =
			GPU_shader_create(vert_cstring, frag_cstring, NULL, defs_cstring);

		MEM_freeN(vert_cstring);
		MEM_freeN(frag_cstring);
		MEM_freeN(defs_cstring);

		BLI_dynstr_free(vert);
		BLI_dynstr_free(frag);
		BLI_dynstr_free(defs);

		if (PIXELS_SHADER != NULL) {
			gpu_common_get_symbols(&PIXELS_COMMON, PIXELS_SHADER);
			gpu_set_common(&PIXELS_COMMON);

			pixels_init_uniform_locations();

			GPU_shader_bind(PIXELS_SHADER);

			commit_pixels(); /* only needs to be done once */
		}
		else {
			PIXELS_FAILED = true;
			gpu_set_common(NULL);
		}
	}
	else {
		gpu_set_common(NULL);
	}
}



void gpu_pixels_bind(void)
{
	bool glsl_support = GPU_glsl_support();

	if (glsl_support)
		gpu_pixels_shader();

#if defined(WITH_GL_PROFILE_COMPAT)
	if (!glsl_support)
		GPU_CHECK_ERRORS_AROUND(glEnable(GL_TEXTURE_2D));
#endif

	gpu_commit_matrix();
}



void gpu_pixels_unbind(void)
{
	bool glsl_support = GPU_glsl_support();

	if (glsl_support)
		GPU_shader_unbind();

#if defined(WITH_GL_PROFILE_COMPAT)
	if (!glsl_support)
		GPU_CHECK_ERRORS_AROUND(glDisable(GL_TEXTURE_2D));
#endif
}



void GPU_pixels_begin()
{
	BLI_assert(!PIXELS_BEGUN);
	PIXELS_BEGUN = true;

#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		if (non_default_flags & NON_DEFAULT_RED_SCALE)   glPixelTransferf(GL_RED_SCALE,   pixels_scale_red);
		if (non_default_flags & NON_DEFAULT_RED_BIAS)    glPixelTransferf(GL_RED_BIAS,    pixels_bias_red);
		if (non_default_flags & NON_DEFAULT_GREEN_SCALE) glPixelTransferf(GL_BLUE_SCALE,  pixels_scale_blue);
		if (non_default_flags & NON_DEFAULT_GREEN_BIAS)  glPixelTransferf(GL_BLUE_BIAS,   pixels_bias_blue);
		if (non_default_flags & NON_DEFAULT_BLUE_SCALE)  glPixelTransferf(GL_GREEN_SCALE, pixels_scale_green);
		if (non_default_flags & NON_DEFAULT_BLUE_BIAS)   glPixelTransferf(GL_GREEN_BIAS,  pixels_bias_green);
		if (non_default_flags & NON_DEFAULT_ALPHA_SCALE) glPixelTransferf(GL_ALPHA_SCALE, pixels_scale_alpha);
		if (non_default_flags & NON_DEFAULT_ALPHA_BIAS)  glPixelTransferf(GL_ALPHA_BIAS,  pixels_bias_alpha);

		if (non_default_flags & NON_DEFAULT_FACTOR) glPixelZoom(pixels_zoom_xfactor, pixels_zoom_yfactor);

		if (non_default_flags & NON_DEFAULT_UNPACK_ROW_LENGTH) glPixelStorei(GL_UNPACK_ROW_LENGTH, format_unpack_row_length);
		if (non_default_flags & NON_DEFAULT_UNPACK_SWAP_BYTES) glPixelStorei(GL_UNPACK_SWAP_BYTES, format_unpack_swap_bytes);
		if (non_default_flags & NON_DEFAULT_UNPACK_ALIGNMENT)  glPixelStorei(GL_UNPACK_ALIGNMENT,  format_unpack_alignment);
	}
#endif

	/* SSS End (Assuming the basic aspect is ending) */
	GPU_aspect_end();

	/* SSS Begin Pixels */
	GPU_aspect_begin(GPU_ASPECT_PIXELS, NULL);
}



void GPU_pixels_end()
{
	BLI_assert(PIXELS_BEGUN);

#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		GPU_ASSERT_NO_GL_ERRORS("GPU_pixels_end start");

		if (non_default_flags & NON_DEFAULT_RED_SCALE)
			glPixelTransferf(GL_RED_SCALE, 1);

		if (non_default_flags & NON_DEFAULT_RED_BIAS)
			glPixelTransferf(GL_RED_BIAS, 0);

		if (non_default_flags & NON_DEFAULT_GREEN_SCALE)
			glPixelTransferf(GL_BLUE_SCALE, 1);

		if (non_default_flags & NON_DEFAULT_GREEN_BIAS)
			glPixelTransferf(GL_BLUE_BIAS, 0);

		if (non_default_flags & NON_DEFAULT_BLUE_SCALE)
			glPixelTransferf(GL_GREEN_SCALE, 1);

		if (non_default_flags & NON_DEFAULT_BLUE_BIAS)
			glPixelTransferf(GL_GREEN_BIAS, 0);

		if (non_default_flags & NON_DEFAULT_ALPHA_SCALE)
			glPixelTransferf(GL_ALPHA_SCALE, 1);

		if (non_default_flags & NON_DEFAULT_ALPHA_BIAS)
			glPixelTransferf(GL_ALPHA_BIAS, 0);

		if (non_default_flags & NON_DEFAULT_FACTOR)
			glPixelZoom(1, 1);

		if (non_default_flags & NON_DEFAULT_UNPACK_ROW_LENGTH)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		if (non_default_flags & NON_DEFAULT_UNPACK_ALIGNMENT)
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		if (non_default_flags & NON_DEFAULT_UNPACK_ROW_LENGTH)
			glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);

		GPU_ASSERT_NO_GL_ERRORS("GPU_pixels_end end");
	}
#endif

	/* SSS End Pixels */
	GPU_aspect_end();

	PIXELS_BEGUN = false;

	/* SSS Begin Basic */
	GPU_aspect_begin(GPU_ASPECT_BASIC, NULL);
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

	GPU_ASSERT_NO_GL_ERRORS("raster_pos_safe_2f start");

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

	GPU_ASSERT_NO_GL_ERRORS("raster_pos_safe_2f end");
}
#endif



void GPU_pixels_pos_2f(GLfloat x, GLfloat y)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	/* Don't use safe RasterPos (slower) if we can avoid it. */
	if (x >= 0 && y >= 0)
		glRasterPos2f(x, y);
	else
		raster_pos_safe_2f(x, y, 0, 0);
#endif

#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	VEC3D(PIXELS_POS, x, y, 0);
#endif
}



void GPU_pixels_pos_3f(GLfloat x, GLfloat y, GLfloat z)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	glRasterPos3fv(PIXELS_POS);
#endif

#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	VEC3D(PIXELS_POS, x, y, z);
#endif
}



void GPU_bitmap(GPUbitmap *bitmap)
{
	BLI_assert(PIXELS_BEGUN);

#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		glBitmap(
			bitmap->width,
			bitmap->height,
			bitmap->xorig,
			bitmap->yorig,
			0,
			0,
			bitmap->bitmap);
	}
#endif
}



extern void glaDrawPixelsTexScaled(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, const void *rect, float scaleX, float scaleY);

void GPU_pixels(GPUpixels *pixels)
{
	BLI_assert(PIXELS_BEGUN);

#if defined(WITH_GL_PROFILE_COMPAT)
	if (GPU_PROFILE_COMPAT) {
		GPU_CHECK_ERRORS_AROUND(
			glDrawPixels(
				pixels->width,
				pixels->height,
				pixels->format,
				pixels->type,
				pixels->pixels));

		return;
	}
#endif

#if defined(WITH_GL_PROFILE_ES20) || defined(WITH_GL_PROFILE_CORE)
	if (GPU_PROFILE_ES20 || GPU_PROFILE_CORE) {
		glaDrawPixelsTexScaled(
			PIXELS_POS[0],
			PIXELS_POS[1],
			pixels->width,
			pixels->height,
			pixels->format,
			pixels->type,
			GL_NEAREST,
			pixels->pixels,
			1,
			1);
		return;
	}
#endif
}
