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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/glutil.c
 *  \ingroup edscr
 */


#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BLI_rect.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "BKE_blender.h"
#include "BKE_global.h"
#include "BKE_colortools.h"
#include "BKE_context.h"

#include "GPU_primitives.h"
#include "GPU_colors.h"

#include "BIF_glutil.h"

#include "GPU_extensions.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE                        0x812F
#endif


/* ******************************************** */

const GLubyte stipple_halftone[128] = {
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55};


/*  repeat this pattern
 *
 *     X000X000
 *     00000000
 *     00X000X0
 *     00000000 */


const GLubyte stipple_quarttone[128] = {
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0};


const GLubyte stipple_diag_stripes_pos[128] = {
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f};


const GLubyte stipple_diag_stripes_neg[128] = {
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80};


//void fdrawbezier(float vec[4][3])
//{
//	float dist;
//	float curve_res = 24, spline_step = 0.0f;
//	
//	dist = 0.5f * ABS(vec[0][0] - vec[3][0]);
//	
//	/* check direction later, for top sockets */
//	vec[1][0] = vec[0][0] + dist;
//	vec[1][1] = vec[0][1];
//	
//	vec[2][0] = vec[3][0] - dist;
//	vec[2][1] = vec[3][1];
//	/* we can reuse the dist variable here to increment the GL curve eval amount*/
//	dist = 1.0f / curve_res;
//	
//	gpuCurrentColor3x(CPACK_BLACK);
//	glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, vec[0]);
//	gpuBegin(GL_LINE_STRIP);
//	while (spline_step < 1.000001f) {
//#if 0
//		if (do_shaded)
//			UI_ThemeColorBlend(th_col1, th_col2, spline_step);
//#endif
//		glEvalCoord1f(spline_step);
//		spline_step += dist;
//	}
//	gpuEnd();
//}


void fdrawcheckerboard(float x1, float y1, float x2, float y2)
{
	unsigned char col1[4] = {40, 40, 40}, col2[4] = {50, 50, 50};

	GLubyte checker_stipple[32 * 32 / 8] = {
		255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
		255,  0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
		0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
		0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
		255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
		255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
		0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
		0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255};
	
	gpuCurrentColor3ubv(col1);
	gpuSingleFilledRectf(x1, y1, x2, y2);
	gpuCurrentColor3ubv(col2);

	gpuEnablePolygonStipple();
	gpuPolygonStipple(checker_stipple);
	gpuSingleFilledRectf(x1, y1, x2, y2);
	gpuDisablePolygonStipple();
}

/*
 *     x1,y2
 *     |  \
 *     |   \
 *     |    \
 *     x1,y1-- x2,y1
 */

static void sdrawtripoints(short x1, short y1, short x2, short y2)
{
	short v[2];
	v[0] = x1; v[1] = y1;
	gpuVertex2sv(v);
	v[0] = x1; v[1] = y2;
	gpuVertex2sv(v);
	v[0] = x2; v[1] = y1;
	gpuVertex2sv(v);
}

void sdrawtri(short x1, short y1, short x2, short y2)
{
	gpuBegin(GL_LINE_STRIP);
	sdrawtripoints(x1, y1, x2, y2);
	gpuEnd();
}

void sdrawtrifill(short x1, short y1, short x2, short y2)
{
	gpuBegin(GL_TRIANGLES);
	sdrawtripoints(x1, y1, x2, y2);
	gpuEnd();
}


/* ******************************************** */

void setlinestyle(int nr)
{
	if (nr == 0) {
		gpuDisableLineStipple();
	}
	else {
		
		gpuEnableLineStipple();
		if (U.pixelsize > 1.0f)
			gpuLineStipple(nr, 0xCCCC);
		else
			gpuLineStipple(nr, 0xAAAA);
	}
}

/* Invert line handling */
	
#define GL_TOGGLE(mode, onoff)  (((onoff) ? glEnable : glDisable)(mode))

