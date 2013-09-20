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

/** \file blender/editors/interface/interface_draw.c
 *  \ingroup edinterface
 */

/* my interface */
#include "interface_intern.h"

/* my library */
#include "UI_interface.h"

/* external */

#include "BIF_glutil.h"

#include "BKE_colortools.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_tracking.h"

#include "BLF_api.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_color_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_movieclip_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "GPU_basic.h"
#include "GPU_blender_aspect.h"
#include "GPU_colors.h"
#include "GPU_lighting.h"
#include "GPU_matrix.h"
#include "GPU_pixels.h"
#include "GPU_primitives.h"
#include "GPU_raster.h"
#include "GPU_sprite.h"

/* standard */
#include <math.h>
#include <string.h>



static int roundboxtype = UI_CNR_ALL;

void uiSetRoundBox(int type)
{
	/* Not sure the roundbox function is the best place to change this
	 * if this is undone, its not that big a deal, only makes curves edges
	 * square for the  */
	roundboxtype = type;
	
}

int uiGetRoundBox(void)
{
	return roundboxtype;
}

void uiDrawBox(int mode, float minx, float miny, float maxx, float maxy, float rad)
{
	float vec[7][2] = {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                   {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	int a;
	
	/* mult */
	for (a = 0; a < 7; a++) {
		mul_v2_fl(vec[a], rad);
	}

	gpuImmediateFormat_V2(); // DOODLE: ui box, a rounded rectangle
	gpuBegin(mode);



	/* start with corner right-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		gpuVertex2f(maxx - rad, miny);
		for (a = 0; a < 7; a++) {
			gpuVertex2f(maxx - rad + vec[a][0], miny + vec[a][1]);
		}
		gpuVertex2f(maxx, miny + rad);
	}
	else {
		gpuVertex2f(maxx, miny);
	}

	/* corner right-top */
	if (roundboxtype & UI_CNR_TOP_RIGHT) {
		gpuVertex2f(maxx, maxy - rad);
		for (a = 0; a < 7; a++) {
			gpuVertex2f(maxx - vec[a][1], maxy - rad + vec[a][0]);
		}
		gpuVertex2f(maxx - rad, maxy);
	}
	else {
		gpuVertex2f(maxx, maxy);
	}

	/* corner left-top */
	if (roundboxtype & UI_CNR_TOP_LEFT) {
		gpuVertex2f(minx + rad, maxy);
		for (a = 0; a < 7; a++) {
			gpuVertex2f(minx + rad - vec[a][0], maxy - vec[a][1]);
		}
		gpuVertex2f(minx, maxy - rad);
	}
	else {
		gpuVertex2f(minx, maxy);
	}

	/* corner left-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
		gpuVertex2f(minx, miny + rad);
		for (a = 0; a < 7; a++) {
			gpuVertex2f(minx + vec[a][1], miny + rad - vec[a][0]);
		}
		gpuVertex2f(minx + rad, miny);
	}
	else {
		gpuVertex2f(minx, miny);
	}

	gpuEnd();
	gpuImmediateUnformat();
}

static void round_box_shade_col(const float col1[3], float const col2[3], const float fac)
{
	float col[3];

	col[0] = (fac * col1[0] + (1.0f - fac) * col2[0]);
	col[1] = (fac * col1[1] + (1.0f - fac) * col2[1]);
	col[2] = (fac * col1[2] + (1.0f - fac) * col2[2]);
	gpuColor3fv(col);
}

/* linear horizontal shade within button or in outline */
/* view2d scrollers use it */
void uiDrawBoxShade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadetop, float shadedown)
{
	float vec[7][2] = {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                   {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	const float div = maxy - miny;
	const float idiv = 1.0f / div;
	float coltop[3], coldown[3], color[4];
	int a;
	
	/* mult */
	for (a = 0; a < 7; a++) {
		mul_v2_fl(vec[a], rad);
	}
	/* get current color, needs to be outside of gpuBegin/End */
	gpuGetColor4fv(color);

	/* 'shade' defines strength of shading */
	coltop[0]  = min_ff(1.0f, color[0] + shadetop);
	coltop[1]  = min_ff(1.0f, color[1] + shadetop);
	coltop[2]  = min_ff(1.0f, color[2] + shadetop);
	coldown[0] = max_ff(0.0f, color[0] + shadedown);
	coldown[1] = max_ff(0.0f, color[1] + shadedown);
	coldown[2] = max_ff(0.0f, color[2] + shadedown);

	// SSS Enable Smooth
	GPU_aspect_enable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);

	gpuBegin(mode);

	/* start with corner right-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		
		round_box_shade_col(coltop, coldown, 0.0);
		gpuVertex2f(maxx - rad, miny);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(coltop, coldown, vec[a][1] * idiv);
			gpuVertex2f(maxx - rad + vec[a][0], miny + vec[a][1]);
		}
		
		round_box_shade_col(coltop, coldown, rad * idiv);
		gpuVertex2f(maxx, miny + rad);
	}
	else {
		round_box_shade_col(coltop, coldown, 0.0);
		gpuVertex2f(maxx, miny);
	}
	
	/* corner right-top */
	if (roundboxtype & UI_CNR_TOP_RIGHT) {
		
		round_box_shade_col(coltop, coldown, (div - rad) * idiv);
		gpuVertex2f(maxx, maxy - rad);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(coltop, coldown, (div - rad + vec[a][1]) * idiv);
			gpuVertex2f(maxx - vec[a][1], maxy - rad + vec[a][0]);
		}
		round_box_shade_col(coltop, coldown, 1.0);
		gpuVertex2f(maxx - rad, maxy);
	}
	else {
		round_box_shade_col(coltop, coldown, 1.0);
		gpuVertex2f(maxx, maxy);
	}
	
	/* corner left-top */
	if (roundboxtype & UI_CNR_TOP_LEFT) {
		
		round_box_shade_col(coltop, coldown, 1.0);
		gpuVertex2f(minx + rad, maxy);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(coltop, coldown, (div - vec[a][1]) * idiv);
			gpuVertex2f(minx + rad - vec[a][0], maxy - vec[a][1]);
		}
		
		round_box_shade_col(coltop, coldown, (div - rad) * idiv);
		gpuVertex2f(minx, maxy - rad);
	}
	else {
		round_box_shade_col(coltop, coldown, 1.0);
		gpuVertex2f(minx, maxy);
	}
	
	/* corner left-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
		
		round_box_shade_col(coltop, coldown, rad * idiv);
		gpuVertex2f(minx, miny + rad);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(coltop, coldown, (rad - vec[a][1]) * idiv);
			gpuVertex2f(minx + vec[a][1], miny + rad - vec[a][0]);
		}
		
		round_box_shade_col(coltop, coldown, 0.0);
		gpuVertex2f(minx + rad, miny);
	}
	else {
		round_box_shade_col(coltop, coldown, 0.0);
		gpuVertex2f(minx, miny);
	}
	
	gpuEnd();

	// SSS Disable Smooth
	GPU_aspect_disable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);
}

/* linear vertical shade within button or in outline */
/* view2d scrollers use it */
void uiDrawBoxVerticalShade(int mode, float minx, float miny, float maxx, float maxy,
                            float rad, float shadeLeft, float shadeRight)
{
	float vec[7][2] = {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                   {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	const float div = maxx - minx;
	const float idiv = 1.0f / div;
	float colLeft[3], colRight[3], color[4];
	int a;
	
	/* mult */
	for (a = 0; a < 7; a++) {
		mul_v2_fl(vec[a], rad);
	}
	/* get current color, needs to be outside of gpuBegin/End */
	gpuGetColor4fv(color);

	/* 'shade' defines strength of shading */
	colLeft[0]  = min_ff(1.0f, color[0] + shadeLeft);
	colLeft[1]  = min_ff(1.0f, color[1] + shadeLeft);
	colLeft[2]  = min_ff(1.0f, color[2] + shadeLeft);
	colRight[0] = max_ff(0.0f, color[0] + shadeRight);
	colRight[1] = max_ff(0.0f, color[1] + shadeRight);
	colRight[2] = max_ff(0.0f, color[2] + shadeRight);

	// SSS Enable Smooth
	GPU_aspect_enable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);

	gpuBegin(mode);

	/* start with corner right-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		round_box_shade_col(colLeft, colRight, 0.0);
		gpuVertex2f(maxx - rad, miny);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(colLeft, colRight, vec[a][0] * idiv);
			gpuVertex2f(maxx - rad + vec[a][0], miny + vec[a][1]);
		}
		
		round_box_shade_col(colLeft, colRight, rad * idiv);
		gpuVertex2f(maxx, miny + rad);
	}
	else {
		round_box_shade_col(colLeft, colRight, 0.0);
		gpuVertex2f(maxx, miny);
	}
	
	/* corner right-top */
	if (roundboxtype & UI_CNR_TOP_RIGHT) {
		round_box_shade_col(colLeft, colRight, 0.0);
		gpuVertex2f(maxx, maxy - rad);
		
		for (a = 0; a < 7; a++) {
			
			round_box_shade_col(colLeft, colRight, (div - rad - vec[a][0]) * idiv);
			gpuVertex2f(maxx - vec[a][1], maxy - rad + vec[a][0]);
		}
		round_box_shade_col(colLeft, colRight, (div - rad) * idiv);
		gpuVertex2f(maxx - rad, maxy);
	}
	else {
		round_box_shade_col(colLeft, colRight, 0.0);
		gpuVertex2f(maxx, maxy);
	}
	
	/* corner left-top */
	if (roundboxtype & UI_CNR_TOP_LEFT) {
		round_box_shade_col(colLeft, colRight, (div - rad) * idiv);
		gpuVertex2f(minx + rad, maxy);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(colLeft, colRight, (div - rad + vec[a][0]) * idiv);
			gpuVertex2f(minx + rad - vec[a][0], maxy - vec[a][1]);
		}
		
		round_box_shade_col(colLeft, colRight, 1.0);
		gpuVertex2f(minx, maxy - rad);
	}
	else {
		round_box_shade_col(colLeft, colRight, 1.0);
		gpuVertex2f(minx, maxy);
	}
	
	/* corner left-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
		round_box_shade_col(colLeft, colRight, 1.0);
		gpuVertex2f(minx, miny + rad);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(colLeft, colRight, (vec[a][0]) * idiv);
			gpuVertex2f(minx + vec[a][1], miny + rad - vec[a][0]);
		}
		
		round_box_shade_col(colLeft, colRight, 1.0);
		gpuVertex2f(minx + rad, miny);
	}
	else {
		round_box_shade_col(colLeft, colRight, 1.0);
		gpuVertex2f(minx, miny);
	}
	
	gpuEnd();

	// SSS Disable Smooth
	GPU_aspect_disable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);
}

/* plain antialiased unfilled rectangle */
void uiRoundRect(float minx, float miny, float maxx, float maxy, float rad)
{
	if (roundboxtype & UI_RB_ALPHA) {
		gpuAlpha(0.5f);
		glEnable(GL_BLEND);
	}

	/* set antialias line */
	GPU_raster_begin();

	GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	glEnable(GL_BLEND);

	uiDrawBox(GL_LINE_LOOP, minx, miny, maxx, maxy, rad);

	glDisable(GL_BLEND);

	GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	GPU_raster_end();
}

/* (old, used in outliner) plain antialiased filled box */
void uiRoundBox(float minx, float miny, float maxx, float maxy, float rad)
{
	ui_draw_anti_roundbox(GL_TRIANGLE_FAN, minx, miny, maxx, maxy, rad, roundboxtype & UI_RB_ALPHA);
}

/* ************** SPECIAL BUTTON DRAWING FUNCTIONS ************* */

void ui_draw_but_IMAGE(ARegion *UNUSED(ar), uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *rect)
{
#ifdef WITH_HEADLESS
	(void)rect;
	(void)but;
#else
	ImBuf *ibuf = (ImBuf *)but->poin;
	//GLint scissor[4];
	int w, h;
	bool do_zoom;

	if (!ibuf) return;
	
	w = BLI_rcti_size_x(rect);
	h = BLI_rcti_size_y(rect);
	
	/* scissor doesn't seem to be doing the right thing...? */
#if 0
	//gpuColor3P(CPACK_RED);
	//gpuSingleWireRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax)

	/* prevent drawing outside widget area */
	glGetIntegerv(GL_SCISSOR_BOX, scissor);
	glScissor(ar->winrct.xmin + rect->xmin, ar->winrct.ymin + rect->ymin, w, h);
#endif
	
	glEnable(GL_BLEND);
	gpuColor4P(CPACK_BLACK, 0.000f);

	do_zoom = w != ibuf->x || h != ibuf->y;

	if (do_zoom) {
		float facx = (float)w / (float)ibuf->x;
		float facy = (float)h / (float)ibuf->y;
		GPU_pixels_zoom(facx, facy);
	}

	glaDrawPixelsAuto((float)rect->xmin, (float)rect->ymin, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST, ibuf->rect);
	
	if (do_zoom) {
		GPU_pixels_zoom(1.0f, 1.0f); /* restore default value */
	}
	
	glDisable(GL_BLEND);
	
#if 0
	// restore scissortest
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
#endif
	
#endif
}

static void draw_scope_end(const rctf *rect, GLint *scissor)
{
	float scaler_x1, scaler_x2;

	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

	/* scale widget */
	scaler_x1 = rect->xmin + BLI_rctf_size_x(rect) / 2 - SCOPE_RESIZE_PAD;
	scaler_x2 = rect->xmin + BLI_rctf_size_x(rect) / 2 + SCOPE_RESIZE_PAD;

	gpuImmediateFormat_C4_V2(); // DOODLE: fixed number of colored lines
	gpuBegin(GL_LINES);

	gpuColor4P(CPACK_BLACK, 0.250f);
	gpuAppendLinef(scaler_x1, rect->ymin - 4, scaler_x2, rect->ymin - 4);
	gpuAppendLinef(scaler_x1, rect->ymin - 7, scaler_x2, rect->ymin - 7);

	gpuColor4P(CPACK_WHITE, 0.250f);
	gpuAppendLinef(scaler_x1, rect->ymin - 5, scaler_x2, rect->ymin - 5);
	gpuAppendLinef(scaler_x1, rect->ymin - 8, scaler_x2, rect->ymin - 8);

	gpuEnd();
	gpuImmediateUnformat();

	/* outline */
	gpuColor4P(CPACK_BLACK, 0.500f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_LINE_LOOP, rect->xmin - 1, rect->ymin, rect->xmax + 1, rect->ymax + 1, 3.0f);
}

static void histogram_draw_one(float r, float g, float b, float alpha,
                               float x, float y, float w, float h, float *data, int res, const short is_line)
{
	int i;

	if (is_line) {
		gpuLineWidth(1.5);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE); /* non-standard blend function */
		gpuColor4f(r, g, b, alpha);

		/* curve outline */

		GPU_raster_begin();

		GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

		gpuBegin(GL_LINE_STRIP);
		for (i = 0; i < res; i++) {
			float x2 = x + i * (w / (float)res);
			gpuVertex2f(x2, y + (data[i] * h));
		}
		gpuEnd();

		gpuLineWidth(1.0);

		GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

		GPU_raster_end();

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); /* reset blender default */
	}
	else {
		/* under the curve */
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		gpuColor4f(r, g, b, alpha);

		// SSS Disable Smooth
		GPU_aspect_disable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);

		gpuBegin(GL_TRIANGLE_STRIP); // DOODLE: line graph drawn using quads, locking done by function callee
		gpuVertex2f(x, y);
		gpuVertex2f(x, y + (data[0] * h));
		for (i = 1; i < res; i++) {
			float x2 = x + i * (w / (float)res);
			gpuVertex2f(x2, y + (data[i] * h));
			gpuVertex2f(x2, y);
		}
		gpuEnd();

		/* curve outline */
		gpuColor4P(CPACK_BLACK, 0.250f);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); /* reset blender default */

		GPU_raster_begin();

		GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

		gpuBegin(GL_LINE_STRIP); // DOODLE: line graph drawn using a line strip, locking done by callee
		for (i = 0; i < res; i++) {
			float x2 = x + i * (w / (float)res);
			gpuVertex2f(x2, y + (data[i] * h));
		}
		gpuEnd();

		GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

		GPU_raster_end();
	}
}

