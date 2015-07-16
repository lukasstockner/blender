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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/widgets/widgets_draw/drawstyle_classic.c
 *  \ingroup edinterface
 */

#include "BIF_gl.h"

#include "BLF_api.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_brush_types.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "../interface_intern.h" /* XXX */


#include "widgets.h"
#include "widgets_draw_intern.h" /* own include */



static void widget_numbut_embossn(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign);


/* widget drawing ************************************* */

static void widget_box(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetDrawBase wtb;
	float rad;
	char old_col[3];

	widget_drawbase_init(&wtb);

	copy_v3_v3_char(old_col, wcol->inner);

	/* abuse but->hsv - if it's non-zero, use this color as the box's background */
	if (but->col[3]) {
		wcol->inner[0] = but->col[0];
		wcol->inner[1] = but->col[1];
		wcol->inner[2] = but->col[2];
	}

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

	widget_drawbase_draw(&wtb, wcol);

	copy_v3_v3_char(wcol->inner, old_col);
}

static void widget_but(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetDrawBase wtb;
	float rad;

	widget_drawbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

	widget_drawbase_draw(&wtb, wcol);
}

static void widget_checkbox(uiWidgetColors *wcol, rcti *rect, int state, int UNUSED(roundboxalign))
{
	uiWidgetDrawBase wtb;
	rcti recttemp = *rect;
	float rad;
	int delta;

	widget_drawbase_init(&wtb);

	/* square */
	recttemp.xmax = recttemp.xmin + BLI_rcti_size_y(&recttemp);

	/* smaller */
	delta = 1 + BLI_rcti_size_y(&recttemp) / 8;
	recttemp.xmin += delta;
	recttemp.ymin += delta;
	recttemp.xmax -= delta;
	recttemp.ymax -= delta;

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, UI_CNR_ALL, &recttemp, rad);

	/* decoration */
	if (state & UI_SELECT) {
		widget_drawbase_check_trias(&wtb.tria1, &recttemp);
	}

	widget_drawbase_draw(&wtb, wcol);

	/* text space */
	rect->xmin += BLI_rcti_size_y(rect) * 0.7 + delta;
}

static void widget_roundbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetDrawBase wtb;
	const float rad = 0.25f * U.widget_unit;

	widget_drawbase_init(&wtb);

	/* half rounded */
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

	widget_drawbase_draw(&wtb, wcol);
}

static void widget_icon_has_anim(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	if (state & (UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN | UI_BUT_REDALERT)) {
		uiWidgetDrawBase wtb;
		float rad;

		widget_drawbase_init(&wtb);
		wtb.draw_outline = false;

		/* rounded */
		rad = 0.5f * BLI_rcti_size_y(rect);
		widget_drawbase_roundboxedges_set(&wtb, UI_CNR_ALL, rect, rad);
		widget_drawbase_draw(&wtb, wcol);
	}
	else if (but->type == UI_BTYPE_NUM) {
		/* Draw number buttons still with left/right
		 * triangles when field is not embossed */
		widget_numbut_embossn(but, wcol, rect, state, roundboxalign);
	}
}

static void widget_link(uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	if (but->flag & UI_SELECT) {
		rcti rectlink;

		UI_ThemeColor(TH_TEXT_HI);

		rectlink.xmin = BLI_rcti_cent_x(rect);
		rectlink.ymin = BLI_rcti_cent_y(rect);
		rectlink.xmax = but->linkto[0];
		rectlink.ymax = but->linkto[1];

		ui_draw_link_bezier(&rectlink);
	}
}

static void widget_list_itembut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetDrawBase wtb;
	float rad;

	widget_drawbase_init(&wtb);

	/* rounded, but no outline */
	wtb.draw_outline = false;
	rad = 0.2f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, UI_CNR_ALL, rect, rad);

	widget_drawbase_draw(&wtb, wcol);
}

