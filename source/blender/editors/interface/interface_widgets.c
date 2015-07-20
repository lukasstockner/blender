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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_widgets.c
 *  \ingroup edinterface
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "DNA_brush_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_curve.h"

#include "RNA_access.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "widgets.h"

#include "interface_intern.h"

#ifdef WITH_INPUT_IME
#  include "WM_types.h"
#endif

/* icons are 80% of height of button (16 pixels inside 20 height) */
#define ICON_SIZE_FROM_BUTRECT(rect) (0.8f * BLI_rcti_size_y(rect))


/* *********************** draw data ************************** */

#define WIDGET_AA_JITTER 8
static const float jit[WIDGET_AA_JITTER][2] = {
	{ 0.468813, -0.481430}, {-0.155755, -0.352820},
	{ 0.219306, -0.238501}, {-0.393286, -0.110949},
	{-0.024699,  0.013908}, { 0.343805,  0.147431},
	{-0.272855,  0.269918}, { 0.095909,  0.388710}
};

/* ************************************************* */

void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3)
{
	float tri_arr[3][2] = {{x1, y1}, {x2, y2}, {x3, y3}};
	float color[4];
	int j;
	
	glEnable(GL_BLEND);
	glGetFloatv(GL_CURRENT_COLOR, color);
	color[3] *= 0.125f;
	glColor4fv(color);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, tri_arr);

	/* for each AA step */
	for (j = 0; j < WIDGET_AA_JITTER; j++) {
		glTranslatef(jit[j][0], jit[j][1], 0.0f);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glTranslatef(-jit[j][0], -jit[j][1], 0.0f);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_BLEND);
}

void ui_draw_anti_roundbox(int mode, float minx, float miny, float maxx, float maxy, float rad, bool use_alpha)
{
	float color[4];
	int j;
	
	glEnable(GL_BLEND);
	glGetFloatv(GL_CURRENT_COLOR, color);
	if (use_alpha) {
		color[3] = 0.5f;
	}
	color[3] *= 0.125f;
	glColor4fv(color);
	
	for (j = 0; j < WIDGET_AA_JITTER; j++) {
		glTranslatef(jit[j][0], jit[j][1], 0.0f);
		UI_draw_roundbox_gl_mode(mode, minx, miny, maxx, maxy, rad);
		glTranslatef(-jit[j][0], -jit[j][1], 0.0f);
	}

	glDisable(GL_BLEND);
}



/* prepares shade colors */
static void shadecolors4(char coltop[4], char coldown[4], const char *color, short shadetop, short shadedown)
{
	coltop[0] = CLAMPIS(color[0] + shadetop, 0, 255);
	coltop[1] = CLAMPIS(color[1] + shadetop, 0, 255);
	coltop[2] = CLAMPIS(color[2] + shadetop, 0, 255);
	coltop[3] = color[3];

	coldown[0] = CLAMPIS(color[0] + shadedown, 0, 255);
	coldown[1] = CLAMPIS(color[1] + shadedown, 0, 255);
	coldown[2] = CLAMPIS(color[2] + shadedown, 0, 255);
	coldown[3] = color[3];
}

static void round_box_shade_col4_r(unsigned char r_col[4], const char col1[4], const char col2[4], const float fac)
{
	const int faci = FTOCHAR(fac);
	const int facm = 255 - faci;

	r_col[0] = (faci * col1[0] + facm * col2[0]) / 256;
	r_col[1] = (faci * col1[1] + facm * col2[1]) / 256;
	r_col[2] = (faci * col1[2] + facm * col2[2]) / 256;
	r_col[3] = (faci * col1[3] + facm * col2[3]) / 256;
}

static void widget_verts_to_triangle_strip(uiWidgetDrawBase *wtb, const int totvert, float triangle_strip[WIDGET_SIZE_MAX * 2 + 2][2])
{
	int a;
	for (a = 0; a < totvert; a++) {
		copy_v2_v2(triangle_strip[a * 2], wtb->outer_v[a]);
		copy_v2_v2(triangle_strip[a * 2 + 1], wtb->inner_v[a]);
	}
	copy_v2_v2(triangle_strip[a * 2], wtb->outer_v[0]);
	copy_v2_v2(triangle_strip[a * 2 + 1], wtb->inner_v[0]);
}

static void widget_drawbase_outline(uiWidgetDrawBase *wtb)
{
	float triangle_strip[WIDGET_SIZE_MAX * 2 + 2][2]; /* + 2 because the last pair is wrapped */
	widget_verts_to_triangle_strip(wtb, wtb->totvert, triangle_strip);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, triangle_strip);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, wtb->totvert * 2 + 2);
	glDisableClientState(GL_VERTEX_ARRAY);
}

/* *********************** widget types ************************************* */

static struct uiWidgetStateColors wcol_state_colors = {
	{115, 190, 76, 255},
	{90, 166, 51, 255},
	{240, 235, 100, 255},
	{215, 211, 75, 255},
	{180, 0, 255, 255},
	{153, 0, 230, 255},
	0.5f, 0.0f
};

static struct uiWidgetColors wcol_num = {
	{25, 25, 25, 255},
	{180, 180, 180, 255},
	{153, 153, 153, 255},
	{90, 90, 90, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	-20, 0
};

static struct uiWidgetColors wcol_numslider = {
	{25, 25, 25, 255},
	{180, 180, 180, 255},
	{153, 153, 153, 255},
	{128, 128, 128, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	-20, 0
};

static struct uiWidgetColors wcol_text = {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{153, 153, 153, 255},
	{90, 90, 90, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	0, 25
};

static struct uiWidgetColors wcol_option = {
	{0, 0, 0, 255},
	{70, 70, 70, 255},
	{70, 70, 70, 255},
	{255, 255, 255, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	15, -15
};

/* button that shows popup */
static struct uiWidgetColors wcol_menu = {
	{0, 0, 0, 255},
	{70, 70, 70, 255},
	{70, 70, 70, 255},
	{255, 255, 255, 255},
	
	{255, 255, 255, 255},
	{204, 204, 204, 255},
	
	1,
	15, -15
};

/* button that starts pulldown */
static struct uiWidgetColors wcol_pulldown = {
	{0, 0, 0, 255},
	{63, 63, 63, 255},
	{86, 128, 194, 255},
	{255, 255, 255, 255},
	