#define HISTOGRAM_TOT_GRID_LINES 4

void ui_draw_but_HISTOGRAM(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *recti)
{
	Histogram *hist = (Histogram *)but->poin;
	int res = hist->x_resolution;
	rctf rect;
	int i;
	float w, h;
	const short is_line = (hist->flag & HISTO_FLAG_LINE) != 0;
	GLint scissor[4];
	
	rect.xmin = (float)recti->xmin + 1;
	rect.xmax = (float)recti->xmax - 1;
	rect.ymin = (float)recti->ymin + SCOPE_RESIZE_PAD + 2;
	rect.ymax = (float)recti->ymax - 1;
	
	w = BLI_rctf_size_x(&rect);
	h = BLI_rctf_size_y(&rect) * hist->ymax;
	
	glEnable(GL_BLEND);

	gpuColor4P(CPACK_BLACK, 0.300f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_TRIANGLE_FAN, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f);

	/* need scissor test, histogram can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	gpuColor4P(CPACK_WHITE, 0.080f);

	gpuImmediateFormat_V2(); /* lock both for grid and histogram */ // DOODLE: 4 monochrome lines and 1 or 3 histograms

	/* draw grid lines here */
	for (i = 1; i < (HISTOGRAM_TOT_GRID_LINES + 1); i++) {
		const float fac = (float)i / (float)HISTOGRAM_TOT_GRID_LINES;

		/* so we can tell the 1.0 color point */
		if (i == HISTOGRAM_TOT_GRID_LINES) {
			gpuColor4f(1.0f, 1.0f, 1.0f, 0.5f);
		}

		gpuAppendLinef(rect.xmin, rect.ymin + fac * h, rect.xmax, rect.ymin + fac * h);
		gpuAppendLinef(rect.xmin + fac * w, rect.ymin, rect.xmin + fac * w, rect.ymax);
	}
	gpuEnd();

	if (hist->mode == HISTO_MODE_LUMA) {
		histogram_draw_one(1.0, 1.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_luma, res, is_line);
	}
	else if (hist->mode == HISTO_MODE_ALPHA) {
		histogram_draw_one(1.0, 1.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_a, res, is_line);
	}
	else {
		if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_R)
			histogram_draw_one(1.0, 0.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_r, res, is_line);
		if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_G)
			histogram_draw_one(0.0, 1.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_g, res, is_line);
		if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_B)
			histogram_draw_one(0.0, 0.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_b, res, is_line);
	}

	gpuImmediateUnformat();

	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);
}