static void widget_menu_back(uiWidgetColors *wcol, rcti *rect, int flag, int direction)
{
	uiWidgetDrawBase wtb;
	int roundboxalign = UI_CNR_ALL;

	widget_drawbase_init(&wtb);

	/* menu is 2nd level or deeper */
	if (flag & UI_BLOCK_POPUP) {
		//rect->ymin -= 4.0;
		//rect->ymax += 4.0;
	}
	else if (direction == UI_DIR_DOWN) {
		roundboxalign = (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
		rect->ymin -= 0.1f * U.widget_unit;
	}
	else if (direction == UI_DIR_UP) {
		roundboxalign = UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT;
		rect->ymax += 0.1f * U.widget_unit;
	}

	glEnable(GL_BLEND);
	widget_drawbase_softshadow(rect, roundboxalign, 0.25f * U.widget_unit);

	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, 0.25f * U.widget_unit);
	wtb.draw_emboss = false;
	widget_drawbase_draw(&wtb, wcol);

	glDisable(GL_BLEND);
}

static void widget_menuiconbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetDrawBase wtb;
	float rad;

	widget_drawbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

	/* decoration */
	widget_drawbase_draw(&wtb, wcol);
}

static void widget_menu_itembut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetDrawBase wtb;

	widget_drawbase_init(&wtb);

	/* not rounded, no outline */
	wtb.draw_outline = false;
	widget_drawbase_roundboxedges_set(&wtb, 0, rect, 0.0f);

	widget_drawbase_draw(&wtb, wcol);
}

static void widget_menu_radial_itembut(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetDrawBase wtb;
	float rad;
	float fac = but->block->pie_data.alphafac;

	widget_drawbase_init(&wtb);

	wtb.draw_emboss = false;

	rad = 0.5f * BLI_rcti_size_y(rect);
	widget_drawbase_roundboxedges_set(&wtb, UI_CNR_ALL, rect, rad);

	wcol->inner[3] *= fac;
	wcol->inner_sel[3] *= fac;
	wcol->item[3] *= fac;
	wcol->text[3] *= fac;
	wcol->text_sel[3] *= fac;
	wcol->outline[3] *= fac;

	widget_drawbase_draw(&wtb, wcol);
}

static void widget_menunodebut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	/* silly node link button hacks */
	uiWidgetDrawBase wtb;
	uiWidgetColors wcol_backup = *wcol;
	float rad;

	widget_drawbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

	wcol->inner[0] = min_ii(wcol->inner[0] + 15, 255);
	wcol->inner[1] = min_ii(wcol->inner[1] + 15, 255);
	wcol->inner[2] = min_ii(wcol->inner[2] + 15, 255);
	wcol->outline[0] = min_ii(wcol->outline[0] + 15, 255);
	wcol->outline[1] = min_ii(wcol->outline[1] + 15, 255);
	wcol->outline[2] = min_ii(wcol->outline[2] + 15, 255);

	/* decoration */
	widget_drawbase_draw(&wtb, wcol);
	*wcol = wcol_backup;
}

static void widget_menubut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetDrawBase wtb;
	float rad;

	widget_drawbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

	/* decoration */
	widget_drawbase_menu_trias(&wtb.tria1, rect);

	widget_drawbase_draw(&wtb, wcol);

	/* text space, arrows are about 0.6 height of button */
	rect->xmax -= (6 * BLI_rcti_size_y(rect)) / 10;
}

static void widget_textbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetDrawBase wtb;
	float rad;

	if (state & UI_SELECT)
		SWAP(short, wcol->shadetop, wcol->shadedown);

	widget_drawbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

	widget_drawbase_draw(&wtb, wcol);
}

