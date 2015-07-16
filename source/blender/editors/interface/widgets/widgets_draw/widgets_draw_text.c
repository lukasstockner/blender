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

/** \file blender/editors/interface/widgets/widgets_draw/widgets_draw_text.c
 *  \ingroup edinterface
 */

#include <limits.h>
#include <string.h>

#include "BIF_gl.h"

#include "BLF_api.h"

#include "BLI_math_base.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "../interface_intern.h"


#include "widgets_draw_intern.h"



#define UI_TEXT_CLIP_MARGIN (0.25f * U.widget_unit / but->block->aspect)

/* icons are 80% of height of button (16 pixels inside 20 height) */
#define ICON_SIZE_FROM_BUTRECT(rect) (0.8f * BLI_rcti_size_y(rect))

#define PREVIEW_PAD 4

void widget_draw_preview(BIFIconID icon, float alpha, const rcti *rect)
{
	int w, h, size;

	if (icon == ICON_NONE)
		return;

	w = BLI_rcti_size_x(rect);
	h = BLI_rcti_size_y(rect);
	size = MIN2(w, h);
	size -= PREVIEW_PAD * 2;  /* padding */

	if (size > 0) {
		int x = rect->xmin + w / 2 - size / 2;
		int y = rect->ymin + h / 2 - size / 2;

		UI_icon_draw_preview_aspect_size(x, y, icon, 1.0f, alpha, size);
	}
}


static int ui_but_draw_menu_icon(const uiBut *but)
{
	return (but->flag & UI_BUT_ICON_SUBMENU) && (but->dt == UI_EMBOSS_PULLDOWN);
}

/* icons have been standardized... and this call draws in untransformed coordinates */

static void widget_draw_icon(
        const uiBut *but, BIFIconID icon, float alpha, const rcti *rect,
        const bool show_menu_icon)
{
	float xs = 0.0f, ys = 0.0f;
	float aspect, height;

	if (but->flag & UI_BUT_ICON_PREVIEW) {
		glEnable(GL_BLEND);
		widget_draw_preview(icon, alpha, rect);
		glDisable(GL_BLEND);
		return;
	}

	/* this icon doesn't need draw... */
	if (icon == ICON_BLANK1 && (but->flag & UI_BUT_ICON_SUBMENU) == 0) return;

	aspect = but->block->aspect / UI_DPI_FAC;
	height = ICON_DEFAULT_HEIGHT / aspect;

	/* calculate blend color */
	if (ELEM(but->type, UI_BTYPE_TOGGLE, UI_BTYPE_ROW, UI_BTYPE_TOGGLE_N, UI_BTYPE_LISTROW)) {
		if (but->flag & UI_SELECT) {}
		else if (but->flag & UI_ACTIVE) {}
		else alpha = 0.5f;
	}

	/* extra feature allows more alpha blending */
	if ((but->type == UI_BTYPE_LABEL) && but->a1 == 1.0f)
		alpha *= but->a2;

	glEnable(GL_BLEND);

	if (icon && icon != ICON_BLANK1) {
		float ofs = 1.0f / aspect;

		if (but->drawflag & UI_BUT_ICON_LEFT) {
			if (but->block->flag & UI_BLOCK_LOOP) {
				if (but->type == UI_BTYPE_SEARCH_MENU)
					xs = rect->xmin + 4.0f * ofs;
				else
					xs = rect->xmin + ofs;
			}
			else {
				xs = rect->xmin + 4.0f * ofs;
			}
			ys = (rect->ymin + rect->ymax - height) / 2.0f;
		}
		else {
			xs = (rect->xmin + rect->xmax - height) / 2.0f;
			ys = (rect->ymin + rect->ymax - height) / 2.0f;
		}

		/* force positions to integers, for zoom levels near 1. draws icons crisp. */
		if (aspect > 0.95f && aspect < 1.05f) {
			xs = (int)(xs + 0.1f);
			ys = (int)(ys + 0.1f);
		}

		/* to indicate draggable */
		if (but->dragpoin && (but->flag & UI_ACTIVE)) {
			float rgb[3] = {1.25f, 1.25f, 1.25f};
			UI_icon_draw_aspect_color(xs, ys, icon, aspect, rgb);
		}
		else
			UI_icon_draw_aspect(xs, ys, icon, aspect, alpha);
	}

	if (show_menu_icon) {
		xs = rect->xmax - UI_DPI_ICON_SIZE - aspect;
		ys = (rect->ymin + rect->ymax - height) / 2.0f;

		UI_icon_draw_aspect(xs, ys, ICON_RIGHTARROW_THIN, aspect, alpha);
	}

	glDisable(GL_BLEND);
}