#undef HISTOGRAM_TOT_GRID_LINES

void ui_draw_but_WAVEFORM(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *recti)
{
	Scopes *scopes = (Scopes *)but->poin;
	rctf rect;
	int i, c;
	float w, w3, h, alpha, yofs;
	GLint scissor[4];
	float colors[3][3] = MAT3_UNITY;
	float colorsycc[3][3] = {{1, 0, 1}, {1, 1, 0}, {0, 1, 1}};
	float colors_alpha[3][3], colorsycc_alpha[3][3]; /* colors  pre multiplied by alpha for speed up */
	float min, max;

	if (scopes == NULL) return;

	rect.xmin = (float)recti->xmin + 1;
	rect.xmax = (float)recti->xmax - 1;
	rect.ymin = (float)recti->ymin + SCOPE_RESIZE_PAD + 2;
	rect.ymax = (float)recti->ymax - 1;

	if (scopes->wavefrm_yfac < 0.5f)
		scopes->wavefrm_yfac = 0.98f;
	w = BLI_rctf_size_x(&rect) - 7;
	h = BLI_rctf_size_y(&rect) * scopes->wavefrm_yfac;
	yofs = rect.ymin + (BLI_rctf_size_y(&rect) - h) / 2.0f;
	w3 = w / 3.0f;

	/* log scale for alpha */
	alpha = scopes->wavefrm_alpha * scopes->wavefrm_alpha;

	for (c = 0; c < 3; c++) {
		for (i = 0; i < 3; i++) {
			colors_alpha[c][i] = colors[c][i] * alpha;
			colorsycc_alpha[c][i] = colorsycc[c][i] * alpha;
		}
	}

	glEnable(GL_BLEND);

	gpuColor4P(CPACK_BLACK, 0.300f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_TRIANGLE_FAN, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f);
	

	/* need scissor test, waveform can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	gpuColor4P(CPACK_WHITE, 0.080f);

	/* draw grid lines here */
	gpuImmediateFormat_V2(); // DOODLE: fixed number of monochrome lines, a grid
	gpuBegin(GL_LINES);
	for (i = 0; i < 6; i++) {
		gpuAppendLinef(rect.xmin + 22, yofs + (i / 5.f) * h, rect.xmax + 1, yofs + (i / 5.f) * h);
	}
	gpuEnd();
	gpuImmediateUnformat();

	/* draw text on grid */
	BLF_draw_default_lock(); // DOODLE: grid of numbers
	for (i = 0; i < 6; i++) {
		char str[4];
		BLI_snprintf(str, sizeof(str), "%-3d", i * 20);
		str[3] = '\0';
		BLF_draw_default(rect.xmin + 1, yofs - 5 + (i / 5.f) * h, 0, str, sizeof(str) - 1);
	}
	BLF_draw_default_unlock();

	gpuImmediateFormat_C4_V2(); // DOODLE: variable number of lines, colors passed mainly to reduce number of batches
	gpuBegin(GL_LINES);

	/* 3 vertical separation */
	if (scopes->wavefrm_mode != SCOPES_WAVEFRM_LUMA) {
		for (i = 1; i < 3; i++) {
			gpuAppendLinef(rect.xmin + i * w3, rect.ymin, rect.xmin + i * w3, rect.ymax);
		}
	}

	/* separate min max zone on the right */
	gpuAppendLinef(rect.xmin + w, rect.ymin, rect.xmin + w, rect.ymax);
	/* 16-235-240 level in case of ITU-R BT601/709 */
	gpuColor4f(1.0f, 0.4f, 0.0f, 0.200f);
	if (ELEM(scopes->wavefrm_mode, SCOPES_WAVEFRM_YCC_601, SCOPES_WAVEFRM_YCC_709)) {
		gpuAppendLinef(rect.xmin + 22, yofs + h * 16.0f / 255.0f, rect.xmax + 1, yofs + h * 16.0f / 255.0f);
		gpuAppendLinef(rect.xmin + 22, yofs + h * 235.0f / 255.0f, rect.xmin + w3, yofs + h * 235.0f / 255.0f);
		gpuAppendLinef(rect.xmin + 3 * w3, yofs + h * 235.0f / 255.0f, rect.xmax + 1, yofs + h * 235.0f / 255.0f);
		gpuAppendLinef(rect.xmin + w3, yofs + h * 240.0f / 255.0f, rect.xmax + 1, yofs + h * 240.0f / 255.0f);
	}
	/* 7.5 IRE black point level for NTSC */
	if (scopes->wavefrm_mode == SCOPES_WAVEFRM_LUMA) {
		gpuAppendLinef(rect.xmin, yofs + h * 0.075f, rect.xmax + 1, yofs + h * 0.075f);
	}

	gpuEnd();
	gpuImmediateUnformat();

	if (scopes->ok && scopes->waveform_1 != NULL) {
		GPUarrays arrays = GPU_ARRAYS_V2F;

		glBlendFunc(GL_ONE, GL_ONE); /* non-standard blend function */

		gpuImmediateFormat_V2();

		/* LUMA (1 channel) */
		gpuGray3f(alpha);
		if (scopes->wavefrm_mode == SCOPES_WAVEFRM_LUMA) {

			gpuPushMatrix();
			gpuTranslate(rect.xmin, yofs, 0);
			gpuScale(w, h, 0);

			arrays.vertexPointer = scopes->waveform_1;
			gpuDrawClientArrays(GL_POINTS, &arrays, 0, scopes->waveform_tot);

			gpuPopMatrix();

			/* min max */
			gpuGray3f(0.500f);
			min = yofs + scopes->minmax[0][0] * h;
			max = yofs + scopes->minmax[0][1] * h;
			CLAMP(min, rect.ymin, rect.ymax);
			CLAMP(max, rect.ymin, rect.ymax);
			gpuDrawLinef(rect.xmax - 3, min, rect.xmax - 3, max);
		}
		/* RGB / YCC (3 channels) */
		else if (ELEM4(scopes->wavefrm_mode,
		               SCOPES_WAVEFRM_RGB,
		               SCOPES_WAVEFRM_YCC_601,
		               SCOPES_WAVEFRM_YCC_709,
		               SCOPES_WAVEFRM_YCC_JPEG))
		{
			GPUarrays arrays = GPU_ARRAYS_V2F;

			int rgb = (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB);

			gpuPushMatrix();

			gpuTranslate(rect.xmin, yofs, 0);
			gpuScale(w3, h, 0);

			gpuColor3fv((rgb) ? colors_alpha[0] : colorsycc_alpha[0]);
			arrays.vertexPointer = scopes->waveform_1;
			gpuDrawClientArrays(GL_POINTS, &arrays, 0, scopes->waveform_tot);

			gpuTranslate(1, 0, 0);
			gpuColor3fv((rgb) ? colors_alpha[1] : colorsycc_alpha[1]);
			arrays.vertexPointer = scopes->waveform_2;
			gpuDrawClientArrays(GL_POINTS, &arrays, 0, scopes->waveform_tot);

			gpuTranslate(1, 0, 0);
			gpuColor3fv((rgb) ? colors_alpha[2] : colorsycc_alpha[2]);
			arrays.vertexPointer = scopes->waveform_3;
			gpuDrawClientArrays(GL_POINTS, &arrays, 0, scopes->waveform_tot);

			gpuPopMatrix();

			/* min max */
			for (c = 0; c < 3; c++) {
				if (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB) {
					gpuColor3f(colors[c][0] * 0.75f, colors[c][1] * 0.75f, colors[c][2] * 0.75f);
				}
				else {
					gpuColor3f(colorsycc[c][0] * 0.75f, colorsycc[c][1] * 0.75f, colorsycc[c][2] * 0.75f);
				}

				min = yofs + scopes->minmax[c][0] * h;
				max = yofs + scopes->minmax[c][1] * h;
				CLAMP(min, rect.ymin, rect.ymax);
				CLAMP(max, rect.ymin, rect.ymax);

				gpuDrawLinef(rect.xmin + w + 2 + c * 2, min, rect.xmin + w + 2 + c * 2, max); // DOODLE: single line
			}
		}

		gpuImmediateUnformat();

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); /* reset blender default */
	}

	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);
}