static void widget_numbut_draw(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign, bool emboss)
{
	uiWidgetDrawBase wtb;
	const float rad = 0.5f * BLI_rcti_size_y(rect);
	float textofs = rad * 0.85f;

	if (state & UI_SELECT)
		SWAP(short, wcol->shadetop, wcol->shadedown);

	widget_drawbase_init(&wtb);

	if (!emboss) {
		widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);
	}

	/* decoration */
	if (!(state & UI_TEXTINPUT)) {
		widget_drawbase_num_tria(&wtb.tria1, rect, 0.6f, 'l');
		widget_drawbase_num_tria(&wtb.tria2, rect, 0.6f, 'r');
	}

	widget_drawbase_draw(&wtb, wcol);

	if (!(state & UI_TEXTINPUT)) {
		/* text space */
		rect->xmin += textofs;
		rect->xmax -= textofs;
	}
}

static void widget_progressbar(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	rcti rect_prog = *rect, rect_bar = *rect;
	float value = but->a1;
	float w, min;

	/* make the progress bar a proportion of the original height */
	/* hardcoded 4px high for now */
	rect_prog.ymax = rect_prog.ymin + 4 * UI_DPI_FAC;
	rect_bar.ymax = rect_bar.ymin + 4 * UI_DPI_FAC;

	w = value * BLI_rcti_size_x(&rect_prog);

	/* ensure minimium size */
	min = BLI_rcti_size_y(&rect_prog);
	w = MAX2(w, min);

	rect_bar.xmax = rect_bar.xmin + w;

	UI_draw_widget_scroll(wcol, &rect_prog, &rect_bar, UI_SCROLL_NO_OUTLINE);

	/* raise text a bit */
	rect->ymin += 6 * UI_DPI_FAC;
	rect->xmin -= 6 * UI_DPI_FAC;
}

static void widget_pulldownbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	if (state & UI_ACTIVE) {
		uiWidgetDrawBase wtb;
		const float rad = 0.2f * U.widget_unit;

		widget_drawbase_init(&wtb);

		/* half rounded */
		widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

		widget_drawbase_draw(&wtb, wcol);
	}
}

static void widget_radiobut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetDrawBase wtb;
	float rad;

	widget_drawbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

	widget_drawbase_draw(&wtb, wcol);
}

static void widget_scroll(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int UNUSED(roundboxalign))
{
	rcti rect1;
	double value;
	float fac, size, min;
	int horizontal;

	/* calculate slider part */
	value = ui_but_value_get(but);

	size = (but->softmax + but->a1 - but->softmin);
	size = max_ff(size, 2.0f);

	/* position */
	rect1 = *rect;

	/* determine horizontal/vertical */
	horizontal = (BLI_rcti_size_x(rect) > BLI_rcti_size_y(rect));

	if (horizontal) {
		fac = BLI_rcti_size_x(rect) / size;
		rect1.xmin = rect1.xmin + ceilf(fac * ((float)value - but->softmin));
		rect1.xmax = rect1.xmin + ceilf(fac * (but->a1 - but->softmin));

		/* ensure minimium size */
		min = BLI_rcti_size_y(rect);

		if (BLI_rcti_size_x(&rect1) < min) {
			rect1.xmax = rect1.xmin + min;

			if (rect1.xmax > rect->xmax) {
				rect1.xmax = rect->xmax;
				rect1.xmin = max_ii(rect1.xmax - min, rect->xmin);
			}
		}
	}
	else {
		fac = BLI_rcti_size_y(rect) / size;
		rect1.ymax = rect1.ymax - ceilf(fac * ((float)value - but->softmin));
		rect1.ymin = rect1.ymax - ceilf(fac * (but->a1 - but->softmin));

		/* ensure minimium size */
		min = BLI_rcti_size_x(rect);

		if (BLI_rcti_size_y(&rect1) < min) {
			rect1.ymax = rect1.ymin + min;

			if (rect1.ymax > rect->ymax) {
				rect1.ymax = rect->ymax;
				rect1.ymin = max_ii(rect1.ymax - min, rect->ymin);
			}
		}
	}

	if (state & UI_SELECT)
		state = UI_SCROLL_PRESSED;
	else
		state = 0;
	UI_draw_widget_scroll(wcol, rect, &rect1, state);
}