static void ui_text_clip_give_prev_off(uiBut *but, const char *str)
{
	const char *prev_utf8 = BLI_str_find_prev_char_utf8(str, str + but->ofs);
	int bytes = str + but->ofs - prev_utf8;

	but->ofs -= bytes;
}

static void ui_text_clip_give_next_off(uiBut *but, const char *str)
{
	const char *next_utf8 = BLI_str_find_next_char_utf8(str + but->ofs, NULL);
	int bytes = next_utf8 - (str + but->ofs);

	but->ofs += bytes;
}

/**
 * Helper.
 * This func assumes things like kerning handling have already been handled!
 * Return the length of modified (right-clipped + ellipsis) string.
 */
static void ui_text_clip_right_ex(
        uiFontStyle *fstyle, char *str, const size_t max_len, const float okwidth,
        const char *sep, const int sep_len, const float sep_strwidth, size_t *r_final_len)
{
	float tmp;
	int l_end;

	BLI_assert(str[0]);

	/* If the trailing ellipsis takes more than 20% of all available width, just cut the string
	 * (as using the ellipsis would remove even more useful chars, and we cannot show much already!).
	 */
	if (sep_strwidth / okwidth > 0.2f) {
		l_end = BLF_width_to_strlen(fstyle->uifont_id, str, max_len, okwidth, &tmp);
		str[l_end] = '\0';
		if (r_final_len) {
			*r_final_len = (size_t)l_end;
		}
	}
	else {
		l_end = BLF_width_to_strlen(fstyle->uifont_id, str, max_len, okwidth - sep_strwidth, &tmp);
		memcpy(str + l_end, sep, sep_len + 1);  /* +1 for trailing '\0'. */
		if (r_final_len) {
			*r_final_len = (size_t)(l_end + sep_len);
		}
	}
}

/**
 * Cut off the middle of the text to fit into the given width.
 * Note in case this middle clipping would just remove a few chars, it rather clips right, which is more readable.
 * If rpart_sep is not Null, the part of str starting to first occurrence of rpart_sep is preserved at all cost (useful
 * for strings with shortcuts, like 'AVeryLongFooBarLabelForMenuEntry|Ctrl O' -> 'AVeryLong...MenuEntry|Ctrl O').
 */