static float polar_to_x(float center, float diam, float ampli, float angle)
{
	return center + diam *ampli * cosf(angle);
}

static float polar_to_y(float center, float diam, float ampli, float angle)
{
	return center + diam *ampli * sinf(angle);
}

static void vectorscope_draw_target(float centerx, float centery, float diam, const float colf[3])
{
	float y, u, v;
	float tangle = 0.f, tampli;
	float dangle, dampli, dangle2, dampli2;

	rgb_to_yuv(colf[0], colf[1], colf[2], &y, &u, &v);
	if (u > 0 && v >= 0) tangle = atanf(v / u);
	else if (u > 0 && v < 0) tangle = atanf(v / u) + 2.0f * (float)M_PI;
	else if (u < 0) tangle = atanf(v / u) + (float)M_PI;
	else if (u == 0 && v > 0.0f) tangle = (float)M_PI / 2.0f;
	else if (u == 0 && v < 0.0f) tangle = -(float)M_PI / 2.0f;
	tampli = sqrtf(u * u + v * v);

	/* small target vary by 2.5 degree and 2.5 IRE unit */
	gpuColor4P(CPACK_WHITE, 0.120f);

	dangle = DEG2RADF(2.5f);
	dampli = 2.5f / 200.0f;

	gpuBegin(GL_LINE_STRIP);
	gpuVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle + dangle), polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle + dangle), polar_to_y(centery, diam, tampli - dampli, tangle + dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle - dangle), polar_to_y(centery, diam, tampli - dampli, tangle - dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle - dangle), polar_to_y(centery, diam, tampli + dampli, tangle - dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle + dangle), polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
	gpuEnd();
	/* big target vary by 10 degree and 20% amplitude */
	dangle = DEG2RADF(10.0f);
	dampli = 0.2f * tampli;
	dangle2 = DEG2RADF(5.0f);
	dampli2 = 0.5f * dampli;
	gpuBegin(GL_LINE_STRIP);
	gpuVertex2f(polar_to_x(centerx, diam, tampli + dampli - dampli2, tangle + dangle), polar_to_y(centery, diam, tampli + dampli - dampli2, tangle + dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle + dangle), polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle + dangle - dangle2), polar_to_y(centery, diam, tampli + dampli, tangle + dangle - dangle2));
	gpuEnd();
	gpuBegin(GL_LINE_STRIP);
	gpuVertex2f(polar_to_x(centerx, diam, tampli - dampli + dampli2, tangle + dangle), polar_to_y(centery, diam, tampli - dampli + dampli2, tangle + dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle + dangle), polar_to_y(centery, diam, tampli - dampli, tangle + dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle + dangle - dangle2), polar_to_y(centery, diam, tampli - dampli, tangle + dangle - dangle2));
	gpuEnd();
	gpuBegin(GL_LINE_STRIP);
	gpuVertex2f(polar_to_x(centerx, diam, tampli - dampli + dampli2, tangle - dangle), polar_to_y(centery, diam, tampli - dampli + dampli2, tangle - dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle - dangle), polar_to_y(centery, diam, tampli - dampli, tangle - dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle - dangle + dangle2), polar_to_y(centery, diam, tampli - dampli, tangle - dangle + dangle2));
	gpuEnd();
	gpuBegin(GL_LINE_STRIP);
	gpuVertex2f(polar_to_x(centerx, diam, tampli + dampli - dampli2, tangle - dangle), polar_to_y(centery, diam, tampli + dampli - dampli2, tangle - dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle - dangle), polar_to_y(centery, diam, tampli + dampli, tangle - dangle));
	gpuVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle - dangle + dangle2), polar_to_y(centery, diam, tampli + dampli, tangle - dangle + dangle2));
	gpuEnd();
}