static void widget_numslider(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetDrawBase wtb, wtb1;
	rcti rect1;
	double value;
	float offs, toffs, fac = 0;
	char outline[3];

	widget_drawbase_init(&wtb);
	widget_drawbase_init(&wtb1);

	/* backdrop first */

	/* fully rounded */
	offs = 0.5f * BLI_rcti_size_y(rect);
	toffs = offs * 0.75f;
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, offs);

	wtb.draw_outline = false;
	widget_drawbase_draw(&wtb, wcol);

	/* draw left/right parts only when not in text editing */
	if (!(state & UI_TEXTINPUT)) {
		int roundboxalign_slider;

		/* slider part */
		copy_v3_v3_char(outline, wcol->outline);
		copy_v3_v3_char(wcol->outline, wcol->item);
		copy_v3_v3_char(wcol->inner, wcol->item);

		if (!(state & UI_SELECT))
			SWAP(short, wcol->shadetop, wcol->shadedown);

		rect1 = *rect;

		value = ui_but_value_get(but);
		if ((but->softmax - but->softmin) > 0) {
			fac = ((float)value - but->softmin) * (BLI_rcti_size_x(&rect1) - offs) / (but->softmax - but->softmin);
		}

		/* left part of slider, always rounded */
		rect1.xmax = rect1.xmin + ceil(offs + U.pixelsize);
		widget_drawbase_roundboxedges_set(&wtb1, roundboxalign & ~(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT), &rect1, offs);
		wtb1.draw_outline = false;
		widget_drawbase_draw(&wtb1, wcol);

		/* right part of slider, interpolate roundness */
		rect1.xmax = rect1.xmin + fac + offs;
		rect1.xmin +=  floor(offs - U.pixelsize);

		if (rect1.xmax + offs > rect->xmax) {
			roundboxalign_slider = roundboxalign & ~(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT);
			offs *= (rect1.xmax + offs - rect->xmax) / offs;
		}
		else {
			roundboxalign_slider = 0;
			offs = 0.0f;
		}
		widget_drawbase_roundboxedges_set(&wtb1, roundboxalign_slider, &rect1, offs);

		widget_drawbase_draw(&wtb1, wcol);
		copy_v3_v3_char(wcol->outline, outline);

		if (!(state & UI_SELECT))
			SWAP(short, wcol->shadetop, wcol->shadedown);
	}

	/* outline */
	wtb.draw_outline = true;
	wtb.draw_inner = false;
	widget_drawbase_draw(&wtb, wcol);

	/* add space at either side of the button so text aligns with numbuttons (which have arrow icons) */
	if (!(state & UI_TEXTINPUT)) {
		rect->xmax -= toffs;
		rect->xmin += toffs;
	}
}

/* I think 3 is sufficient border to indicate keyed status */
#define SWATCH_KEYED_BORDER 3