float UI_text_clip_middle_ex(
        uiFontStyle *fstyle, char *str, float okwidth, const float minwidth,
        const size_t max_len, const char rpart_sep)
{
	float strwidth;

	/* Add some epsilon to OK width, avoids 'ellipsing' text that nearly fits!
	 * Better to have a small piece of the last char cut out, than two remaining chars replaced by an allipsis... */
	okwidth += 1.0f + UI_DPI_FAC;

	BLI_assert(str[0]);

	/* need to set this first */
	UI_fontstyle_set(fstyle);

	if (fstyle->kerning == 1) {  /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	}

	strwidth = BLF_width(fstyle->uifont_id, str, max_len);

	if ((okwidth > 0.0f) && (strwidth > okwidth)) {
		/* utf8 ellipsis '...', some compilers complain */
		const char sep[] = {0xe2, 0x80, 0xa6, 0x0};
		const int sep_len = sizeof(sep) - 1;
		const float sep_strwidth = BLF_width(fstyle->uifont_id, sep, sep_len + 1);
		float parts_strwidth;
		size_t l_end;

		char *rpart = NULL, rpart_buf[UI_MAX_DRAW_STR];
		float rpart_width = 0.0f;
		size_t rpart_len = 0;
		size_t final_lpart_len;

		if (rpart_sep) {
			rpart = strrchr(str, rpart_sep);

			if (rpart) {
				rpart_len = strlen(rpart);
				rpart_width = BLF_width(fstyle->uifont_id, rpart, rpart_len);
				okwidth -= rpart_width;
				strwidth -= rpart_width;

				if (okwidth < 0.0f) {
					/* Not enough place for actual label, just display protected right part.
					 * Here just for safety, should never happen in real life! */
					memmove(str, rpart, rpart_len + 1);
					rpart = NULL;
					okwidth += rpart_width;
					strwidth = rpart_width;
				}
			}
		}

		parts_strwidth = (okwidth - sep_strwidth) / 2.0f;

		if (rpart) {
			strcpy(rpart_buf, rpart);
			*rpart = '\0';
			rpart = rpart_buf;
		}

		l_end = BLF_width_to_strlen(fstyle->uifont_id, str, max_len, parts_strwidth, &rpart_width);
		if (l_end < 10 || min_ff(parts_strwidth, strwidth - okwidth) < minwidth) {
			/* If we really have no place, or we would clip a very small piece of string in the middle,
			 * only show start of string.
			 */
			ui_text_clip_right_ex(fstyle, str, max_len, okwidth, sep, sep_len, sep_strwidth, &final_lpart_len);
		}
		else {
			size_t r_offset, r_len;

			r_offset = BLF_width_to_rstrlen(fstyle->uifont_id, str, max_len, parts_strwidth, &rpart_width);
			r_len = strlen(str + r_offset) + 1;  /* +1 for the trailing '\0'. */

			if (l_end + sep_len + r_len + rpart_len > max_len) {
				/* Corner case, the str already takes all available mem, and the ellipsis chars would actually
				 * add more chars...
				 * Better to just trim one or two letters to the right in this case...
				 * Note: with a single-char ellipsis, this should never happen! But better be safe here...
				 */
				ui_text_clip_right_ex(fstyle, str, max_len, okwidth, sep, sep_len, sep_strwidth, &final_lpart_len);
			}
			else {
				memmove(str + l_end + sep_len, str + r_offset, r_len);
				memcpy(str + l_end, sep, sep_len);
				final_lpart_len = (size_t)(l_end + sep_len + r_len - 1);  /* -1 to remove trailing '\0'! */
			}
		}

		if (rpart) {
			/* Add back preserved right part to our shorten str. */
			memcpy(str + final_lpart_len, rpart, rpart_len + 1);  /* +1 for trailing '\0'. */
		}

		strwidth = BLF_width(fstyle->uifont_id, str, max_len);
	}

	if (fstyle->kerning == 1) {
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	}

	return strwidth;
}

/**
 * Wrapper around UI_text_clip_middle_ex.
 */
static void ui_text_clip_middle(uiFontStyle *fstyle, uiBut *but, const rcti *rect)
{
	/* No margin for labels! */
	const int border = ELEM(but->type, UI_BTYPE_LABEL, UI_BTYPE_MENU) ? 0 : (int)(UI_TEXT_CLIP_MARGIN + 0.5f);
	const float okwidth = (float)max_ii(BLI_rcti_size_x(rect) - border, 0);
	const size_t max_len = sizeof(but->drawstr);
	const float minwidth = (float)(UI_DPI_ICON_SIZE) / but->block->aspect * 2.0f;

	but->ofs = 0;
	but->strwidth = UI_text_clip_middle_ex(fstyle, but->drawstr, okwidth, minwidth, max_len, '\0');
}

/**
 * Like ui_text_clip_middle(), but protect/preserve at all cost the right part of the string after sep.
 * Useful for strings with shortcuts (like 'AVeryLongFooBarLabelForMenuEntry|Ctrl O' -> 'AVeryLong...MenuEntry|Ctrl O').
 */
static void ui_text_clip_middle_protect_right(uiFontStyle *fstyle, uiBut *but, const rcti *rect, const char rsep)
{
	/* No margin for labels! */
	const int border = ELEM(but->type, UI_BTYPE_LABEL, UI_BTYPE_MENU) ? 0 : (int)(UI_TEXT_CLIP_MARGIN + 0.5f);
	const float okwidth = (float)max_ii(BLI_rcti_size_x(rect) - border, 0);
	const size_t max_len = sizeof(but->drawstr);
	const float minwidth = (float)(UI_DPI_ICON_SIZE) / but->block->aspect * 2.0f;

	but->ofs = 0;
	but->strwidth = UI_text_clip_middle_ex(fstyle, but->drawstr, okwidth, minwidth, max_len, rsep);
}

/**
 * Cut off the text, taking into account the cursor location (text display while editing).
 */