void ui_draw_but_VECTORSCOPE(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *recti)
{
	const float skin_rad = DEG2RADF(123.0f); /* angle in radians of the skin tone line */
	Scopes *scopes = (Scopes *)but->poin;
	rctf rect;
	int i, j;
	float w, h, centerx, centery, diam;
	float alpha;
	const float colors[6][3] = {
	    {0.75, 0.0, 0.0},  {0.75, 0.75, 0.0}, {0.0, 0.75, 0.0},
	    {0.0, 0.75, 0.75}, {0.0, 0.0, 0.75},  {0.75, 0.0, 0.75}};
	GLint scissor[4];

	rect.xmin = (float)recti->xmin + 1;
	rect.xmax = (float)recti->xmax - 1;
	rect.ymin = (float)recti->ymin + SCOPE_RESIZE_PAD + 2;
	rect.ymax = (float)recti->ymax - 1;

	w = BLI_rctf_size_x(&rect);
	h = BLI_rctf_size_y(&rect);
	centerx = rect.xmin + w / 2;
	centery = rect.ymin + h / 2;
	diam = (w < h) ? w : h;

	alpha = scopes->vecscope_alpha * scopes->vecscope_alpha * scopes->vecscope_alpha;

	glEnable(GL_BLEND);

	gpuColor4P(CPACK_BLACK, 0.300f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_TRIANGLE_FAN, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f);

	/* need scissor test, hvectorscope can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	gpuColor4P(CPACK_WHITE, 0.080f);

	/* draw grid elements */

	/* cross */
	gpuImmediateFormat_V2(); // DOODLE: cross, fixed number of lines, and 5 circles

	gpuBegin(GL_LINES);
	gpuAppendLinef(centerx - (diam / 2) - 5, centery, centerx + (diam / 2) + 5, centery);
	gpuAppendLinef(centerx, centery - (diam / 2) - 5, centerx, centery + (diam / 2) + 5);
	gpuEnd();

	/* circles */
	for (j = 0; j < 5; j++) {
		gpuBegin(GL_LINE_STRIP);
		for (i = 0; i <= 360; i = i + 15) {
			const float a = DEG2RADF((float)i);
			const float r = (j + 1) / 10.0f;
			gpuVertex2f(polar_to_x(centerx, diam, r, a), polar_to_y(centery, diam, r, a));
		}
		gpuEnd();
	}

	/* skin tone line */
	gpuColor4f(1.0f, 0.4f, 0.0f, 0.200f);
	gpuDrawLinef(
		polar_to_x(centerx, diam, 0.5f, skin_rad),
		polar_to_y(centery, diam, 0.5f, skin_rad),
		polar_to_x(centerx, diam, 0.1f, skin_rad),
		polar_to_y(centery, diam, 0.1f, skin_rad));

	/* saturation points */
	for (i = 0; i < 6; i++) {
		vectorscope_draw_target(centerx, centery, diam, colors[i]);
	}

	if (scopes->ok && scopes->vecscope != NULL) {
		GPUarrays arrays = GPU_ARRAYS_V2F;

		glBlendFunc(GL_ONE, GL_ONE); /* non-standard blendfunc */

		/* pixel point cloud */
		gpuGray3f(alpha);

		gpuPushMatrix();
		gpuTranslate(centerx, centery, 0);
		gpuScale(diam, diam, 0);

		arrays.vertexPointer = scopes->vecscope;
		gpuDrawClientArrays(GL_POINTS, &arrays, 0, scopes->waveform_tot);

		gpuPopMatrix();

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); /* reset blender default */
	}

	gpuImmediateUnformat();

	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);

	glDisable(GL_BLEND);
}

void ui_draw_but_COLORBAND(uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *rect)
{
	ColorBand *coba;
	CBData *cbd;
	float x1, y1, sizex, sizey;
	float v3[2], v1[2], v2[2], v1a[2], v2a[2];
	int a;
	float pos, colf[4] = {0, 0, 0, 0}; /* initialize in case the colorband isn't valid */
	struct ColorManagedDisplay *display = NULL;

	coba = (ColorBand *)(but->editcoba ? but->editcoba : but->poin);
	if (coba == NULL) return;

	if (but->block->color_profile)
		display = ui_block_display_get(but->block);

	x1 = rect->xmin;
	y1 = rect->ymin;
	sizex = rect->xmax - x1;
	sizey = rect->ymax - y1;

	gpuImmediateFormat_C4_V2();

	/* first background, to show tranparency */

	gpuColor4ub(UI_TRANSP_DARK, UI_TRANSP_DARK, UI_TRANSP_DARK, 255);
	gpuDrawFilledRectf(x1, y1, x1 + sizex, y1 + sizey);

	GPU_raster_begin();

	GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_POLYGON|GPU_RASTER_STIPPLE);

	gpuColor4ub(UI_TRANSP_LIGHT, UI_TRANSP_LIGHT, UI_TRANSP_LIGHT, 255);
	gpuPolygonStipple(checker_stipple_sml);
	gpuDrawFilledRectf(x1, y1, x1 + sizex, y1 + sizey);

	GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_POLYGON|GPU_RASTER_STIPPLE);

	GPU_raster_end();

	// SSS Enable Smooth
	GPU_aspect_enable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);

	glEnable(GL_BLEND);

	cbd = coba->data;

	v1[0] = v2[0] = x1;
	v1[1] = y1;
	v2[1] = y1 + sizey;

	gpuBegin(GL_TRIANGLE_STRIP);

	gpuColor4fv(&cbd->r);
	gpuVertex2fv(v1);
	gpuVertex2fv(v2);

	for (a = 1; a <= sizex; a++) {
		pos = ((float)a) / (sizex - 1);
		do_colorband(coba, pos, colf);
		if (display)
			IMB_colormanagement_scene_linear_to_display_v3(colf, display);

		v1[0] = v2[0] = x1 + a;

		gpuColor4fv(colf);
		gpuVertex2fv(v1);
		gpuVertex2fv(v2);
	}

	gpuEnd();

	// SSS Disable Smooth
	GPU_aspect_disable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);

	glDisable(GL_BLEND);

	/* outline */
	gpuColor3P(CPACK_BLACK);
	gpuDrawWireRectf(x1, y1, x1 + sizex, y1 + sizey);

	/* help lines */
	v1[0] = v2[0] = v3[0] = x1;
	v1[1] = y1;
	v1a[1] = y1 + 0.25f * sizey;
	v2[1] = y1 + 0.5f * sizey;
	v2a[1] = y1 + 0.75f * sizey;
	v3[1] = y1 + sizey;

	GPU_raster_begin();

	cbd = coba->data;
	gpuBegin(GL_LINES);
	for (a = 0; a < coba->tot; a++, cbd++) {
		v1[0] = v2[0] = v3[0] = v1a[0] = v2a[0] = x1 + cbd->pos * sizex;

		if (a == coba->cur) {
			gpuColor3P(CPACK_BLACK);
			gpuVertex2fv(v1);
			gpuVertex2fv(v3);
			gpuEnd();

			GPU_raster_set_line_style(2);
			gpuBegin(GL_LINES);
			gpuColor3P(CPACK_WHITE);
			gpuVertex2fv(v1);
			gpuVertex2fv(v3);
			gpuEnd();
			GPU_raster_set_line_style(0);
			gpuBegin(GL_LINES);

#if 0
			gpuColor3P(CPACK_BLACK);

			gpuVertex2fv(v1);
			gpuVertex2fv(v1a);

			gpuVertex2fv(v2);
			gpuVertex2fv(v2a);

			gpuColor3P(CPACK_WHITE);

			gpuVertex2fv(v1a);
			gpuVertex2fv(v2);

			gpuVertex2fv(v2a);
			gpuVertex2fv(v3);
#endif
		}
		else {
			gpuColor3P(CPACK_BLACK);
			gpuVertex2fv(v1);
			gpuVertex2fv(v2);

			gpuColor3P(CPACK_WHITE);
			gpuVertex2fv(v2);
			gpuVertex2fv(v3);
		}
	}
	gpuEnd();

	GPU_raster_end();

	gpuImmediateUnformat();
}