	{0, 0, 0, 255},
	{0, 0, 0, 255},
	
	0,
	25, -20
};

/* button inside menu */
static struct uiWidgetColors wcol_menu_item = {
	{0, 0, 0, 255},
	{0, 0, 0, 0},
	{86, 128, 194, 255},
	{172, 172, 172, 128},
	
	{255, 255, 255, 255},
	{0, 0, 0, 255},
	
	1,
	38, 0
};

/* backdrop menu + title text color */
static struct uiWidgetColors wcol_menu_back = {
	{0, 0, 0, 255},
	{25, 25, 25, 230},
	{45, 45, 45, 230},
	{100, 100, 100, 255},
	
	{160, 160, 160, 255},
	{255, 255, 255, 255},
	
	0,
	25, -20
};

/* pie menus */
static struct uiWidgetColors wcol_pie_menu = {
	{10, 10, 10, 200},
	{25, 25, 25, 230},
	{140, 140, 140, 255},
	{45, 45, 45, 230},

	{160, 160, 160, 255},
	{255, 255, 255, 255},

	1,
	10, -10
};


/* tooltip color */
static struct uiWidgetColors wcol_tooltip = {
	{0, 0, 0, 255},
	{25, 25, 25, 230},
	{45, 45, 45, 230},
	{100, 100, 100, 255},

	{255, 255, 255, 255},
	{255, 255, 255, 255},

	0,
	25, -20
};

static struct uiWidgetColors wcol_radio = {
	{0, 0, 0, 255},
	{70, 70, 70, 255},
	{86, 128, 194, 255},
	{255, 255, 255, 255},
	
	{255, 255, 255, 255},
	{0, 0, 0, 255},
	
	1,
	15, -15
};

static struct uiWidgetColors wcol_regular = {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};

static struct uiWidgetColors wcol_tool = {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	15, -15
};

static struct uiWidgetColors wcol_box = {
	{25, 25, 25, 255},
	{128, 128, 128, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};

static struct uiWidgetColors wcol_toggle = {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};

static struct uiWidgetColors wcol_scroll = {
	{50, 50, 50, 180},
	{80, 80, 80, 180},
	{100, 100, 100, 180},
	{128, 128, 128, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	5, -5
};

static struct uiWidgetColors wcol_progress = {
	{0, 0, 0, 255},
	{190, 190, 190, 255},
	{100, 100, 100, 180},
	{68, 68, 68, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};

static struct uiWidgetColors wcol_list_item = {
	{0, 0, 0, 255},
	{0, 0, 0, 0},
	{86, 128, 194, 255},
	{0, 0, 0, 255},
	
	{0, 0, 0, 255},
	{0, 0, 0, 255},
	
	0,
	0, 0
};

/* free wcol struct to play with */
static struct uiWidgetColors wcol_tmp = {
	{0, 0, 0, 255},
	{128, 128, 128, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};


/* called for theme init (new theme) and versions */
void ui_widget_color_init(ThemeUI *tui)
{
	tui->wcol_regular = wcol_regular;
	tui->wcol_tool = wcol_tool;
	tui->wcol_text = wcol_text;
	tui->wcol_radio = wcol_radio;
	tui->wcol_option = wcol_option;
	tui->wcol_toggle = wcol_toggle;
	tui->wcol_num = wcol_num;
	tui->wcol_numslider = wcol_numslider;
	tui->wcol_menu = wcol_menu;
	tui->wcol_pulldown = wcol_pulldown;
	tui->wcol_menu_back = wcol_menu_back;
	tui->wcol_pie_menu = wcol_pie_menu;
	tui->wcol_tooltip = wcol_tooltip;
	tui->wcol_menu_item = wcol_menu_item;
	tui->wcol_box = wcol_box;
	tui->wcol_scroll = wcol_scroll;
	tui->wcol_list_item = wcol_list_item;
	tui->wcol_progress = wcol_progress;

	tui->wcol_state = wcol_state_colors;

	/* just to get rid of unused warning */
	(void)wcol_tmp;
}

/* ************ button callbacks, state ***************** */

static void widget_state_blend(char cp[3], const char cpstate[3], const float fac)
{
	if (fac != 0.0f) {
		cp[0] = (int)((1.0f - fac) * cp[0] + fac * cpstate[0]);
		cp[1] = (int)((1.0f - fac) * cp[1] + fac * cpstate[1]);
		cp[2] = (int)((1.0f - fac) * cp[2] + fac * cpstate[2]);
	}
}

/* put all widget colors on half alpha, use local storage */
static void ui_widget_color_disabled(uiWidgetType *wt)
{
	static uiWidgetColors wcol_theme_s;

	wcol_theme_s = *wt->wcol_theme;

	wcol_theme_s.outline[3] *= 0.5;
	wcol_theme_s.inner[3] *= 0.5;
	wcol_theme_s.inner_sel[3] *= 0.5;
	wcol_theme_s.item[3] *= 0.5;
	wcol_theme_s.text[3] *= 0.5;
	wcol_theme_s.text_sel[3] *= 0.5;

	wt->wcol_theme = &wcol_theme_s;
}

/* copy colors from theme, and set changes in it based on state */
static void widget_state(uiWidgetType *wt, int state)
{
	uiWidgetStateColors *wcol_state = wt->wcol_state;

	if ((state & UI_BUT_LIST_ITEM) && !(state & UI_TEXTINPUT)) {
		/* Override default widget's colors. */
		bTheme *btheme = UI_GetTheme();
		wt->wcol_theme = &btheme->tui.wcol_list_item;

		if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE)) {
			ui_widget_color_disabled(wt);
		}
	}

	wt->wcol = *(wt->wcol_theme);

	if (state & UI_SELECT) {
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);

		if (state & UI_BUT_ANIMATED_KEY)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_key_sel, wcol_state->blend);
		else if (state & UI_BUT_ANIMATED)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_anim_sel, wcol_state->blend);
		else if (state & UI_BUT_DRIVEN)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_driven_sel, wcol_state->blend);

		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
		