static void widget_swatch(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetDrawBase wtb;
	float rad, col[4];
	bool color_profile = but->block->color_profile;

	col[3] = 1.0f;

	if (but->rnaprop) {
		BLI_assert(but->rnaindex == -1);

		if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
			color_profile = false;

		if (RNA_property_array_length(&but->rnapoin, but->rnaprop) == 4) {
			col[3] = RNA_property_float_get_index(&but->rnapoin, but->rnaprop, 3);
		}
	}

	widget_drawbase_init(&wtb);

	/* half rounded */
	rad = 0.25f * U.widget_unit;
	widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);

	ui_but_v3_get(but, col);

	if (state & (UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN | UI_BUT_REDALERT)) {
		/* draw based on state - color for keyed etc */
		widget_drawbase_draw(&wtb, wcol);

		/* inset to draw swatch color */
		rect->xmin += SWATCH_KEYED_BORDER;
		rect->xmax -= SWATCH_KEYED_BORDER;
		rect->ymin += SWATCH_KEYED_BORDER;
		rect->ymax -= SWATCH_KEYED_BORDER;

		widget_drawbase_roundboxedges_set(&wtb, roundboxalign, rect, rad);
	}

	if (color_profile)
		ui_block_cm_to_display_space_v3(but->block, col);

	rgba_float_to_uchar((unsigned char *)wcol->inner, col);

	wcol->shaded = 0;
	wcol->alpha_check = (wcol->inner[3] < 255);

	widget_drawbase_draw(&wtb, wcol);

	if (but->a1 == UI_PALETTE_COLOR && ((Palette *)but->rnapoin.id.data)->active_color == (int)but->a2) {
		float width = rect->xmax - rect->xmin;
		float height = rect->ymax - rect->ymin;
		/* find color luminance and change it slightly */
		float bw = rgb_to_grayscale(col);

		bw += (bw < 0.5f) ? 0.5f : -0.5f;

		glColor4f(bw, bw, bw, 1.0);
		glBegin(GL_TRIANGLES);
		glVertex2f(rect->xmin + 0.1f * width, rect->ymin + 0.9f * height);
		glVertex2f(rect->xmin + 0.1f * width, rect->ymin + 0.5f * height);
		glVertex2f(rect->xmin + 0.5f * width, rect->ymin + 0.9f * height);
		glEnd();
	}
}

static void widget_unitvec(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	ui_draw_but_UNITVEC(but, wcol, rect);
}


/* states ************************************* */

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

/* labels use Editor theme colors for text */
static void widget_state_label(uiWidgetType *wt, int state)
{
	if (state & UI_BUT_LIST_ITEM) {
		/* Override default label theme's colors. */
		bTheme *btheme = UI_GetTheme();
		wt->wcol_theme = &btheme->tui.wcol_list_item;
		/* call this for option button */
		widget_state(wt, state);
	}
	else {
		/* call this for option button */
		widget_state(wt, state);
		if (state & UI_SELECT)
			UI_GetThemeColor3ubv(TH_TEXT_HI, (unsigned char *)wt->wcol.text);
		else
			UI_GetThemeColor3ubv(TH_TEXT, (unsigned char *)wt->wcol.text);
	}
}

/* special case, menu items */
static void widget_state_menu_item(uiWidgetType *wt, int state)
{
	wt->wcol = *(wt->wcol_theme);

	/* active and disabled (not so common) */
	if ((state & UI_BUT_DISABLED) && (state & UI_ACTIVE)) {
		widget_state_blend(wt->wcol.text, wt->wcol.text_sel, 0.5f);
		/* draw the backdrop at low alpha, helps navigating with keys
		 * when disabled items are active */
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);
		wt->wcol.inner[3] = 64;
	}
	/* regular disabled */
	else if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE)) {
		widget_state_blend(wt->wcol.text, wt->wcol.inner, 0.5f);
	}
	/* regular active */
	else if (state & UI_ACTIVE) {
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);
		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
	}
}

/* special case, pie menu items */
static void widget_state_pie_menu_item(uiWidgetType *wt, int state)
{
	wt->wcol = *(wt->wcol_theme);

	/* active and disabled (not so common) */
	if ((state & UI_BUT_DISABLED) && (state & UI_ACTIVE)) {
		widget_state_blend(wt->wcol.text, wt->wcol.text_sel, 0.5f);
		/* draw the backdrop at low alpha, helps navigating with keys
		 * when disabled items are active */
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.item);
		wt->wcol.inner[3] = 64;
	}
	/* regular disabled */
	else if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE)) {
		widget_state_blend(wt->wcol.text, wt->wcol.inner, 0.5f);
	}
	/* regular active */
	else if (state & UI_SELECT) {
		copy_v4_v4_char(wt->wcol.outline, wt->wcol.inner_sel);
		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
	}
	else if (state & UI_ACTIVE) {
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.item);
		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
	}
}