void ui_draw_but_NORMAL(uiBut *but, uiWidgetColors *wcol, rcti *rect)
{
	static struct GPUimmediate *displist = NULL;
	static struct GPUindex *index = NULL;

	struct GPUbasiclight light;
	struct GPUbasiclight backup_lights[GPU_MAX_COMMON_LIGHTS];
	int backup_count;

	static const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	static const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	float dir  [4];
	float size;

	backup_count = GPU_get_basic_lights(backup_lights);

	/* backdrop */
	gpuColor3ubv((unsigned char *)wcol->inner);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_TRIANGLE_FAN, rect->xmin, rect->ymin, rect->xmax, rect->ymax, 5.0f);

	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	GPU_aspect_disable(GPU_ASPECT_BASIC, -1);

	/* own light */

	// SSS Enable Lighting
	GPU_aspect_enable(GPU_ASPECT_BASIC, GPU_BASIC_LIGHTING);

	ui_get_but_vectorf(but, dir);
	dir[3] = 0;   /* A zero W component makes a sun lamp. */

	light = GPU_DEFAULT_LIGHT;
	copy_v4_v4(light.position, dir);
	copy_v4_v4(light.diffuse,  white);
	copy_v4_v4(light.specular, black);

	GPU_set_basic_lights(1, &light);

	/* transform to button */
	gpuPushMatrix();
	gpuTranslate(rect->xmin + 0.5f * BLI_rcti_size_x(rect), rect->ymin + 0.5f * BLI_rcti_size_y(rect), 0.0f);
	
	if (BLI_rcti_size_x(rect) < BLI_rcti_size_y(rect))
		size = BLI_rcti_size_x(rect) / 200.f;
	else
		size = BLI_rcti_size_y(rect) / 200.f;

	gpuScale(size, size, size);

	// SSS Enable Smooth
	GPU_aspect_enable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);

	if (!displist) {
		GPUprim3 prim = GPU_PRIM_HIFI_SOLID;
		prim.usegs = 32;
		prim.vsegs = 24;

		gpuPushImmediate();
		gpuImmediateMaxVertexCount(800);
		
		index = gpuNewIndex();
		gpuImmediateIndex(index);
		gpuImmediateMaxIndexCount(4608, GL_UNSIGNED_SHORT);

		/* sphere color */
		gpuColor4fv(white); /* Note: Have to set color here because the immediate context is separate from the main immediate context */

		gpuSingleSphere(&prim, 100);

		displist = gpuPopImmediate();
	}
	else {
		gpuImmediateSingleRepeatElements(displist);
	}

	// SSS Disable Smooth
	GPU_aspect_disable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);

	/* restore */

	// SSS Disable Lighting
	GPU_aspect_disable(GPU_ASPECT_BASIC, GPU_BASIC_LIGHTING);

	GPU_restore_basic_lights(backup_count, backup_lights);

	glDisable(GL_CULL_FACE);
	
	/* AA circle */
	glEnable(GL_BLEND);

	GPU_raster_begin();

	GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	gpuColor3ubv((unsigned char *)wcol->inner);

	gpuSingleFastCircleXY(100.0f);

	GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	GPU_raster_end();

	glDisable(GL_BLEND);

	/* matrix after circle */
	gpuPopMatrix();
}

static void ui_draw_but_curve_grid(rcti *rect, float zoomx, float zoomy, float offsx, float offsy, float step)
{
	float dx, dy, fx, fy;
	
	gpuBegin(GL_LINES);
	dx = step * zoomx;
	fx = rect->xmin + zoomx * (-offsx);
	if (fx > rect->xmin) fx -= dx * (floorf(fx - rect->xmin));
	while (fx < rect->xmax) {
		gpuVertex2f(fx, rect->ymin);
		gpuVertex2f(fx, rect->ymax);
		fx += dx;
	}
	
	dy = step * zoomy;
	fy = rect->ymin + zoomy * (-offsy);
	if (fy > rect->ymin) fy -= dy * (floorf(fy - rect->ymin));
	while (fy < rect->ymax) {
		gpuVertex2f(rect->xmin, fy);
		gpuVertex2f(rect->xmax, fy);
		fy += dy;
	}
	gpuEnd();
	
}

static void gl_shaded_color(unsigned char *col, int shade)
{
	gpuColor3ub(col[0] - shade > 0 ? col[0] - shade : 0,
	           col[1] - shade > 0 ? col[1] - shade : 0,
	           col[2] - shade > 0 ? col[2] - shade : 0);
}

void ui_draw_but_CURVE(ARegion *ar, uiBut *but, uiWidgetColors *wcol, rcti *rect)
{
	CurveMapping *cumap;
	CurveMap *cuma;
	CurveMapPoint *cmp;
	float fx, fy, fac[2], zoomx, zoomy, offsx, offsy;
	GLint scissor[4];
	rcti scissor_new;
	int a;

	if (but->editcumap) {
		cumap = but->editcumap;
	}
	else {
		cumap = (CurveMapping *)but->poin;
	}

	cuma = &cumap->cm[cumap->cur];

	/* need scissor test, curve can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	scissor_new.xmin = ar->winrct.xmin + rect->xmin;
	scissor_new.ymin = ar->winrct.ymin + rect->ymin;
	scissor_new.xmax = ar->winrct.xmin + rect->xmax;
	scissor_new.ymax = ar->winrct.ymin + rect->ymax;
	BLI_rcti_isect(&scissor_new, &ar->winrct, &scissor_new);
	glScissor(scissor_new.xmin,
	          scissor_new.ymin,
	          BLI_rcti_size_x(&scissor_new),
	          BLI_rcti_size_y(&scissor_new));

	/* calculate offset and zoom */
	zoomx = (BLI_rcti_size_x(rect) - 2.0f * but->aspect) / BLI_rctf_size_x(&cumap->curr);
	zoomy = (BLI_rcti_size_y(rect) - 2.0f * but->aspect) / BLI_rctf_size_y(&cumap->curr);
	offsx = cumap->curr.xmin - but->aspect / zoomx;
	offsy = cumap->curr.ymin - but->aspect / zoomy;
	
	/* backdrop */
	if (but->a1 == UI_GRAD_H) {
		/* magic trigger for curve backgrounds */
		rcti grid;
		float col[3] = {0,0,0}; /* dummy arg */

		grid.xmin = rect->xmin + zoomx * (-offsx);
		grid.xmax = rect->xmax + zoomx * (-offsx);
		grid.ymin = rect->ymin + zoomy * (-offsy);
		grid.ymax = rect->ymax + zoomy * (-offsy);

		ui_draw_gradient(&grid, col, UI_GRAD_H, 1.0f);

		/* grid, hsv uses different grid */
		glEnable(GL_BLEND);
		gpuColor4P(CPACK_BLACK, 0.188f);
		ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 0.1666666f);
		glDisable(GL_BLEND);
	}
	else {
		if (cumap->flag & CUMA_DO_CLIP) {
			gl_shaded_color((unsigned char *)wcol->inner, -20);
			gpuSingleFilledRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
			gpuColor3ubv((unsigned char *)wcol->inner);
			gpuSingleFilledRectf(rect->xmin + zoomx * (cumap->clipr.xmin - offsx),
		        rect->ymin + zoomy * (cumap->clipr.ymin - offsy),
		        rect->xmin + zoomx * (cumap->clipr.xmax - offsx),
		        rect->ymin + zoomy * (cumap->clipr.ymax - offsy));
		}
		else {
			gpuColor3ubv((unsigned char *)wcol->inner);
			gpuSingleFilledRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
		}

		/* grid, every 0.25 step */
		gl_shaded_color((unsigned char *)wcol->inner, -16);
		ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 0.25f);
		/* grid, every 1.0 step */
		gl_shaded_color((unsigned char *)wcol->inner, -24);
		ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 1.0f);
		/* axes */
		gl_shaded_color((unsigned char *)wcol->inner, -50);
		gpuBegin(GL_LINES);
		gpuVertex2f(rect->xmin, rect->ymin + zoomy * (-offsy));
		gpuVertex2f(rect->xmax, rect->ymin + zoomy * (-offsy));
		gpuVertex2f(rect->xmin + zoomx * (-offsx), rect->ymin);
		gpuVertex2f(rect->xmin + zoomx * (-offsx), rect->ymax);
		gpuEnd();
	}

	/* cfra option */
	/* XXX 2.48 */