		if (state & UI_SELECT)
			SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
	}
	else {
		if (state & UI_BUT_ANIMATED_KEY)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_key, wcol_state->blend);
		else if (state & UI_BUT_ANIMATED)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_anim, wcol_state->blend);
		else if (state & UI_BUT_DRIVEN)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_driven, wcol_state->blend);

		if (state & UI_ACTIVE) { /* mouse over? */
			wt->wcol.inner[0] = wt->wcol.inner[0] >= 240 ? 255 : wt->wcol.inner[0] + 15;
			wt->wcol.inner[1] = wt->wcol.inner[1] >= 240 ? 255 : wt->wcol.inner[1] + 15;
			wt->wcol.inner[2] = wt->wcol.inner[2] >= 240 ? 255 : wt->wcol.inner[2] + 15;
		}
	}

	if (state & UI_BUT_REDALERT) {
		char red[4] = {255, 0, 0};
		widget_state_blend(wt->wcol.inner, red, 0.4f);
	}

	if (state & UI_BUT_DRAG_MULTI) {
		/* the button isn't SELECT but we're editing this so draw with sel color */
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);
		SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
		widget_state_blend(wt->wcol.text, wt->wcol.text_sel, 0.85f);
	}

	if (state & UI_BUT_NODE_ACTIVE) {
		char blue[4] = {86, 128, 194};
		widget_state_blend(wt->wcol.inner, blue, 0.3f);
	}
}

/* labels use theme colors for text */
static void widget_state_option_menu(uiWidgetType *wt, int state)
{
	bTheme *btheme = UI_GetTheme(); /* XXX */
	
	/* call this for option button */
	widget_state(wt, state);
	
	/* if not selected we get theme from menu back */
	if (state & UI_SELECT)
		copy_v3_v3_char(wt->wcol.text, btheme->tui.wcol_menu_back.text_sel);
	else
		copy_v3_v3_char(wt->wcol.text, btheme->tui.wcol_menu_back.text);
}


/* ************ menu backdrop ************************* */


void ui_hsvcircle_vals_from_pos(float *val_rad, float *val_dist, const rcti *rect,
                                const float mx, const float my)
{
	/* duplication of code... well, simple is better now */
	const float centx = BLI_rcti_cent_x_fl(rect);
	const float centy = BLI_rcti_cent_y_fl(rect);
	const float radius = (float)min_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)) / 2.0f;
	const float m_delta[2] = {mx - centx, my - centy};
	const float dist_sq = len_squared_v2(m_delta);

	*val_dist = (dist_sq < (radius * radius)) ? sqrtf(dist_sq) / radius : 1.0f;
	*val_rad = atan2f(m_delta[0], m_delta[1]) / (2.0f * (float)M_PI) + 0.5f;
}

/* cursor in hsv circle, in float units -1 to 1, to map on radius */
void ui_hsvcircle_pos_from_vals(uiBut *but, const rcti *rect, float *hsv, float *xpos, float *ypos)
{
	/* duplication of code... well, simple is better now */
	const float centx = BLI_rcti_cent_x_fl(rect);
	const float centy = BLI_rcti_cent_y_fl(rect);
	float radius = (float)min_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)) / 2.0f;
	float ang, radius_t;
	
	ang = 2.0f * (float)M_PI * hsv[0] + (float)M_PI_2;
	
	if ((but->flag & UI_BUT_COLOR_CUBIC) && (U.color_picker_type == USER_CP_CIRCLE_HSV))
		radius_t = (1.0f - pow3f(1.0f - hsv[1]));
	else
		radius_t = hsv[1];
	
	radius = CLAMPIS(radius_t, 0.0f, 1.0f) * radius;
	*xpos = centx + cosf(-ang) * radius;
	*ypos = centy + sinf(-ang) * radius;
}

/* ************ custom buttons, old stuff ************** */