static void ui_text_clip_cursor(uiFontStyle *fstyle, uiBut *but, const rcti *rect)
{
	const int border = (int)(UI_TEXT_CLIP_MARGIN + 0.5f);
	const int okwidth = max_ii(BLI_rcti_size_x(rect) - border, 0);

	BLI_assert(but->editstr && but->pos >= 0);

	/* need to set this first */
	UI_fontstyle_set(fstyle);

	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);

	/* define ofs dynamically */
	if (but->ofs > but->pos)
		but->ofs = but->pos;

	if (BLF_width(fstyle->uifont_id, but->editstr, INT_MAX) <= okwidth)
		but->ofs = 0;

	but->strwidth = BLF_width(fstyle->uifont_id, but->editstr + but->ofs, INT_MAX);

	if (but->strwidth > okwidth) {
		int len = strlen(but->editstr);

		while (but->strwidth > okwidth) {
			float width;

			/* string position of cursor */
			width = BLF_width(fstyle->uifont_id, but->editstr + but->ofs, (but->pos - but->ofs));

			/* if cursor is at 20 pixels of right side button we clip left */
			if (width > okwidth - 20) {
				ui_text_clip_give_next_off(but, but->editstr);
			}
			else {
				int bytes;
				/* shift string to the left */
				if (width < 20 && but->ofs > 0)
					ui_text_clip_give_prev_off(but, but->editstr);
				bytes = BLI_str_utf8_size(BLI_str_find_prev_char_utf8(but->editstr, but->editstr + len));
				if (bytes == -1)
					bytes = 1;
				len -= bytes;
			}

			but->strwidth = BLF_width(fstyle->uifont_id, but->editstr + but->ofs, len - but->ofs);

			if (but->strwidth < 10) break;
		}
	}

	if (fstyle->kerning == 1) {
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	}
}

/**
 * Cut off the end of text to fit into the width of \a rect.
 *
 * \note deals with ': ' especially for number buttons
 */
static void ui_text_clip_right_label(uiFontStyle *fstyle, uiBut *but, const rcti *rect)
{
	const int border = UI_TEXT_CLIP_MARGIN + 1;
	const int okwidth = max_ii(BLI_rcti_size_x(rect) - border, 0);
	char *cpoin = NULL;
	int drawstr_len = strlen(but->drawstr);
	const char *cpend = but->drawstr + drawstr_len;

	/* need to set this first */
	UI_fontstyle_set(fstyle);

	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);

	but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr, sizeof(but->drawstr));
	but->ofs = 0;

	/* First shorten num-buttons eg,
	 *   Translucency: 0.000
	 * becomes
	 *   Trans: 0.000
	 */

	/* find the space after ':' separator */
	cpoin = strrchr(but->drawstr, ':');

	if (cpoin && (cpoin < cpend - 2)) {
		char *cp2 = cpoin;

		/* chop off the leading text, starting from the right */
		while (but->strwidth > okwidth && cp2 > but->drawstr) {
			const char *prev_utf8 = BLI_str_find_prev_char_utf8(but->drawstr, cp2);
			int bytes = cp2 - prev_utf8;

			/* shift the text after and including cp2 back by 1 char, +1 to include null terminator */
			memmove(cp2 - bytes, cp2, drawstr_len + 1);
			cp2 -= bytes;

			drawstr_len -= bytes;
			// BLI_assert(strlen(but->drawstr) == drawstr_len);

			but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs, sizeof(but->drawstr) - but->ofs);
			if (but->strwidth < 10) break;
		}

		/* after the leading text is gone, chop off the : and following space, with ofs */
		while ((but->strwidth > okwidth) && (but->ofs < 2)) {
			ui_text_clip_give_next_off(but, but->drawstr);
			but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs, sizeof(but->drawstr) - but->ofs);
			if (but->strwidth < 10) break;
		}
	}

	/* Now just remove trailing chars */
	/* once the label's gone, chop off the least significant digits */
	if (but->strwidth > okwidth) {
		float strwidth;
		drawstr_len = BLF_width_to_strlen(fstyle->uifont_id, but->drawstr + but->ofs,
		                                  drawstr_len - but->ofs, okwidth, &strwidth) + but->ofs;
		but->strwidth = strwidth;
		but->drawstr[drawstr_len] = 0;
	}

	if (fstyle->kerning == 1)
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
}