#if 0
	if (cumap->flag & CUMA_DRAW_CFRA) {
		gpuColor3ub(0x60, 0xc0, 0x40);
		gpuBegin(GL_LINES);
		gpuVertex2f(rect->xmin + zoomx * (cumap->sample[0] - offsx), rect->ymin);
		gpuVertex2f(rect->xmin + zoomx * (cumap->sample[0] - offsx), rect->ymax);
		gpuEnd();
	}
#endif
	/* sample option */

	if (cumap->flag & CUMA_DRAW_SAMPLE) {
		if (but->a1 == UI_GRAD_H) {
			float tsample[3];
			float hsv[3];
			gpuGray3f(0.941f);
			linearrgb_to_srgb_v3_v3(tsample, cumap->sample);
			rgb_to_hsv_v(tsample, hsv);

			gpuBegin(GL_LINES);
			gpuVertex2f(rect->xmin + zoomx * (hsv[0] - offsx), rect->ymin);
			gpuVertex2f(rect->xmin + zoomx * (hsv[0] - offsx), rect->ymax);
			gpuEnd();
		}
		else if (cumap->cur == 3) {
			float lum = rgb_to_bw(cumap->sample);
			
			gpuBegin(GL_LINES);
			gpuVertex2f(rect->xmin + zoomx * (lum - offsx), rect->ymin);
			gpuVertex2f(rect->xmin + zoomx * (lum - offsx), rect->ymax);
			gpuEnd();
		}
		else {
			if (cumap->cur == 0)
				gpuColor3ub(240, 100, 100);
			else if (cumap->cur == 1)
				gpuColor3ub(100, 240, 100);
			else
				gpuColor3ub(100, 100, 240);
			
			gpuBegin(GL_LINES);
			gpuVertex2f(rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymin);
			gpuVertex2f(rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymax);
			gpuEnd();
		}
	}

	/* the curve */
	gpuColor3ubv((unsigned char *)wcol->item);

	GPU_raster_begin();

	GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	glEnable(GL_BLEND);

	gpuBegin(GL_LINE_STRIP);

	if (cuma->table == NULL)
		curvemapping_changed(cumap, FALSE);
	cmp = cuma->table;

	/* first point */
	if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
		gpuVertex2f(rect->xmin, rect->ymin + zoomy * (cmp[0].y - offsy));
	}
	else {
		fx = rect->xmin + zoomx * (cmp[0].x - offsx + cuma->ext_in[0]);
		fy = rect->ymin + zoomy * (cmp[0].y - offsy + cuma->ext_in[1]);
		gpuVertex2f(fx, fy);
	}
	for (a = 0; a <= CM_TABLE; a++) {
		fx = rect->xmin + zoomx * (cmp[a].x - offsx);
		fy = rect->ymin + zoomy * (cmp[a].y - offsy);
		gpuVertex2f(fx, fy);
	}
	/* last point */
	if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
		gpuVertex2f(rect->xmax, rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy));
	}
	else {
		fx = rect->xmin + zoomx * (cmp[CM_TABLE].x - offsx - cuma->ext_out[0]);
		fy = rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy - cuma->ext_out[1]);
		gpuVertex2f(fx, fy);
	}
	gpuEnd();

	glDisable(GL_BLEND);

	GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	GPU_raster_end();

	/* the points, use aspect to make them visible on edges */
	cmp = cuma->curve;
	GPU_sprite_size(3.0f);
	GPU_sprite_begin();
	for (a = 0; a < cuma->totpoint; a++) {
		if (cmp[a].flag & CUMA_SELECT)
			UI_ThemeColor(TH_TEXT_HI);
		else
			UI_ThemeColor(TH_TEXT);
		fac[0] = rect->xmin + zoomx * (cmp[a].x - offsx);
		fac[1] = rect->ymin + zoomy * (cmp[a].y - offsy);
		GPU_sprite_2fv(fac);
	}
	GPU_sprite_end();
	GPU_sprite_size(1.0f);
	
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

	/* outline */
	gpuColor3ubv((unsigned char *)wcol->outline);
	gpuSingleWireRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
}

void ui_draw_but_TRACKPREVIEW(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *recti)
{
	rctf rect;
	int ok = 0, width, height;
	GLint scissor[4];
	MovieClipScopes *scopes = (MovieClipScopes *)but->poin;

	rect.xmin = (float)recti->xmin + 1;
	rect.xmax = (float)recti->xmax - 1;
	rect.ymin = (float)recti->ymin + SCOPE_RESIZE_PAD + 2;
	rect.ymax = (float)recti->ymax - 1;

	width  = BLI_rctf_size_x(&rect) + 1;
	height = BLI_rctf_size_y(&rect);

	glEnable(GL_BLEND);

	/* need scissor test, preview image can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	if (scopes->track_disabled) {
		gpuColor4f(0.7f, 0.3f, 0.3f, 0.3f);
		uiSetRoundBox(15);
		uiDrawBox(GL_TRIANGLE_FAN, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f);

		ok = 1;
	}
	else if ((scopes->track_search) &&
	         ((!scopes->track_preview) ||
	          (scopes->track_preview->x != width || scopes->track_preview->y != height)))
	{
		ImBuf *tmpibuf;

		if (scopes->track_preview)
			IMB_freeImBuf(scopes->track_preview);

		tmpibuf = BKE_tracking_sample_pattern(scopes->frame_width, scopes->frame_height,
		                                            scopes->track_search, scopes->track,
		                                            &scopes->undist_marker, TRUE, scopes->use_track_mask,
		                                            width, height, scopes->track_pos);

		if (tmpibuf) {
			if (tmpibuf->rect_float)
				IMB_rect_from_float(tmpibuf);

			if (tmpibuf->rect)
				scopes->track_preview = tmpibuf;
			else
				IMB_freeImBuf(tmpibuf);
		}
	}

	if (!ok && scopes->track_preview) {
		float track_pos[2];
		int a;
		ImBuf *drawibuf;

		gpuPushMatrix();

		track_pos[0] = scopes->track_pos[0];
		track_pos[1] = scopes->track_pos[1];

		/* draw content of pattern area */
		glScissor(ar->winrct.xmin + rect.xmin, ar->winrct.ymin + rect.ymin, scissor[2], scissor[3]);

		if (width > 0 && height > 0) {
			drawibuf = scopes->track_preview;

			if (scopes->use_track_mask) {
				gpuColor4P(CPACK_BLACK, 0.300f);
				uiSetRoundBox(15);
				uiDrawBox(GL_TRIANGLE_FAN, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f);
			}

			glaDrawPixelsSafe(rect.xmin, rect.ymin + 1, drawibuf->x, drawibuf->y,
			                  drawibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, drawibuf->rect);

			/* draw cross for pizel position */
			gpuTranslate(rect.xmin + track_pos[0], rect.ymin + track_pos[1], 0.f);
			glScissor(ar->winrct.xmin + rect.xmin,
			          ar->winrct.ymin + rect.ymin,
			          BLI_rctf_size_x(&rect),
			          BLI_rctf_size_y(&rect));

			GPU_raster_begin();

			for (a = 0; a < 2; a++) {
				if (a == 1) {
					gpuLineStipple(3, 0xAAAA);
					GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_STIPPLE);
					UI_ThemeColor(TH_SEL_MARKER);
				}
				else {
					UI_ThemeColor(TH_MARKER_OUTLINE);
				}

				gpuBegin(GL_LINES);
				gpuVertex2f(-10.0f, 0.0f);
				gpuVertex2f(10.0f, 0.0f);
				gpuVertex2f(0.0f, -10.0f);
				gpuVertex2f(0.0f, 10.0f);
				gpuEnd();
			}

			GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_STIPPLE);

			GPU_raster_end();
		}

		gpuPopMatrix();

		ok = 1;
	}

	if (!ok) {
		gpuColor4P(CPACK_BLACK, 0.300f);
		uiSetRoundBox(15);
		uiDrawBox(GL_TRIANGLE_FAN, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f);
	}

	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);

	glDisable(GL_BLEND);
}