/* special case, button that calls pulldown */
static void widget_state_pulldown(uiWidgetType *wt, int state)
{
	wt->wcol = *(wt->wcol_theme);

	copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);
	copy_v3_v3_char(wt->wcol.outline, wt->wcol.inner);

	if (state & UI_ACTIVE)
		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
}

static void widget_state_nothing(uiWidgetType *wt, int UNUSED(state))
{
	wt->wcol = *(wt->wcol_theme);
}

/* sliders use special hack which sets 'item' as inner when drawing filling */
static void widget_state_numslider(uiWidgetType *wt, int state)
{
	uiWidgetStateColors *wcol_state = wt->wcol_state;
	float blend = wcol_state->blend - 0.2f; /* XXX special tweak to make sure that bar will still be visible */

	/* call this for option button */
	widget_state(wt, state);

	/* now, set the inner-part so that it reflects state settings too */
	/* TODO: maybe we should have separate settings for the blending colors used for this case? */
	if (state & UI_SELECT) {
		if (state & UI_BUT_ANIMATED_KEY)
			widget_state_blend(wt->wcol.item, wcol_state->inner_key_sel, blend);
		else if (state & UI_BUT_ANIMATED)
			widget_state_blend(wt->wcol.item, wcol_state->inner_anim_sel, blend);
		else if (state & UI_BUT_DRIVEN)
			widget_state_blend(wt->wcol.item, wcol_state->inner_driven_sel, blend);

		if (state & UI_SELECT)
			SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
	}
	else {
		if (state & UI_BUT_ANIMATED_KEY)
			widget_state_blend(wt->wcol.item, wcol_state->inner_key, blend);
		else if (state & UI_BUT_ANIMATED)
			widget_state_blend(wt->wcol.item, wcol_state->inner_anim, blend);
		else if (state & UI_BUT_DRIVEN)
			widget_state_blend(wt->wcol.item, wcol_state->inner_driven, blend);
	}
}


/* text *************************************** */

/* nothing here yet - currently we call widgets_draw_text.c functions directly */


/* helper calls *************************************** */

/**
 * Draw number buttons still with triangles when field is not embossed
 */
static void widget_numbut_embossn(uiBut *UNUSED(but), uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	widget_numbut_draw(wcol, rect, state, roundboxalign, true);
}

static void widget_numbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	widget_numbut_draw(wcol, rect, state, roundboxalign, false);
}


/* uiWidget struct initialization ********************* */