#ifdef WITH_INPUT_IME
static void widget_draw_text_ime_underline(
        uiFontStyle *fstyle, uiWidgetColors *wcol, uiBut *but, const rcti *rect,
        const wmIMEData *ime_data, const char *drawstr)
{
	int ofs_x, width;
	int rect_x = BLI_rcti_size_x(rect);
	int sel_start = ime_data->sel_start, sel_end = ime_data->sel_end;

	if (drawstr[0] != 0) {
		if (but->pos >= but->ofs) {
			ofs_x = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but->pos - but->ofs);
		}
		else {
			ofs_x = 0;
		}

		width = BLF_width(fstyle->uifont_id, drawstr + but->ofs,
		                  ime_data->composite_len + but->pos - but->ofs);

		glColor4ubv((unsigned char *)wcol->text);
		UI_draw_text_underline(rect->xmin + ofs_x, rect->ymin + 6 * U.pixelsize, min_ii(width, rect_x - 2) - ofs_x, 1);

		/* draw the thick line */
		if (sel_start != -1 && sel_end != -1) {
			sel_end -= sel_start;
			sel_start += but->pos;

			if (sel_start >= but->ofs) {
				ofs_x = BLF_width(fstyle->uifont_id, drawstr + but->ofs, sel_start - but->ofs);
			}
			else {
				ofs_x = 0;
			}

			width = BLF_width(fstyle->uifont_id, drawstr + but->ofs,
			                  sel_end + sel_start - but->ofs);

			UI_draw_text_underline(rect->xmin + ofs_x, rect->ymin + 6 * U.pixelsize, min_ii(width, rect_x - 2) - ofs_x, 2);
		}
	}
}
#endif  /* WITH_INPUT_IME */

static void widget_draw_text(uiFontStyle *fstyle, uiWidgetColors *wcol, uiBut *but, rcti *rect)
{
	int drawstr_left_len = UI_MAX_DRAW_STR;
	const char *drawstr = but->drawstr;
	const char *drawstr_right = NULL;
	bool use_right_only = false;

#ifdef WITH_INPUT_IME
	const wmIMEData *ime_data;
#endif

	UI_fontstyle_set(fstyle);

	if (but->editstr || (but->drawflag & UI_BUT_TEXT_LEFT))
		fstyle->align = UI_STYLE_TEXT_LEFT;
	else if (but->drawflag & UI_BUT_TEXT_RIGHT)
		fstyle->align = UI_STYLE_TEXT_RIGHT;
	else
		fstyle->align = UI_STYLE_TEXT_CENTER;

	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);

	/* Special case: when we're entering text for multiple buttons,
	 * don't draw the text for any of the multi-editing buttons */
	if (UNLIKELY(but->flag & UI_BUT_DRAG_MULTI)) {
		uiBut *but_edit = ui_but_drag_multi_edit_get(but);
		if (but_edit) {
			drawstr = but_edit->editstr;
			fstyle->align = UI_STYLE_TEXT_LEFT;
		}
	}
	else {
		if (but->editstr) {
			/* max length isn't used in this case,
			 * we rely on string being NULL terminated. */
			drawstr_left_len = INT_MAX;

#ifdef WITH_INPUT_IME
			/* FIXME, IME is modifying 'const char *drawstr! */
			ime_data = ui_but_ime_data_get(but);

			if (ime_data && ime_data->composite_len) {
				/* insert composite string into cursor pos */
				BLI_snprintf((char *)drawstr, UI_MAX_DRAW_STR, "%s%s%s",
				             but->editstr, ime_data->str_composite,
				             but->editstr + but->pos);
			}
			else
#endif
			{
				drawstr = but->editstr;
			}
		}
	}


	/* text button selection, cursor, composite underline */
	if (but->editstr && but->pos != -1) {
		int but_pos_ofs;
		int tx, ty;

		/* text button selection */
		if ((but->selend - but->selsta) > 0) {
			int selsta_draw, selwidth_draw;

			if (drawstr[0] != 0) {

				if (but->selsta >= but->ofs) {
					selsta_draw = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but->selsta - but->ofs);
				}
				else {
					selsta_draw = 0;
				}

				selwidth_draw = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but->selend - but->ofs);

				glColor4ubv((unsigned char *)wcol->item);
				glRecti(rect->xmin + selsta_draw,
				        rect->ymin + 2,
				        min_ii(rect->xmin + selwidth_draw, rect->xmax - 2),
				        rect->ymax - 2);
			}
		}

		/* text cursor */
		but_pos_ofs = but->pos;

#ifdef WITH_INPUT_IME
		/* if is ime compositing, move the cursor */
		if (ime_data && ime_data->composite_len && ime_data->cursor_pos != -1) {
			but_pos_ofs += ime_data->cursor_pos;
		}