/* draws in resolution of 48x4 colors */
void ui_draw_gradient(const rcti *rect, const float hsv[3], const int type, const float alpha)
{
	/* allows for 4 steps (red->yellow) */
	const float color_step = 1.0f / 48.0f;
	int a;
	float h = hsv[0], s = hsv[1], v = hsv[2];
	float dx, dy, sx1, sx2, sy;
	float col0[4][3];   /* left half, rect bottom to top */
	float col1[4][3];   /* right half, rect bottom to top */

	/* draw series of gouraud rects */
	glShadeModel(GL_SMOOTH);
	
	switch (type) {
		case UI_GRAD_SV:
			hsv_to_rgb(h, 0.0, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(h, 0.0, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(h, 0.0, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(h, 0.0, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
			break;
		case UI_GRAD_HV:
			hsv_to_rgb(0.0, s, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(0.0, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(0.0, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(0.0, s, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
			break;
		case UI_GRAD_HS:
			hsv_to_rgb(0.0, 0.0, v,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(0.0, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(0.0, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(0.0, 1.0, v,   &col1[3][0], &col1[3][1], &col1[3][2]);
			break;
		case UI_GRAD_H:
			hsv_to_rgb(0.0, 1.0, 1.0, &col1[0][0], &col1[0][1], &col1[0][2]);
			copy_v3_v3(col1[1], col1[0]);
			copy_v3_v3(col1[2], col1[0]);
			copy_v3_v3(col1[3], col1[0]);
			break;
		case UI_GRAD_S:
			hsv_to_rgb(1.0, 0.0, 1.0, &col1[1][0], &col1[1][1], &col1[1][2]);
			copy_v3_v3(col1[0], col1[1]);
			copy_v3_v3(col1[2], col1[1]);
			copy_v3_v3(col1[3], col1[1]);
			break;
		case UI_GRAD_V:
			hsv_to_rgb(1.0, 1.0, 0.0, &col1[2][0], &col1[2][1], &col1[2][2]);
			copy_v3_v3(col1[0], col1[2]);
			copy_v3_v3(col1[1], col1[2]);
			copy_v3_v3(col1[3], col1[2]);
			break;
		default:
			assert(!"invalid 'type' argument");
			hsv_to_rgb(1.0, 1.0, 1.0, &col1[2][0], &col1[2][1], &col1[2][2]);
			copy_v3_v3(col1[0], col1[2]);
			copy_v3_v3(col1[1], col1[2]);
			copy_v3_v3(col1[3], col1[2]);
			break;
	}
	
	/* old below */
	
	for (dx = 0.0f; dx < 0.999f; dx += color_step) { /* 0.999 = prevent float inaccuracy for steps */
		const float dx_next = dx + color_step;

		/* previous color */
		copy_v3_v3(col0[0], col1[0]);
		copy_v3_v3(col0[1], col1[1]);
		copy_v3_v3(col0[2], col1[2]);
		copy_v3_v3(col0[3], col1[3]);
		
		/* new color */
		switch (type) {
			case UI_GRAD_SV:
				hsv_to_rgb(h, dx, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
				hsv_to_rgb(h, dx, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
				hsv_to_rgb(h, dx, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
				hsv_to_rgb(h, dx, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
				break;
			case UI_GRAD_HV:
				hsv_to_rgb(dx_next, s, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
				hsv_to_rgb(dx_next, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
				hsv_to_rgb(dx_next, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
				hsv_to_rgb(dx_next, s, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
				break;
			case UI_GRAD_HS:
				hsv_to_rgb(dx_next, 0.0, v,   &col1[0][0], &col1[0][1], &col1[0][2]);
				hsv_to_rgb(dx_next, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
				hsv_to_rgb(dx_next, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
				hsv_to_rgb(dx_next, 1.0, v,   &col1[3][0], &col1[3][1], &col1[3][2]);
				break;
			case UI_GRAD_H:
				/* annoying but without this the color shifts - could be solved some other way
				 * - campbell */
				hsv_to_rgb(dx_next, 1.0, 1.0, &col1[0][0], &col1[0][1], &col1[0][2]);
				copy_v3_v3(col1[1], col1[0]);
				copy_v3_v3(col1[2], col1[0]);
				copy_v3_v3(col1[3], col1[0]);
				break;
			case UI_GRAD_S:
				hsv_to_rgb(h, dx, 1.0, &col1[1][0], &col1[1][1], &col1[1][2]);
				copy_v3_v3(col1[0], col1[1]);
				copy_v3_v3(col1[2], col1[1]);
				copy_v3_v3(col1[3], col1[1]);
				break;
			case UI_GRAD_V:
				hsv_to_rgb(h, 1.0, dx, &col1[2][0], &col1[2][1], &col1[2][2]);
				copy_v3_v3(col1[0], col1[2]);
				copy_v3_v3(col1[1], col1[2]);
				copy_v3_v3(col1[3], col1[2]);
				break;
		}
		
		/* rect */
		sx1 = rect->xmin + dx      * BLI_rcti_size_x(rect);
		sx2 = rect->xmin + dx_next * BLI_rcti_size_x(rect);
		sy = rect->ymin;
		dy = (float)BLI_rcti_size_y(rect) / 3.0f;
		
		glBegin(GL_QUADS);
		for (a = 0; a < 3; a++, sy += dy) {
			glColor4f(col0[a][0], col0[a][1], col0[a][2], alpha);
			glVertex2f(sx1, sy);
			
			glColor4f(col1[a][0], col1[a][1], col1[a][2], alpha);
			glVertex2f(sx2, sy);

			glColor4f(col1[a + 1][0], col1[a + 1][1], col1[a + 1][2], alpha);
			glVertex2f(sx2, sy + dy);
			
			glColor4f(col0[a + 1][0], col0[a + 1][1], col0[a + 1][2], alpha);
			glVertex2f(sx1, sy + dy);
		}
		glEnd();
	}

	glShadeModel(GL_FLAT);
}

bool ui_but_is_colorpicker_display_space(uiBut *but)
{
	bool color_profile = but->block->color_profile;

	if (but->rnaprop) {
		if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
			color_profile = false;
	}

	return color_profile;
}

void ui_hsvcube_pos_from_vals(uiBut *but, const rcti *rect, float *hsv, float *xp, float *yp)
{
	float x = 0.0f, y = 0.0f;

	switch ((int)but->a1) {
		case UI_GRAD_SV:
			x = hsv[1]; y = hsv[2]; break;
		case UI_GRAD_HV:
			x = hsv[0]; y = hsv[2]; break;
		case UI_GRAD_HS:
			x = hsv[0]; y = hsv[1]; break;
		case UI_GRAD_H:
			x = hsv[0]; y = 0.5; break;
		case UI_GRAD_S:
			x = hsv[1]; y = 0.5; break;
		case UI_GRAD_V:
			x = hsv[2]; y = 0.5; break;
		case UI_GRAD_L_ALT:
			x = 0.5f;
			/* exception only for value strip - use the range set in but->min/max */
			y = hsv[2];
			break;
		case UI_GRAD_V_ALT:
			x = 0.5f;
			/* exception only for value strip - use the range set in but->min/max */
			y = (hsv[2] - but->softmin) / (but->softmax - but->softmin);
			break;
	}

	/* cursor */
	*xp = rect->xmin + x * BLI_rcti_size_x(rect);
	*yp = rect->ymin + y * BLI_rcti_size_y(rect);
}


/* ************ separator, for menus etc ***************** */
static void ui_draw_separator(const rcti *rect,  uiWidgetColors *wcol)
{
	int y = rect->ymin + BLI_rcti_size_y(rect) / 2 - 1;
	unsigned char col[4];
	
	col[0] = wcol->text[0];
	col[1] = wcol->text[1];
	col[2] = wcol->text[2];
	col[3] = 30;
	
	glEnable(GL_BLEND);
	glColor4ubv(col);
	sdrawline(rect->xmin, y, rect->xmax, y);
	glDisable(GL_BLEND);
}

/* ************ button callbacks, draw ***************** */

bool ui_link_bezier_points(const rcti *rect, float coord_array[][2], int resol)
{
	float dist, vec[4][2];

	vec[0][0] = rect->xmin;
	vec[0][1] = rect->ymin;
	vec[3][0] = rect->xmax;
	vec[3][1] = rect->ymax;
	
	dist = 0.5f * fabsf(vec[0][0] - vec[3][0]);
	
	vec[1][0] = vec[0][0] + dist;
	vec[1][1] = vec[0][1];
	
	vec[2][0] = vec[3][0] - dist;
	vec[2][1] = vec[3][1];
	
	BKE_curve_forward_diff_bezier(vec[0][0], vec[1][0], vec[2][0], vec[3][0], &coord_array[0][0], resol, sizeof(float[2]));
	BKE_curve_forward_diff_bezier(vec[0][1], vec[1][1], vec[2][1], vec[3][1], &coord_array[0][1], resol, sizeof(float[2]));

	/* TODO: why return anything if always true? */
	return true;
}

#define LINK_RESOL  24
void ui_draw_link_bezier(const rcti *rect)
{
	float coord_array[LINK_RESOL + 1][2];

	if (ui_link_bezier_points(rect, coord_array, LINK_RESOL)) {
#if 0 /* unused */
		/* we can reuse the dist variable here to increment the GL curve eval amount*/
		const float dist = 1.0f / (float)LINK_RESOL;
#endif
		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, coord_array);
		glDrawArrays(GL_LINE_STRIP, 0, LINK_RESOL + 1);
		glDisableClientState(GL_VERTEX_ARRAY);

		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);
	}
}

/* function in use for buttons and for view2d sliders */
void UI_draw_widget_scroll(uiWidgetColors *wcol, const rcti *rect, const rcti *slider, int state)
{
	uiWidgetType *wt_back = WidgetTypeInit(UI_WTYPE_SCROLL_BACK);
	wt_back->wcol_theme = wcol;

	WidgetDraw(wt_back, NULL, NULL, (rcti *)rect, state, UI_CNR_ALL, 0, NULL, false);

	/* slider */
	if ((BLI_rcti_size_x(slider) > 2) && (BLI_rcti_size_y(slider) > 2)) {
		uiWidgetType *wt_inner = WidgetTypeInit(UI_WTYPE_SCROLL_INNER);
		wt_inner->wcol_theme = wcol;

		WidgetDraw(wt_inner, NULL, NULL, (rcti *)slider, state, UI_CNR_ALL, 0, NULL, false);
	}
}

static void widget_draw_extra_mask(const bContext *C, uiBut *but, uiWidgetType *wt, rcti *rect)
{
	uiWidgetDrawBase wtb;
	const float rad = 0.25f * U.widget_unit;
	unsigned char col[4];
	
	/* state copy! */
	wt->wcol = *(wt->wcol_theme);
	
	widget_drawbase_init(&wtb);
	
	if (but->block->drawextra) {
		/* note: drawextra can change rect +1 or -1, to match round errors of existing previews */
		but->block->drawextra(C, but->poin, but->block->drawextra_arg1, but->block->drawextra_arg2, rect);
		
		/* make mask to draw over image */
		UI_GetThemeColor3ubv(TH_BACK, col);
		glColor3ubv(col);
		
		round_box__edges(&wtb, UI_CNR_ALL, rect, 0.0f, rad);
		widget_drawbase_outline(&wtb);
	}
	
	/* outline */
	widget_drawbase_roundboxedges_set(&wtb, UI_CNR_ALL, rect, rad);
	wtb.draw_outline = true;
	wtb.draw_inner = false;
	widget_drawbase_draw(&wtb, &wt->wcol);
}


static int widget_roundbox_set(uiBut *but, rcti *rect)
{
	int roundbox = UI_CNR_ALL;

	/* alignment */
	if ((but->drawflag & UI_BUT_ALIGN) && but->type != UI_BTYPE_PULLDOWN) {
		
		/* ui_block_position has this correction too, keep in sync */
		if (but->drawflag & UI_BUT_ALIGN_TOP)
			rect->ymax += U.pixelsize;
		if (but->drawflag & UI_BUT_ALIGN_LEFT)
			rect->xmin -= U.pixelsize;
		
		switch (but->drawflag & UI_BUT_ALIGN) {
			case UI_BUT_ALIGN_TOP:
				roundbox = UI_CNR_BOTTOM_LEFT | UI_CNR_BOTTOM_RIGHT;
				break;
			case UI_BUT_ALIGN_DOWN:
				roundbox = UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT;
				break;
			case UI_BUT_ALIGN_LEFT:
				roundbox = UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT;
				break;
			case UI_BUT_ALIGN_RIGHT:
				roundbox = UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT;
				break;
			case UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_RIGHT:
				roundbox = UI_CNR_TOP_LEFT;
				break;
			case UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_LEFT:
				roundbox = UI_CNR_TOP_RIGHT;
				break;
			case UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_RIGHT:
				roundbox = UI_CNR_BOTTOM_LEFT;
				break;
			case UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_LEFT:
				roundbox = UI_CNR_BOTTOM_RIGHT;
				break;
			default:
				roundbox = 0;
				break;
		}
	}

	/* align with open menu */
	if (but->active) {
		int direction = ui_but_menu_direction(but);

		if      (direction == UI_DIR_UP)    roundbox &= ~(UI_CNR_TOP_RIGHT    | UI_CNR_TOP_LEFT);
		else if (direction == UI_DIR_DOWN)  roundbox &= ~(UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
		else if (direction == UI_DIR_LEFT)  roundbox &= ~(UI_CNR_TOP_LEFT     | UI_CNR_BOTTOM_LEFT);
		else if (direction == UI_DIR_RIGHT) roundbox &= ~(UI_CNR_TOP_RIGHT    | UI_CNR_BOTTOM_RIGHT);
	}

	return roundbox;
}

/* conversion from old to new buttons, so still messy */
void ui_draw_but(const bContext *C, ARegion *ar, uiStyle *style, uiBut *but, rcti *rect)
{
	bTheme *btheme = UI_GetTheme();
	ThemeUI *tui = &btheme->tui;
	uiFontStyle *fstyle = &style->widget;
	uiWidgetType *wt = NULL;

	/* handle menus separately */
	if (but->dt == UI_EMBOSS_PULLDOWN) {
		switch (but->type) {
			case UI_BTYPE_LABEL:
				wt = WidgetTypeInit(UI_WTYPE_MENU_LABEL);
				break;
			case UI_BTYPE_SEPR_LINE:
				ui_draw_separator(rect, &tui->wcol_menu_item);
				break;
			default:
				wt = WidgetTypeInit(UI_WTYPE_MENU_ITEM);
				break;
		}
	}
	else if (but->dt == UI_EMBOSS_NONE) {
		/* "nothing" */
		wt = WidgetTypeInit(UI_WTYPE_ICON);
	}
	else if (but->dt == UI_EMBOSS_RADIAL) {
		wt = WidgetTypeInit(UI_WTYPE_MENU_ITEM_RADIAL);
	}
	else {
		BLI_assert(but->dt == UI_EMBOSS);

		switch (but->type) {
			case UI_BTYPE_LABEL:
				if (but->block->flag & UI_BLOCK_LOOP) {
					wt = WidgetTypeInit(UI_WTYPE_MENU_LABEL);
				}
				else {
					wt = WidgetTypeInit(UI_WTYPE_LABEL);
				}
				fstyle = &style->widgetlabel;
				break;

			case UI_BTYPE_SEPR:
			case UI_BTYPE_SEPR_LINE:
				break;
				
			case UI_BTYPE_BUT:
				wt = WidgetTypeInit(UI_WTYPE_EXEC);
				break;

			case UI_BTYPE_NUM:
				wt = WidgetTypeInit(UI_WTYPE_NUMBER);
				break;
				
			case UI_BTYPE_NUM_SLIDER:
				wt = WidgetTypeInit(UI_WTYPE_SLIDER);
				break;
				
			case UI_BTYPE_ROW:
				wt = WidgetTypeInit(UI_WTYPE_RADIO);
				break;

			case UI_BTYPE_LISTROW:
				wt = WidgetTypeInit(UI_WTYPE_LISTITEM);
				break;
				
			case UI_BTYPE_TEXT:
				wt = WidgetTypeInit(UI_WTYPE_NAME);
				break;

			case UI_BTYPE_SEARCH_MENU:
				wt = WidgetTypeInit(UI_WTYPE_NAME);
				if (but->block->flag & UI_BLOCK_LOOP)
					wt->wcol_theme = &btheme->tui.wcol_menu_back;
				break;
				
			case UI_BTYPE_BUT_TOGGLE:
			case UI_BTYPE_TOGGLE:
			case UI_BTYPE_TOGGLE_N:
				wt = WidgetTypeInit(UI_WTYPE_TOGGLE);
				break;
				
			case UI_BTYPE_CHECKBOX:
			case UI_BTYPE_CHECKBOX_N:
				if (!(but->flag & UI_HAS_ICON)) {
					wt = WidgetTypeInit(UI_WTYPE_CHECKBOX);
					but->drawflag |= UI_BUT_TEXT_LEFT;
				}
				else
					wt = WidgetTypeInit(UI_WTYPE_TOGGLE);
				
				/* XXX this should really not be here! */
				/* option buttons have strings outside, on menus use different colors */
				if (but->block->flag & UI_BLOCK_LOOP)
					wt->draw_type->state = widget_state_option_menu;
				
				break;
				
			case UI_BTYPE_MENU:
			case UI_BTYPE_BLOCK:
				if (but->flag & UI_BUT_NODE_LINK) {
					/* new node-link button, not active yet XXX */
					wt = WidgetTypeInit(UI_WTYPE_MENU_NODE_LINK);
				}
				else {
					/* with menu arrows */

					/* we could use a flag for this, but for now just check size,
					 * add updown arrows if there is room. */
					if ((!but->str[0] && but->icon && (BLI_rcti_size_x(rect) < BLI_rcti_size_y(rect) + 2)) ||
					    /* disable for brushes also */
					    (but->flag & UI_BUT_ICON_PREVIEW))
					{
						/* no arrows */
						wt = WidgetTypeInit(UI_WTYPE_MENU_ICON_RADIO);
					}
					else {
						wt = WidgetTypeInit(UI_WTYPE_MENU_RADIO);
					}
				}
				break;
				
			case UI_BTYPE_PULLDOWN:
				wt = WidgetTypeInit(UI_WTYPE_PULLDOWN);
				break;
			
			case UI_BTYPE_BUT_MENU:
				wt = WidgetTypeInit(UI_WTYPE_MENU_ITEM);
				break;
				
			case UI_BTYPE_COLOR:
				wt = WidgetTypeInit(UI_WTYPE_SWATCH);
				break;
				
			case UI_BTYPE_ROUNDBOX:
			case UI_BTYPE_LISTBOX:
				wt = WidgetTypeInit(UI_WTYPE_BOX);
				break;
				
			case UI_BTYPE_LINK:
			case UI_BTYPE_INLINK:
				wt = WidgetTypeInit(UI_WTYPE_LINK);
				break;
			
			case UI_BTYPE_EXTRA:
				widget_draw_extra_mask(C, but, WidgetTypeInit(UI_WTYPE_BOX), rect);
				break;
				
			case UI_BTYPE_HSVCUBE:
				/* vertical V slider */
				if (ELEM(but->a1, UI_GRAD_V_ALT, UI_GRAD_L_ALT)) {
					wt = WidgetTypeInit(UI_WTYPE_HSV_VERT);
				}
				/* other HSV pickers */
				else {
					wt = WidgetTypeInit(UI_WTYPE_HSV_CUBE);
				}
				break;
				
			case UI_BTYPE_HSVCIRCLE:
				wt = WidgetTypeInit(UI_WTYPE_HSV_CIRCLE);
				break;
				
			case UI_BTYPE_COLORBAND:
				wt = WidgetTypeInit(UI_WTYPE_COLORBAND);
				break;
				
			case UI_BTYPE_UNITVEC:
				wt = WidgetTypeInit(UI_WTYPE_UNITVEC);
				break;
				
			case UI_BTYPE_IMAGE:
				ui_draw_but_IMAGE(ar, but, &tui->wcol_regular, rect);
				break;
			
			case UI_BTYPE_HISTOGRAM:
				ui_draw_but_HISTOGRAM(ar, but, &tui->wcol_regular, rect);
				break;
				
			case UI_BTYPE_WAVEFORM:
				ui_draw_but_WAVEFORM(ar, but, &tui->wcol_regular, rect);
				break;
				
			case UI_BTYPE_VECTORSCOPE:
				ui_draw_but_VECTORSCOPE(ar, but, &tui->wcol_regular, rect);
				break;
					
			case UI_BTYPE_CURVE:
				ui_draw_but_CURVE(ar, but, &tui->wcol_regular, rect);
				break;
				
			case UI_BTYPE_PROGRESS_BAR:
				wt = WidgetTypeInit(UI_WTYPE_PROGRESSBAR);
				fstyle = &style->widgetlabel;
				break;

			case UI_BTYPE_SCROLL:
				wt = WidgetTypeInit(UI_WTYPE_LISTSCROLL);
				break;

			case UI_BTYPE_GRIP:
				wt = WidgetTypeInit(UI_WTYPE_ICON);
				break;

			case UI_BTYPE_TRACK_PREVIEW:
				ui_draw_but_TRACKPREVIEW(ar, but, &tui->wcol_regular, rect);
				break;

			case UI_BTYPE_NODE_SOCKET:
				ui_draw_but_NODESOCKET(ar, but, &tui->wcol_regular, rect);
				break;

			default:
				wt = WidgetTypeInit(UI_WTYPE_REGULAR);
				break;
		}
	}
	
	if (wt) {
		//rcti disablerect = *rect; /* rect gets clipped smaller for text */
		int roundboxalign, state;
		bool disabled = false;
		
		roundboxalign = widget_roundbox_set(but, rect);

		state = but->flag;

		if ((but->editstr) ||
		    (UNLIKELY(but->flag & UI_BUT_DRAG_MULTI) && ui_but_drag_multi_edit_get(but)))
		{
			state |= UI_TEXTINPUT;
		}

		if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE))
			if (but->dt != UI_EMBOSS_PULLDOWN)
				disabled = true;
		
		if (disabled)
			ui_widget_color_disabled(wt);

		/* callback routine */
		WidgetDraw(wt, fstyle, but, rect, state, roundboxalign, but->icon, but->drawstr, disabled);
	}
}

void ui_draw_menu_back(uiStyle *UNUSED(style), uiBlock *block, rcti *rect)
{
	uiWidgetType *wt = WidgetTypeInit(UI_WTYPE_MENU_BACK);
	const int flag = block ? block->flag : 0;
	const char dir = block ? block->direction : 0;

	WidgetDraw(wt, NULL, NULL, rect, flag, dir, 0, NULL, false);

	if (block) {
		if (block->flag & UI_BLOCK_CLIPTOP) {
			/* XXX no scaling for UI here yet */
			glColor3ubv((unsigned char *)wt->wcol.text);
			UI_draw_icon_tri(BLI_rcti_cent_x(rect), rect->ymax - 8, 't');
		}
		if (block->flag & UI_BLOCK_CLIPBOTTOM) {
			/* XXX no scaling for UI here yet */
			glColor3ubv((unsigned char *)wt->wcol.text);
			UI_draw_icon_tri(BLI_rcti_cent_x(rect), rect->ymin + 10, 'v');
		}
	}
}

static void draw_disk_shaded(
        float start, float angle,
        float radius_int, float radius_ext, int subd,
        const char col1[4], const char col2[4],
        bool shaded)
{
	const float radius_ext_scale = (0.5f / radius_ext);  /* 1 / (2 * radius_ext) */
	int i;

	float s, c;
	float y1, y2;
	float fac;
	unsigned char r_col[4];

	glBegin(GL_TRIANGLE_STRIP);

	s = sinf(start);
	c = cosf(start);

	y1 = s * radius_int;
	y2 = s * radius_ext;

	if (shaded) {
		fac = (y1 + radius_ext) * radius_ext_scale;
		round_box_shade_col4_r(r_col, col1, col2, fac);

		glColor4ubv(r_col);
	}

	glVertex2f(c * radius_int, s * radius_int);

	if (shaded) {
		fac = (y2 + radius_ext) * radius_ext_scale;
		round_box_shade_col4_r(r_col, col1, col2, fac);

		glColor4ubv(r_col);
	}
	glVertex2f(c * radius_ext, s * radius_ext);

	for (i = 1; i < subd; i++) {
		float a;

		a = start + ((i) / (float)(subd - 1)) * angle;
		s = sinf(a);
		c = cosf(a);
		y1 = s * radius_int;
		y2 = s * radius_ext;

		if (shaded) {
			fac = (y1 + radius_ext) * radius_ext_scale;
			round_box_shade_col4_r(r_col, col1, col2, fac);

			glColor4ubv(r_col);
		}
		glVertex2f(c * radius_int, s * radius_int);

		if (shaded) {
			fac = (y2 + radius_ext) * radius_ext_scale;
			round_box_shade_col4_r(r_col, col1, col2, fac);

			glColor4ubv(r_col);
		}
		glVertex2f(c * radius_ext, s * radius_ext);
	}
	glEnd();
}

void ui_draw_pie_center(uiBlock *block)
{
	bTheme *btheme = UI_GetTheme();
	float cx = block->pie_data.pie_center_spawned[0];
	float cy = block->pie_data.pie_center_spawned[1];

	float *pie_dir = block->pie_data.pie_dir;

	float pie_radius_internal = U.pixelsize * U.pie_menu_threshold;
	float pie_radius_external = U.pixelsize * (U.pie_menu_threshold + 7.0f);

	int subd = 40;

	float angle = atan2f(pie_dir[1], pie_dir[0]);
	float range = (block->pie_data.flags & UI_PIE_DEGREES_RANGE_LARGE) ? M_PI_2 : M_PI_4;

	glPushMatrix();
	glTranslatef(cx, cy, 0.0f);

	glEnable(GL_BLEND);
	if (btheme->tui.wcol_pie_menu.shaded) {
		char col1[4], col2[4];
		shadecolors4(col1, col2, btheme->tui.wcol_pie_menu.inner, btheme->tui.wcol_pie_menu.shadetop, btheme->tui.wcol_pie_menu.shadedown);
		draw_disk_shaded(0.0f, (float)(M_PI * 2.0), pie_radius_internal, pie_radius_external, subd, col1, col2, true);
	}
	else {
		glColor4ubv((GLubyte *)btheme->tui.wcol_pie_menu.inner);
		draw_disk_shaded(0.0f, (float)(M_PI * 2.0), pie_radius_internal, pie_radius_external, subd, NULL, NULL, false);
	}

	if (!(block->pie_data.flags & UI_PIE_INVALID_DIR)) {
		if (btheme->tui.wcol_pie_menu.shaded) {
			char col1[4], col2[4];
			shadecolors4(col1, col2, btheme->tui.wcol_pie_menu.inner_sel, btheme->tui.wcol_pie_menu.shadetop, btheme->tui.wcol_pie_menu.shadedown);
			draw_disk_shaded(angle - range / 2.0f, range, pie_radius_internal, pie_radius_external, subd, col1, col2, true);
		}
		else {
			glColor4ubv((GLubyte *)btheme->tui.wcol_pie_menu.inner_sel);
			draw_disk_shaded(angle - range / 2.0f, range, pie_radius_internal, pie_radius_external, subd, NULL, NULL, false);
		}
	}

	glColor4ubv((GLubyte *)btheme->tui.wcol_pie_menu.outline);
	glutil_draw_lined_arc(0.0f, (float)M_PI * 2.0f, pie_radius_internal, subd);
	glutil_draw_lined_arc(0.0f, (float)M_PI * 2.0f, pie_radius_external, subd);

	if (U.pie_menu_confirm > 0 && !(block->pie_data.flags & (UI_PIE_INVALID_DIR | UI_PIE_CLICK_STYLE))) {
		float pie_confirm_radius = U.pixelsize * (pie_radius_internal + U.pie_menu_confirm);
		float pie_confirm_external = U.pixelsize * (pie_radius_internal + U.pie_menu_confirm + 7.0f);

		glColor4ub(btheme->tui.wcol_pie_menu.text_sel[0], btheme->tui.wcol_pie_menu.text_sel[1], btheme->tui.wcol_pie_menu.text_sel[2], 64);
		draw_disk_shaded(angle - range / 2.0f, range, pie_confirm_radius, pie_confirm_external, subd, NULL, NULL, false);
	}

	glDisable(GL_BLEND);
	glPopMatrix();
}


uiWidgetColors *ui_tooltip_get_theme(void)
{
	uiWidgetType *wt = WidgetTypeInit(UI_WTYPE_TOOLTIP);
	return wt->wcol_theme;
}

void ui_draw_tooltip_background(uiStyle *UNUSED(style), uiBlock *UNUSED(block), rcti *rect)
{
	uiWidgetType *wt = WidgetTypeInit(UI_WTYPE_TOOLTIP);
	WidgetDraw(wt, NULL, NULL, rect, 0, 0, 0, NULL, false);
}

void ui_draw_search_back(uiStyle *UNUSED(style), uiBlock *block, rcti *rect)
{
	uiWidgetType *wt = WidgetTypeInit(UI_WTYPE_BOX);
	
	glEnable(GL_BLEND);
	widget_drawbase_softshadow(rect, UI_CNR_ALL, 0.25f * U.widget_unit);
	glDisable(GL_BLEND);

	if (block)
		WidgetDraw(wt, NULL, NULL, rect, block->flag, UI_CNR_ALL, 0, NULL, false);
	else
		WidgetDraw(wt, NULL, NULL, rect, 0, UI_CNR_ALL, 0, NULL, false);
}


/* helper call to draw a menu item without button */
/* state: UI_ACTIVE or 0 */
void ui_draw_menu_item(uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state, bool use_sep)
{
	uiWidgetType *wt = WidgetTypeInit(UI_WTYPE_MENU_ITEM);
	rcti _rect = *rect;
	char *cpoin = NULL;

	WidgetDraw(wt, NULL, NULL, rect, state, 0, 0, NULL, false);

	UI_fontstyle_set(fstyle);
	fstyle->align = UI_STYLE_TEXT_LEFT;
	
	/* text location offset */
	rect->xmin += 0.25f * UI_UNIT_X;
	if (iconid) rect->xmin += UI_DPI_ICON_SIZE;

	/* cut string in 2 parts? */
	if (use_sep) {
		cpoin = strchr(name, UI_SEP_CHAR);
		if (cpoin) {
			*cpoin = 0;

			/* need to set this first */
			UI_fontstyle_set(fstyle);

			if (fstyle->kerning == 1) { /* for BLF_width */
				BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
			}

			rect->xmax -= BLF_width(fstyle->uifont_id, cpoin + 1, INT_MAX) + UI_DPI_ICON_SIZE;

			if (fstyle->kerning == 1) {
				BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
			}
		}
	}

	{
		char drawstr[UI_MAX_DRAW_STR];
		const float okwidth = (float)BLI_rcti_size_x(rect);
		const size_t max_len = sizeof(drawstr);
		const float minwidth = (float)(UI_DPI_ICON_SIZE);

		BLI_strncpy(drawstr, name, sizeof(drawstr));
		UI_text_clip_middle_ex(fstyle, drawstr, okwidth, minwidth, max_len, '\0');

		glColor4ubv((unsigned char *)wt->wcol.text);
		UI_fontstyle_draw(fstyle, rect, drawstr);
	}

	/* part text right aligned */
	if (use_sep) {
		if (cpoin) {
			fstyle->align = UI_STYLE_TEXT_RIGHT;
			rect->xmax = _rect.xmax - 5;
			UI_fontstyle_draw(fstyle, rect, cpoin + 1);
			*cpoin = UI_SEP_CHAR;
		}
	}
	
	/* restore rect, was messed with */
	*rect = _rect;

	if (iconid) {
		float height, aspect;
		int xs = rect->xmin + 0.2f * UI_UNIT_X;
		int ys = rect->ymin + 0.1f * BLI_rcti_size_y(rect);

		height = ICON_SIZE_FROM_BUTRECT(rect);
		aspect = ICON_DEFAULT_HEIGHT / height;
		
		glEnable(GL_BLEND);
		UI_icon_draw_aspect(xs, ys, iconid, aspect, 1.0f); /* XXX scale weak get from fstyle? */
		glDisable(GL_BLEND);
	}
}

void ui_draw_preview_item(uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state)
{
	uiWidgetType *wt = WidgetTypeInit(UI_WTYPE_MENU_ITEM_PREVIEW);

	/* drawing button background */
	WidgetDraw(wt, fstyle, NULL, rect, state, UI_CNR_ALL, iconid, name, false);
	/* XXX special case: we don't have a but here so NULL pointer needs to be allowed for it. */
	wt->draw_type->text(fstyle, &wt->wcol, NULL, rect, name, iconid);
}