uiWidgetDrawType drawtype_classic_box = {
	/* state */  widget_state,
	/* draw */   widget_but,
	/* custom */ widget_box,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_checkbox = {
	/* state */  widget_state,
	/* draw */   widget_checkbox,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_exec = {
	/* state */  widget_state,
	/* draw */   widget_roundbut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_icon = {
	/* state */  widget_state,
	/* draw */   NULL,
	/* custom */ widget_icon_has_anim,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_label = {
	/* state */  widget_state_label,
	/* draw */   NULL,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_link = {
	/* state */  widget_state,
	/* draw */   NULL,
	/* custom */ widget_link,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_listitem = {
	/* state */  widget_state,
	/* draw */   widget_list_itembut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_menu_back = {
	/* state */  widget_state,
	/* draw */   widget_menu_back,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_menu_icon_radio = {
	/* state */  widget_state,
	/* draw */   widget_menuiconbut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_menu_item = {
	/* state */  widget_state_menu_item,
	/* draw */   widget_menu_itembut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_menu_item_preview = {
	/* state */  widget_state_menu_item,
	/* draw */   widget_menu_itembut,
	/* custom */ NULL,
	/* text */   widget_draw_text_preview_item,
};

uiWidgetDrawType drawtype_classic_menu_item_radial = {
	/* state */  widget_state_pie_menu_item,
	/* draw */   NULL,
	/* custom */ widget_menu_radial_itembut,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_menu_label = {
	/* state */  widget_state_nothing,
	/* draw */   NULL,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_menu_node_link = {
	/* state */  widget_state,
	/* draw */   widget_menunodebut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_menu_pointer_link = {
	/* state */  widget_state,
	/* draw */   widget_menubut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_menu_radio = {
	/* state */  widget_state,
	/* draw */   widget_menubut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_name = {
	/* state */  widget_state,
	/* draw */   widget_textbut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_number = {
	/* state */  widget_state,
	/* draw */   widget_numbut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_progressbar = {
	/* state */  widget_state,
	/* draw */   NULL,
	/* custom */ widget_progressbar,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_pulldown = {
	/* state */  widget_state_pulldown,
	/* draw */   widget_pulldownbut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_radio = {
	/* state */  widget_state,
	/* draw */   widget_radiobut,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_regular = {
	/* state */  widget_state,
	/* draw */   widget_but,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_scroll = {
	/* state */  widget_state_nothing,
	/* draw */   NULL,
	/* custom */ widget_scroll,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_numslider = {
	/* state */  widget_state_numslider,
	/* draw */   NULL,
	/* custom */ widget_numslider,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_swatch = {
	/* state */  widget_state,
	/* draw */   NULL,
	/* custom */ widget_swatch,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_toggle = {
	/* state */  widget_state,
	/* draw */   widget_but,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_tooltip = {
	/* state */  widget_state,
	/* draw */   widget_menu_back,
	/* custom */ NULL,
	/* text */   widget_draw_text_icon,
};

uiWidgetDrawType drawtype_classic_unitvec = {
	/* state */  widget_state,
	/* draw */   NULL,
	/* custom */ widget_unitvec,
	/* text */   widget_draw_text_icon,
};


uiWidgetDrawStyle WidgetStyle_Classic = {
	/* box */               &drawtype_classic_box,
	/* checkbox */          &drawtype_classic_checkbox,
	/* exec */              &drawtype_classic_exec,
	/* filename */          NULL, /* not used (yet?) */
	/* icon */              &drawtype_classic_icon,
	/* label */             &drawtype_classic_label,
	/* link */              &drawtype_classic_link,
	/* listitem */          &drawtype_classic_listitem,
	/* menu_back */         &drawtype_classic_menu_back,
	/* menu_icon_radio */   &drawtype_classic_menu_icon_radio,
	/* menu_item */         &drawtype_classic_menu_item,
	/* menu_item_preview */ &drawtype_classic_menu_item_preview,
	/* menu_item_radial */  &drawtype_classic_menu_item_radial,
	/* menu_item_label */   &drawtype_classic_menu_label,
	/* menu_node_link */    &drawtype_classic_menu_node_link,
	/* menu_pointer_link */ &drawtype_classic_menu_pointer_link, /* not used (yet?) */
	/* menu_radio */        &drawtype_classic_menu_radio,
	/* name */              &drawtype_classic_name,
	/* name_link */         NULL, /* not used (yet?) */
	/* number */            &drawtype_classic_number,
	/* pointer_link */      NULL, /* not used (yet?) */
	/* progressbar */       &drawtype_classic_progressbar,
	/* pulldown */          &drawtype_classic_pulldown,
	/* radio */             &drawtype_classic_radio,
	/* regular */           &drawtype_classic_regular,
	/* rgb_picker */        NULL, /* not used (yet?) */
	/* scroll */            &drawtype_classic_scroll,
	/* slider */            &drawtype_classic_numslider,
	/* swatch */            &drawtype_classic_swatch,
	/* toggle */            &drawtype_classic_toggle,
	/* tooltip */           &drawtype_classic_tooltip,
	/* unitvec */           &drawtype_classic_unitvec,
};