void set_inverted_drawing(int enable) 
{
// XXX jwilkins: LogicOp invert and 1-DST are two ways to the same thing.
#if defined(WITH_GL_PROFILE_COMPAT)
	glLogicOp(enable ? GL_INVERT : GL_COPY);
	GL_TOGGLE(GL_COLOR_LOGIC_OP, enable);
#else
	if (enable) 
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_DST_COLOR);
	else
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

	GL_TOGGLE(GL_DITHER, !enable);
}

void sdrawXORline(int x0, int y0, int x1, int y1)
{
	if (x0 == x1 && y0 == y1) return;

	set_inverted_drawing(1);

	gpuBegin(GL_LINES);
	gpuVertex2i(x0, y0);
	gpuVertex2i(x1, y1);
	gpuEnd();

	set_inverted_drawing(0);
}

void sdrawXORline4(int nr, int x0, int y0, int x1, int y1)
{
	static short old[4][2][2];
	static char flags[4] = {0, 0, 0, 0};
	
	/* with builtin memory, max 4 lines */

	set_inverted_drawing(1);
		
	gpuBegin(GL_LINES);
	if (nr == -1) { /* flush */
		for (nr = 0; nr < 4; nr++) {
			if (flags[nr]) {
				gpuVertex2sv(old[nr][0]);
				gpuVertex2sv(old[nr][1]);
				flags[nr] = 0;
			}
		}
	}
	else {
		if (nr >= 0 && nr < 4) {
			if (flags[nr]) {
				gpuVertex2sv(old[nr][0]);
				gpuVertex2sv(old[nr][1]);
			}

			old[nr][0][0] = x0;
			old[nr][0][1] = y0;
			old[nr][1][0] = x1;
			old[nr][1][1] = y1;
			
			flags[nr] = 1;
		}
		
		gpuVertex2i(x0, y0);
		gpuVertex2i(x1, y1);
	}
	gpuEnd();
	
	set_inverted_drawing(0);
}

void fdrawXORellipse(float xofs, float yofs, float hw, float hh)
{
	if (hw == 0) {
		return;
	}

	set_inverted_drawing(1);
	gpuSingleEllipse(xofs, yofs, hw, hh, 20);
	set_inverted_drawing(0);
}

void fdrawXORcirc(float xofs, float yofs, float rad)
{
	set_inverted_drawing(1);
	gpuSingleCircle(xofs, yofs, rad, 20);
	set_inverted_drawing(0);
}

int glaGetOneInteger(int param)
{
	GLint i;
	glGetIntegerv(param, &i);
	return i;
}

float glaGetOneFloat(int param)
{
	GLfloat v;
	glGetFloatv(param, &v);
	return v;
}

static int get_cached_work_texture(int *w_r, int *h_r)
{
	static GLint texid = -1;
	static int tex_w = 256;
	static int tex_h = 256;

	if (texid == -1) {
		GLint ltexid = gpuGetTextureBinding2D();
		unsigned char *tbuf;

		glGenTextures(1, (GLuint *)&texid);

		gpuBindTexture(GL_TEXTURE_2D, texid);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		tbuf = MEM_callocN(tex_w * tex_h * 4, "tbuf");
		glTexImage2D(GL_TEXTURE_2D, 0, (GLEW_VERSION_1_1 || GLEW_OES_required_internalformat) ? GL_RGBA8 : GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tbuf);
		MEM_freeN(tbuf);

		gpuBindTexture(GL_TEXTURE_2D, ltexid); /* restore previous value */
	}

	*w_r = tex_w;
	*h_r = tex_h;
	return texid;
}