#endif

		if (but->pos >= but->ofs) {
			int t;
			if (drawstr[0] != 0) {
				t = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but_pos_ofs - but->ofs);
			}
			else {
				t = 0;
			}

			glColor3f(0.2, 0.6, 0.9);

			tx = rect->xmin + t + 2;
			ty = rect->ymin + 2;

			/* draw cursor */
			glRecti(rect->xmin + t, ty, tx, rect->ymax - 2);
		}

#ifdef WITH_INPUT_IME
		if (ime_data && ime_data->composite_len) {
			/* ime cursor following */
			if (but->pos >= but->ofs) {
				ui_but_ime_reposition(but, tx + 5, ty + 3, false);
			}

			/* composite underline */
			widget_draw_text_ime_underline(fstyle, wcol, but, rect, ime_data, drawstr);
		}
#endif
	}

	if (fstyle->kerning == 1)
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);

#if 0
	ui_rasterpos_safe(x, y, but->aspect);
	transopts = ui_translate_buttons();
#endif

	/* cut string in 2 parts - only for menu entries */
	if ((but->block->flag & UI_BLOCK_LOOP) &&
	    (but->editstr == NULL))
	{
		if (but->flag & UI_BUT_HAS_SEP_CHAR) {
			drawstr_right = strrchr(drawstr, UI_SEP_CHAR);
			if (drawstr_right) {
				drawstr_left_len = (drawstr_right - drawstr);
				drawstr_right++;
			}
		}
	}

#ifdef USE_NUMBUTS_LR_ALIGN
	if (!drawstr_right && ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER) &&
	    /* if we're editing or multi-drag (fake editing), then use left alignment */
	    (but->editstr == NULL) && (drawstr == but->drawstr))
	{
		drawstr_right = strchr(drawstr + but->ofs, ':');
		if (drawstr_right) {
			drawstr_right++;
			drawstr_left_len = (drawstr_right - drawstr);

			while (*drawstr_right == ' ') {
				drawstr_right++;
			}
		}
		else {
			/* no prefix, even so use only cpoin */
			drawstr_right = drawstr + but->ofs;
			use_right_only = true;
		}
	}
#endif

	glColor4ubv((unsigned char *)wcol->text);

	if (!use_right_only) {
		/* for underline drawing */
		float font_xofs, font_yofs;

		UI_fontstyle_draw_ex(fstyle, rect, drawstr + but->ofs,
		                   drawstr_left_len - but->ofs, &font_xofs, &font_yofs);

		if (but->menu_key != '\0') {
			char fixedbuf[128];
			const char *str;

			BLI_strncpy(fixedbuf, drawstr + but->ofs, min_ii(sizeof(fixedbuf), drawstr_left_len));

			str = strchr(fixedbuf, but->menu_key - 32); /* upper case */
			if (str == NULL)
				str = strchr(fixedbuf, but->menu_key);

			if (str) {
				int ul_index = -1;
				float ul_advance;

				ul_index = (int)(str - fixedbuf);

				if (fstyle->kerning == 1) {
					BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
				}

				fixedbuf[ul_index] = '\0';
				ul_advance = BLF_width(fstyle->uifont_id, fixedbuf, ul_index);

				BLF_position(fstyle->uifont_id, rect->xmin + font_xofs + ul_advance, rect->ymin + font_yofs, 0.0f);
				BLF_draw(fstyle->uifont_id, "_", 2);

				if (fstyle->kerning == 1) {
					BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
				}
			}
		}
	}

	/* part text right aligned */
	if (drawstr_right) {
		fstyle->align = UI_STYLE_TEXT_RIGHT;
		rect->xmax -= UI_TEXT_CLIP_MARGIN;
		UI_fontstyle_draw(fstyle, rect, drawstr_right);
	}
}