void ui_draw_but_NODESOCKET(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *recti)
{
	static const float size = 5.0f;
	
	/* 16 values of sin function */
	static float si[16] = {
	    0.00000000f, 0.39435585f, 0.72479278f, 0.93775213f,
	    0.99871650f, 0.89780453f, 0.65137248f, 0.29936312f,
	    -0.10116832f, -0.48530196f, -0.79077573f, -0.96807711f,
	    -0.98846832f, -0.84864425f, -0.57126821f, -0.20129852f
	};
	/* 16 values of cos function */
	static float co[16] = {
	    1.00000000f, 0.91895781f, 0.68896691f, 0.34730525f,
	    -0.05064916f, -0.44039415f, -0.75875812f, -0.95413925f,
	    -0.99486932f, -0.87434661f, -0.61210598f, -0.25065253f,
	    0.15142777f, 0.52896401f, 0.82076344f, 0.97952994f,
	};
	
	unsigned char *col = but->col;
	int a;
	GLint scissor[4];
	rcti scissor_new;
	float x, y;
	
	x = 0.5f * (recti->xmin + recti->xmax);
	y = 0.5f * (recti->ymin + recti->ymax);
	
	/* need scissor test, can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	scissor_new.xmin = ar->winrct.xmin + recti->xmin;
	scissor_new.ymin = ar->winrct.ymin + recti->ymin;
	scissor_new.xmax = ar->winrct.xmin + recti->xmax;
	scissor_new.ymax = ar->winrct.ymin + recti->ymax;
	BLI_rcti_isect(&scissor_new, &ar->winrct, &scissor_new);
	glScissor(scissor_new.xmin,
	          scissor_new.ymin,
	          BLI_rcti_size_x(&scissor_new),
	          BLI_rcti_size_y(&scissor_new));
	
	gpuColor4ubv(col);
	
	gpuImmediateFormat_V2();

	glEnable(GL_BLEND);
	gpuBegin(GL_TRIANGLE_FAN);
	for (a = 0; a < 16; a++)
		gpuVertex2f(x + size * si[a], y + size * co[a]);
	gpuEnd();
	glDisable(GL_BLEND);
	
	gpuColor4ub(0, 0, 0, 150);
	
	glEnable(GL_BLEND);

	GPU_raster_begin();

	GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	gpuBegin(GL_LINE_LOOP);
	for (a = 0; a < 16; a++)
		gpuVertex2f(x + size * si[a], y + size * co[a]);
	gpuEnd();

	GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	GPU_raster_end();

	glDisable(GL_BLEND);
	gpuLineWidth(1.0f);
	
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

	gpuImmediateUnformat();
}

/* ****************************************************** */


static void ui_shadowbox(float minx, float miny, float maxx, float maxy, float shadsize, unsigned char alpha)
{
	glEnable(GL_BLEND);

	// SSS Enable Smooth
	GPU_aspect_enable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);

	/* right quad */
	gpuBegin(GL_TRIANGLE_FAN);

	gpuColor4P(CPACK_BLACK, alpha);
	gpuVertex2f(maxx, miny);
	gpuVertex2f(maxx, maxy - 0.3f * shadsize);

	gpuColor4P(CPACK_BLACK, 0.000f);
	gpuVertex2f(maxx + shadsize, maxy - 0.75f * shadsize);
	gpuVertex2f(maxx + shadsize, miny);

	gpuEnd();
	
	/* corner shape */
	gpuBegin(GL_TRIANGLE_FAN);

	gpuColor4P(CPACK_BLACK, alpha);
	gpuVertex2f(maxx, miny);

	gpuColor4P(CPACK_BLACK, 0.000f);
	gpuVertex2f(maxx + shadsize, miny);
	gpuVertex2f(maxx + 0.7f * shadsize, miny - 0.7f * shadsize);
	gpuVertex2f(maxx, miny - shadsize);

	gpuEnd();
	
	/* bottom quad */
	gpuBegin(GL_TRIANGLE_FAN);

	gpuColor4P(CPACK_BLACK, alpha);
	gpuVertex2f(minx + 0.3f * shadsize, miny);
	gpuVertex2f(maxx, miny);

	gpuColor4P(CPACK_BLACK, 0.000f);
	gpuVertex2f(maxx, miny - shadsize);
	gpuVertex2f(minx + 0.5f * shadsize, miny - shadsize);

	gpuEnd();
	
	glDisable(GL_BLEND);

	// SSS Disable Smooth
	GPU_aspect_disable(GPU_ASPECT_BASIC, GPU_BASIC_SMOOTH);
}

void uiDrawBoxShadow(unsigned char alpha, float minx, float miny, float maxx, float maxy)
{
	/* accumulated outline boxes to make shade not linear, is more pleasant */
	ui_shadowbox(minx, miny, maxx, maxy, 11.0, (20 * alpha) >> 8);
	ui_shadowbox(minx, miny, maxx, maxy, 7.0, (40 * alpha) >> 8);
	ui_shadowbox(minx, miny, maxx, maxy, 5.0, (80 * alpha) >> 8);
	
}


void ui_dropshadow(const rctf *rct, float radius, float aspect, float alpha, int UNUSED(select))
{
	int i;
	float rad;
	float a;
	float dalpha = alpha * 2.0f / 255.0f, calpha;
	
	glEnable(GL_BLEND);
	
	if (radius > (BLI_rctf_size_y(rct) - 10.0f) / 2.0f)
		rad = (BLI_rctf_size_y(rct) - 10.0f) / 2.0f;
	else
		rad = radius;

	i = 12;
#if 0
	if (select) {
		a = i * aspect; /* same as below */
	}
	else
#endif
	{
		a = i * aspect;
	}

	calpha = dalpha;
	for (; i--; a -= aspect) {
		/* alpha ranges from 2 to 20 or so */
		gpuColor4P(CPACK_BLACK, calpha);
		calpha += dalpha;

		uiDrawBox(GL_TRIANGLE_FAN, rct->xmin - a, rct->ymin - a, rct->xmax + a, rct->ymax - 10.0f + a, rad + a);
	}

	/* outline emphasis */
	GPU_raster_begin();

	GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	gpuColor4P(CPACK_BLACK, 0.392f);
	uiDrawBox(GL_LINE_LOOP, rct->xmin - 0.5f, rct->ymin - 0.5f, rct->xmax + 0.5f, rct->ymax + 0.5f, radius + 0.5f);

	GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_AA);

	GPU_raster_end();

	glDisable(GL_BLEND);
}