void glaDrawPixelsTexScaled(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect, float scaleX, float scaleY)
{
	float xzoom, yzoom;
	int ltexid = gpuGetTextureBinding2D();
	int subpart_x, subpart_y, tex_w, tex_h;
	int seamless, offset_x, offset_y, nsubparts_x, nsubparts_y;
	int texid = get_cached_work_texture(&tex_w, &tex_h);
	int components;

	gpuGetPixelZoom(&xzoom, &yzoom);

#if defined(WITH_GL_PROFILE_COMPAT)
	/* Specify the color outside this function, and tex will modulate it.
	 * This is useful for changing alpha without using glPixelTransferf()
	 */
	//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // XXX jwilkins: blender never changes the TEXTURE_ENV_MODE
	glPixelStorei(GL_UNPACK_ROW_LENGTH, img_w);
#else
	// XXX jwilkins: ES doesn't have GL_UNPACK_ROW_LENGTH, so sub images will have to be created on the CPU
	if (tex_w < img_w || tex_h < img_h) 
		return;
#endif

	gpuBindTexture(GL_TEXTURE_2D, texid);

	/* don't want nasty border artifacts */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, zoomfilter);

	/* setup seamless 2=on, 0=off */
	seamless = ((tex_w < img_w || tex_h < img_h) && tex_w > 2 && tex_h > 2) ? 2 : 0;

	offset_x = tex_w - seamless;
	offset_y = tex_h - seamless;

	nsubparts_x = (img_w + (offset_x - 1)) / (offset_x);
	nsubparts_y = (img_h + (offset_y - 1)) / (offset_y);

	if (format == GL_RGBA)
		components = 4;
	else if (format == GL_RGB)
		components = 3;
	else if (format == GL_LUMINANCE)
		components = 1;
	else {
		BLI_assert(!"Incompatible format passed to glaDrawPixelsTexScaled");
		return;
	}

	if (type == GL_FLOAT)
	{
		/* need to set internal format to higher range float */

		/* NOTE: this could fail on some drivers, like mesa,
		 *       but currently this code is only used by color
		 *       management stuff which already checks on whether
		 *       it's possible to use GL_RGBA16F
		 */

		if (GLEW_VERSION_3_0 || GLEW_ARB_texture_float || GLEW_EXT_texture_storage || GLEW_EXT_color_buffer_half_float) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, tex_w, tex_h, 0, format, GL_FLOAT, NULL);
		}
		else {
			// XXX jwilkins: not sure if falling back to RGBA internal format will work
			glTexImage2D(GL_TEXTURE_2D, 0, (GLEW_VERSION_1_1 || GLEW_OES_required_internalformat) ? GL_RGBA8 : GL_RGBA, tex_w, tex_h, 0, format, GL_FLOAT, NULL);
		}
	}
	else
	{
		/* switch to 8bit RGBA for byte buffer  */
		glTexImage2D(GL_TEXTURE_2D, 0, (GLEW_VERSION_1_1 || GLEW_OES_required_internalformat) ? GL_RGBA8 : GL_RGBA, tex_w, tex_h, 0, format, GL_UNSIGNED_BYTE, NULL);
	}

	gpuImmediateFormat_T2_V2();

	for (subpart_y = 0; subpart_y < nsubparts_y; subpart_y++) {
		for (subpart_x = 0; subpart_x < nsubparts_x; subpart_x++) {
			int remainder_x = img_w - subpart_x * offset_x;
			int remainder_y = img_h - subpart_y * offset_y;
			int subpart_w = (remainder_x < tex_w) ? remainder_x : tex_w;
			int subpart_h = (remainder_y < tex_h) ? remainder_y : tex_h;
			int offset_left = (seamless && subpart_x != 0) ? 1 : 0;
			int offset_bot = (seamless && subpart_y != 0) ? 1 : 0;
			int offset_right = (seamless && remainder_x > tex_w) ? 1 : 0;
			int offset_top = (seamless && remainder_y > tex_h) ? 1 : 0;
			float rast_x = x + subpart_x * offset_x * xzoom;
			float rast_y = y + subpart_y * offset_y * yzoom;
			
			/* check if we already got these because we always get 2 more when doing seamless*/
			if (subpart_w <= seamless || subpart_h <= seamless)
				continue;
			
			if (type == GL_FLOAT) {
				float *f_rect = (float *)rect;

				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, format, GL_FLOAT, &f_rect[subpart_y * offset_y * img_w * components + subpart_x * offset_x * components]);
				
				/* add an extra border of pixels so linear looks ok at edges of full image. */
				if (subpart_w < tex_w)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, 0, 1, subpart_h, format, GL_FLOAT, &f_rect[subpart_y * offset_y * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
				if (subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, subpart_h, subpart_w, 1, format, GL_FLOAT, &f_rect[(subpart_y * offset_y + subpart_h - 1) * img_w * components + subpart_x * offset_x * components]);
				if (subpart_w < tex_w && subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, subpart_h, 1, 1, format, GL_FLOAT, &f_rect[(subpart_y * offset_y + subpart_h - 1) * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
			}
			else {
				unsigned char *uc_rect = (unsigned char *) rect;

				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, format, GL_UNSIGNED_BYTE, &uc_rect[subpart_y * offset_y * img_w * components + subpart_x * offset_x * components]);
				
				if (subpart_w < tex_w)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, 0, 1, subpart_h, format, GL_UNSIGNED_BYTE, &uc_rect[subpart_y * offset_y * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
				if (subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, subpart_h, subpart_w, 1, format, GL_UNSIGNED_BYTE, &uc_rect[(subpart_y * offset_y + subpart_h - 1) * img_w * components + subpart_x * offset_x * components]);
				if (subpart_w < tex_w && subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, subpart_h, 1, 1, format, GL_UNSIGNED_BYTE, &uc_rect[(subpart_y * offset_y + subpart_h - 1) * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
			}

#if defined(WITH_GL_PROFILE_COMPAT)
			if (GPU_PROFILE_COMPAT) {
				glEnable(GL_TEXTURE_2D);
			}
#endif

			gpuBegin(GL_TRIANGLE_FAN);

			gpuTexCoord2f((float)(0 + offset_left) / tex_w, (float)(0 + offset_bot) / tex_h);
			gpuVertex2f(rast_x + (float)offset_left * xzoom, rast_y + (float)offset_bot * yzoom);

			gpuTexCoord2f((float)(subpart_w - offset_right) / tex_w, (float)(0 + offset_bot) / tex_h);
			gpuVertex2f(rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX, rast_y + (float)offset_bot * yzoom);

			gpuTexCoord2f((float)(subpart_w - offset_right) / tex_w, (float)(subpart_h - offset_top) / tex_h);
			gpuVertex2f(rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX, rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY);

			gpuTexCoord2f((float)(0 + offset_left) / tex_w, (float)(subpart_h - offset_top) / tex_h);
			gpuVertex2f(rast_x + (float)offset_left * xzoom, rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY);

			gpuEnd();

#if defined(WITH_GL_PROFILE_COMPAT)
			if (GPU_PROFILE_COMPAT) {
				glDisable(GL_TEXTURE_2D);
			}
#endif
		}
	}

	gpuImmediateUnformat();

	gpuBindTexture(GL_TEXTURE_2D, ltexid);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); /* restore default value */
}

void glaDrawPixelsTex(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect)
{
	glaDrawPixelsTexScaled(x, y, img_w, img_h, format, type, zoomfilter, rect, 1.0f, 1.0f);
}

void glaDrawPixelsSafe(float x, float y, int img_w, int img_h, int row_w, int format, int type, void *rect)
{
	float xzoom, yzoom;
	float ix, iy;
	float off_x, off_y;
	float rast_x, rast_y;
	int scissor[4];
	int draw_w, draw_h;

	gpuGetPixelZoom(&xzoom, &yzoom);

	/* The pixel space coordinate of the intersection of
	 * the [zoomed] image with the origin.
	 */
	ix = -x / xzoom;
	iy = -y / yzoom;
		
	/* The maximum pixel amounts the image can be cropped
	 * at the lower left without exceeding the origin.
	 */
	off_x = floor(max_ff(ix, 0.0f));
	off_y = floor(max_ff(iy, 0.0f));

	/* The zoomed space coordinate of the raster position
	 * (starting at the lower left most unclipped pixel).
	 */
	rast_x = x + off_x * xzoom;
	rast_y = y + off_y * yzoom;

	/* Determine the smallest number of pixels we need to draw
	 * before the image would go off the upper right corner.
	 *
	 * It may seem this is just an optimization but some graphics
	 * cards (ATI) freak out if there is a large zoom factor and
	 * a large number of pixels off the screen (probably at some
	 * level the number of image pixels to draw is getting multiplied
	 * by the zoom and then clamped). Making sure we draw the
	 * fewest pixels possible keeps everyone mostly happy (still
	 * fails if we zoom in on one really huge pixel so that it
	 * covers the entire screen).
	 */
	gpuGetSizeBox(GL_SCISSOR_BOX, scissor);
	draw_w = min_ii(img_w - off_x, ceil((float)(scissor[2] - rast_x) / xzoom));
	draw_h = min_ii(img_h - off_y, ceil((float)(scissor[3] - rast_y) / yzoom));

	if (draw_w > 0 && draw_h > 0) {
		int index, components;
		void* data;

		gpuPixelFormat(GL_UNPACK_ROW_LENGTH, row_w);

		components = ELEM(format, GL_LUMINANCE, GL_RED) ? 1 : 4; // Note: GL_RED is not defined in ES 2.0,
		                                                         // but if it still somehow gets passed in here, it should work fine.
		index = (off_y * row_w + off_x) * components;

		if (type == GL_FLOAT) {
			data = (GLfloat*)rect + index;
		}
		else if (type == GL_INT || type == GL_UNSIGNED_INT) {
			data = (GLint*)rect + index;
		}

		{
			GPUpixels pixels = { draw_w, draw_h, format, type, data };

			gpuPixelsBegin();
			gpuPixelPos2f(rast_x, rast_y);
			gpuPixels(&pixels);
			gpuPixelsEnd();
		}
	}
}

/* uses either DrawPixelsSafe or DrawPixelsTex, based on user defined maximum */
void glaDrawPixelsAuto(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect)
{
	if (U.image_draw_method != IMAGE_DRAW_METHOD_DRAWPIXELS) {
		gpuCurrentColor4f(1.0, 1.0, 1.0, 1.0);
		glaDrawPixelsTex(x, y, img_w, img_h, format, type, zoomfilter, rect);
	}
	else {
		glaDrawPixelsSafe(x, y, img_w, img_h, img_w, format, type, rect);
	}
}

/* 2D Drawing Assistance */

void glaDefine2DArea(rcti *screen_rect)
{
	const int sc_w = BLI_rcti_size_x(screen_rect) + 1;
	const int sc_h = BLI_rcti_size_y(screen_rect) + 1;

	gpuViewport(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);
	gpuScissor(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);

	/* The GLA_PIXEL_OFS magic number is to shift the matrix so that
	 * both raster and vertex integer coordinates fall at pixel
	 * centers properly. For a longer discussion see the OpenGL
	 * Programming Guide, Appendix H, Correctness Tips.
	 */

	gpuMatrixMode(GL_PROJECTION);
	gpuLoadIdentity();
	gpuOrtho(0.0, sc_w, 0.0, sc_h, -1, 1);
	gpuTranslate(GLA_PIXEL_OFS, GLA_PIXEL_OFS, 0.0);

	gpuMatrixMode(GL_MODELVIEW);
	gpuLoadIdentity();
}

#if 0 /* UNUSED */

struct gla2DDrawInfo {
	int orig_vp[4], /*orig_sc[4]; Unused*/
	float orig_projmat[16], orig_viewmat[16];

	rcti screen_rect;
	rctf world_rect;

	float wo_to_sc[2];
};

void gla2DGetMap(gla2DDrawInfo *di, rctf *rect) 
{
	*rect = di->world_rect;
}

void gla2DSetMap(gla2DDrawInfo *di, rctf *rect) 
{
	int sc_w, sc_h;
	float wo_w, wo_h;

	di->world_rect = *rect;
	
	sc_w = BLI_rcti_size_x(&di->screen_rect);
	sc_h = BLI_rcti_size_y(&di->screen_rect);
	wo_w = BLI_rcti_size_x(&di->world_rect);
	wo_h = BLI_rcti_size_y(&di->world_rect);
	
	di->wo_to_sc[0] = sc_w / wo_w;
	di->wo_to_sc[1] = sc_h / wo_h;
}

/** Save the current OpenGL state and initialize OpenGL for 2D
 * rendering. glaEnd2DDraw should be called on the returned structure
 * to free it and to return OpenGL to its previous state. The
 * scissor rectangle is set to match the viewport.
 *
 * See glaDefine2DArea for an explanation of why this function uses integers.
 *
 * \param screen_rect The screen rectangle to be used for 2D drawing.
 * \param world_rect The world rectangle that the 2D area represented
 * by \a screen_rect is supposed to represent. If NULL it is assumed the
 * world has a 1 to 1 mapping to the screen.
 */
gla2DDrawInfo *glaBegin2DDraw(rcti *screen_rect, rctf *world_rect) 
{
	gla2DDrawInfo *di = MEM_mallocN(sizeof(*di), "gla2DDrawInfo");
	int sc_w, sc_h;
	float wo_w, wo_h;

	gpuGetSizeBox(GL_VIEWPORT, (GLint *)di->orig_vp);
	gpuGetMatrix(GL_PROJECTION_MATRIX, (GLfloat *)di->orig_projmat);
	gpuGetMatrix(GL_MODELVIEW_MATRIX, (GLfloat *)di->orig_viewmat);

	di->screen_rect = *screen_rect;
	if (world_rect) {
		di->world_rect = *world_rect;
	}
	else {
		di->world_rect.xmin = di->screen_rect.xmin;
		di->world_rect.ymin = di->screen_rect.ymin;
		di->world_rect.xmax = di->screen_rect.xmax;
		di->world_rect.ymax = di->screen_rect.ymax;
	}

	sc_w = BLI_rcti_size_x(&di->screen_rect);
	sc_h = BLI_rcti_size_y(&di->screen_rect);
	wo_w = BLI_rcti_size_x(&di->world_rect);
	wo_h = BLI_rcti_size_y(&di->world_rect);

	di->wo_to_sc[0] = sc_w / wo_w;
	di->wo_to_sc[1] = sc_h / wo_h;

	glaDefine2DArea(&di->screen_rect);

	return di;
}

/**
 * Translate the (\a wo_x, \a wo_y) point from world coordinates into screen space.
 */
void gla2DDrawTranslatePt(gla2DDrawInfo *di, float wo_x, float wo_y, int *sc_x_r, int *sc_y_r)
{
	*sc_x_r = (wo_x - di->world_rect.xmin) * di->wo_to_sc[0];
	*sc_y_r = (wo_y - di->world_rect.ymin) * di->wo_to_sc[1];
}

/**
 * Translate the \a world point from world coordiantes into screen space.
 */
void gla2DDrawTranslatePtv(gla2DDrawInfo *di, float world[2], int screen_r[2])
{
	screen_r[0] = (world[0] - di->world_rect.xmin) * di->wo_to_sc[0];
	screen_r[1] = (world[1] - di->world_rect.ymin) * di->wo_to_sc[1];
}

/**
 * Restores the previous OpenGL state and free's the auxilary gla data.
 */
void glaEnd2DDraw(gla2DDrawInfo *di)
{
	gpuViewport(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	gpuScissor(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	gpuMatrixMode(GL_PROJECTION);
	gpuLoadMatrix(di->orig_projmat);
	gpuMatrixMode(GL_MODELVIEW);
	gpuLoadMatrix(di->orig_viewmat);

	MEM_freeN(di);
}
#endif

/* Uses current OpenGL state to get view matrices for gluProject/gluUnProject */
void bgl_get_mats(bglMats *mats)
{
	const double badvalue = 1.0e-6;

	gpuGetMatrix(GL_MODELVIEW_MATRIX, mats->modelview);
	gpuGetMatrix(GL_PROJECTION_MATRIX, mats->projection);
	gpuGetSizeBox(GL_VIEWPORT, (GLint *)mats->viewport);
	
	/* Very strange code here - it seems that certain bad values in the
	 * modelview matrix can cause gluUnProject to give bad results. */
	if (mats->modelview[0] < badvalue &&
	    mats->modelview[0] > -badvalue)
	{
		mats->modelview[0] = 0;
	}
	if (mats->modelview[5] < badvalue &&
	    mats->modelview[5] > -badvalue)
	{
		mats->modelview[5] = 0;
	}
	
	/* Set up viewport so that gluUnProject will give correct values */
	mats->viewport[0] = 0;
	mats->viewport[1] = 0;
}

/* *************** glPolygonOffset hack ************* */

/* dist is only for ortho now... */
void bglPolygonOffset(float viewdist, float dist) 
{
	static float winmat[16], offset = 0.0;
	
	if (dist != 0.0f) {
		float offs;
		
		// glEnable(GL_POLYGON_OFFSET_FILL);
		// glPolygonOffset(-1.0, -1.0);

		/* hack below is to mimic polygon offset */
		gpuMatrixMode(GL_PROJECTION);
		gpuGetMatrix(GL_PROJECTION_MATRIX, (float *)winmat);

		/* dist is from camera to center point */
		
		if (winmat[15] > 0.5f) offs = 0.00001f * dist * viewdist;  // ortho tweaking
		else offs = 0.0005f * dist;  // should be clipping value or so...

		winmat[14] -= offs;
		offset += offs;

		gpuLoadMatrix(winmat);
		gpuMatrixMode(GL_MODELVIEW);
	}
	else {
		gpuMatrixMode(GL_PROJECTION);
		winmat[14] += offset;
		offset = 0.0;
		gpuLoadMatrix(winmat);
		gpuMatrixMode(GL_MODELVIEW);
	}
}


/* **** Color management helper functions for GLSL display/transform ***** */

/* Draw given image buffer on a screen using GLSL for display transform */
void glaDrawImBuf_glsl(ImBuf *ibuf, float x, float y, int zoomfilter,
                       ColorManagedViewSettings *view_settings,
                       ColorManagedDisplaySettings *display_settings)
{
	bool force_fallback = false;
	bool need_fallback = true;

	/* Early out */
	if (ibuf->rect == NULL && ibuf->rect_float == NULL)
		return;

	/* Dithering is not supported on GLSL yet */
	force_fallback |= ibuf->dither != 0.0f;

	/* Single channel images could not be transformed using GLSL yet */
	force_fallback |= ibuf->channels == 1;

	/* If user decided not to use GLSL, fallback to glaDrawPixelsAuto */
	force_fallback |= (U.image_draw_method != IMAGE_DRAW_METHOD_GLSL);

	/* This is actually lots of crap, but currently not sure about
	 * more clear way to bypass partial buffer update crappyness
	 * while rendering.
	 *
	 * The thing is -- render engines are only updating byte and
	 * display buffers for active render result opened in image
	 * editor. This works fine to show render progress without
	 * switching render layers in image editor user, but this is
	 * completely useless for GLSL display, where we need to have
	 * original buffer which we could color manage.
	 *
	 * For the time of rendering, we'll stick back to slower CPU
	 * display buffer update. GLSL could be used as soon as some
	 * fixes (?) are done in render itself, so we'll always have
	 * image buffer with relevant float buffer opened while
	 * rendering.
	 *
	 * On the other hand, when using Cycles, stressing GPU with
	 * GLSL could backfire on a performance.
	 *                                         - sergey -
	 */
	if (G.is_rendering) {
		/* Try to detect whether we're drawing render result,
		 * other images could have both rect and rect_float
		 * but they'll be synchronized
		 */
		if (ibuf->rect_float && ibuf->rect &&
		    ((ibuf->mall & IB_rectfloat) == 0))
		{
			force_fallback = true;
		}
	}

	/* Try to draw buffer using GLSL display transform */
	if (force_fallback == false) {
		int ok;

		if (ibuf->rect_float) {
			if (ibuf->float_colorspace) {
				ok = IMB_colormanagement_setup_glsl_draw_from_space(view_settings, display_settings,
				                                                    ibuf->float_colorspace, TRUE);
			}
			else {
				ok = IMB_colormanagement_setup_glsl_draw(view_settings, display_settings, TRUE);
			}
		}
		else {
			ok = IMB_colormanagement_setup_glsl_draw_from_space(view_settings, display_settings,
			                                                    ibuf->rect_colorspace, FALSE);
		}

		if (ok) {
#if defined(WITH_GL_PROFILE_COMPAT)
			//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // XXX jwilkins: blender never changes the TEXTURE_ENV_MODE
#endif

			gpuCurrentColor3x(CPACK_WHITE);

			if (ibuf->rect_float) {
				int format = 0;

				if (ibuf->channels == 3)
					format = GL_RGB;
				else if (ibuf->channels == 4)
					format = GL_RGBA;
				else
					BLI_assert(!"Incompatible number of channels for GLSL display");

				if (format != 0) {
					glaDrawPixelsTex(x, y, ibuf->x, ibuf->y, format, GL_FLOAT,
					                 zoomfilter, ibuf->rect_float);
				}
			}
			else if (ibuf->rect) {
				/* ibuf->rect is always RGBA */
				glaDrawPixelsTex(x, y, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE,
				                 zoomfilter, ibuf->rect);
			}

			IMB_colormanagement_finish_glsl_draw();

			need_fallback = false;
		}
	}

	/* In case GLSL failed or not usable, fallback to glaDrawPixelsAuto */
	if (need_fallback) {
		unsigned char *display_buffer;
		void *cache_handle;

		display_buffer = IMB_display_buffer_acquire(ibuf, view_settings, display_settings, &cache_handle);

		if (display_buffer)
			glaDrawPixelsAuto(x, y, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE,
			                  zoomfilter, display_buffer);

		IMB_display_buffer_release(cache_handle);
	}
}

void glaDrawImBuf_glsl_ctx(const bContext *C, ImBuf *ibuf, float x, float y, int zoomfilter)
{
	ColorManagedViewSettings *view_settings;
	ColorManagedDisplaySettings *display_settings;

	IMB_colormanagement_display_settings_from_ctx(C, &view_settings, &display_settings);

	glaDrawImBuf_glsl(ibuf, x, y, zoomfilter, view_settings, display_settings);
}

/* Transform buffer from role to scene linear space using GLSL OCIO conversion
 *
 * See IMB_colormanagement_setup_transform_from_role_glsl description for
 * some more details
 *
 * NOTE: this only works for RGBA buffers!
 */
int glaBufferTransformFromRole_glsl(float *buffer, int width, int height, int role)
{
	GPUOffScreen *ofs;
	char err_out[256];
	rcti display_rect;

	ofs = GPU_offscreen_create(width, height, err_out);

	if (!ofs)
		return FALSE;

	GPU_offscreen_bind(ofs);

	if (!IMB_colormanagement_setup_transform_from_role_glsl(role, TRUE)) {
		GPU_offscreen_unbind(ofs);
		GPU_offscreen_free(ofs);
		return FALSE;
	}

	BLI_rcti_init(&display_rect, 0, width, 0, height);

	gpuMatrixMode(GL_PROJECTION);
	gpuPushMatrix();
	gpuMatrixMode(GL_MODELVIEW);
	gpuPushMatrix();

	glaDefine2DArea(&display_rect);

	glaDrawPixelsTex(0, 0, width, height, GL_RGBA, GL_FLOAT,
	                 GL_NEAREST, buffer);

	gpuMatrixMode(GL_PROJECTION);
	gpuPopMatrix();
	gpuMatrixMode(GL_MODELVIEW);
	gpuPopMatrix();

	GPU_offscreen_read_pixels(ofs, GL_FLOAT, buffer);

	IMB_colormanagement_finish_glsl_transform();

	/* unbind */
	GPU_offscreen_unbind(ofs);
	GPU_offscreen_free(ofs);

	return TRUE;
}