/* draws text and icons for buttons */
void widget_draw_text_icon(uiFontStyle *fstyle, uiWidgetColors *wcol, uiBut *but, rcti *rect)
{
	const bool show_menu_icon = ui_but_draw_menu_icon(but);
	float alpha = (float)wcol->text[3] / 255.0f;
	char password_str[UI_MAX_DRAW_STR];
	uiButExtraIconType extra_icon_type;

	ui_but_text_password_hide(password_str, but, false);

	/* check for button text label */
	if (but->type == UI_BTYPE_MENU && (but->flag & UI_BUT_NODE_LINK)) {
		rcti temp = *rect;
		temp.xmin = rect->xmax - BLI_rcti_size_y(rect) - 1;
		widget_draw_icon(but, ICON_LAYER_USED, alpha, &temp, false);
	}

	/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
	 * and offset the text label to accommodate it */

	/* Big previews with optional text label below */
	if (but->flag & UI_BUT_ICON_PREVIEW && ui_block_is_menu(but->block)) {
		const BIFIconID icon = (but->flag & UI_HAS_ICON) ? but->icon + but->iconadd : ICON_NONE;
		int icon_size = BLI_rcti_size_y(rect);
		int text_size = 0;

		/* This is a bit britle, but avoids adding an 'UI_BUT_HAS_LABEL' flag to but... */
		if (icon_size > BLI_rcti_size_x(rect)) {
			/* button is not square, it has extra height for label */
			text_size = UI_UNIT_Y;
			icon_size -= text_size;
		}

		/* draw icon in rect above the space reserved for the label */
		rect->ymin += text_size;
		glEnable(GL_BLEND);
		widget_draw_preview(icon, alpha, rect);
		glDisable(GL_BLEND);

		/* offset rect to draw label in */
		rect->ymin -= text_size;
		rect->ymax -= icon_size;

		/* vertically centering text */
		rect->ymin += UI_UNIT_Y / 2;
	}
	/* Icons on the left with optional text label on the right */
	else if (but->flag & UI_HAS_ICON || show_menu_icon) {
		const BIFIconID icon = (but->flag & UI_HAS_ICON) ? but->icon + but->iconadd : ICON_NONE;
		const float icon_size = ICON_SIZE_FROM_BUTRECT(rect);

		/* menu item - add some more padding so menus don't feel cramped. it must
		 * be part of the button so that this area is still clickable */
		if (ui_block_is_menu(but->block))
			rect->xmin += 0.3f * U.widget_unit;

		widget_draw_icon(but, icon, alpha, rect, show_menu_icon);

		rect->xmin += icon_size;
		/* without this menu keybindings will overlap the arrow icon [#38083] */
		if (show_menu_icon) {
			rect->xmax -= icon_size / 2.0f;
		}
	}

	if (but->editstr || (but->drawflag & UI_BUT_TEXT_LEFT)) {
		rect->xmin += (UI_TEXT_MARGIN_X * U.widget_unit) / but->block->aspect;
	}
	else if ((but->drawflag & UI_BUT_TEXT_RIGHT)) {
		rect->xmax -= (UI_TEXT_MARGIN_X * U.widget_unit) / but->block->aspect;
	}

	/* unlink icon for this button type */
	if ((but->type == UI_BTYPE_SEARCH_MENU) &&
	    ((extra_icon_type = ui_but_icon_extra_get(but)) != UI_BUT_ICONEXTRA_NONE))
	{
		rcti temp = *rect;

		temp.xmin = temp.xmax - (BLI_rcti_size_y(rect) * 1.08f);

		if (extra_icon_type == UI_BUT_ICONEXTRA_UNLINK) {
			widget_draw_icon(but, ICON_X, alpha, &temp, false);
		}
		else if (extra_icon_type == UI_BUT_ICONEXTRA_EYEDROPPER) {
			widget_draw_icon(but, ICON_EYEDROPPER, alpha, &temp, false);
		}
		else {
			BLI_assert(0);
		}

		rect->xmax -= ICON_SIZE_FROM_BUTRECT(rect);
	}

	/* clip but->drawstr to fit in available space */
	if (but->editstr && but->pos >= 0) {
		ui_text_clip_cursor(fstyle, but, rect);
	}
	else if (but->drawstr[0] == '\0') {
		/* bypass text clipping on icon buttons */
		but->ofs = 0;
		but->strwidth = 0;
	}
	else if (ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER)) {
		ui_text_clip_right_label(fstyle, but, rect);
	}
	else if (but->flag & UI_BUT_HAS_SEP_CHAR) {
		/* Clip middle, but protect in all case right part containing the shortcut, if any. */
		ui_text_clip_middle_protect_right(fstyle, but, rect, UI_SEP_CHAR);
	}
	else {
		ui_text_clip_middle(fstyle, but, rect);
	}

	/* always draw text for textbutton cursor */
	widget_draw_text(fstyle, wcol, but, rect);

	ui_but_text_password_hide(password_str, but, true);
}

#undef UI_TEXT_CLIP_MARGIN

