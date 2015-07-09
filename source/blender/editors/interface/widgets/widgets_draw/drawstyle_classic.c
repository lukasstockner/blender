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

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "UI_interface.h"

#include "../interface_intern.h" /* XXX */


#include "widgets.h"
#include "widgets_draw_intern.h" /* own include */



static void widget_numbut_embossn(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign);


/* widget drawing ************************************* */

static void widget_box(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;
	char old_col[3];

	widgetbase_init(&wtb);

	copy_v3_v3_char(old_col, wcol->inner);

	/* abuse but->hsv - if it's non-zero, use this color as the box's background */
	if (but->col[3]) {
		wcol->inner[0] = but->col[0];
		wcol->inner[1] = but->col[1];
		wcol->inner[2] = but->col[2];
	}

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);

	widgetbase_draw(&wtb, wcol);

	copy_v3_v3_char(wcol->inner, old_col);
}

static void widget_checkbox(uiWidgetColors *wcol, rcti *rect, int state, int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;
	rcti recttemp = *rect;
	float rad;
	int delta;

	widgetbase_init(&wtb);

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
	round_box_edges(&wtb, UI_CNR_ALL, &recttemp, rad);

	/* decoration */
	if (state & UI_SELECT) {
		widget_check_trias(&wtb.tria1, &recttemp);
	}

	widgetbase_draw(&wtb, wcol);

	/* text space */
	rect->xmin += BLI_rcti_size_y(rect) * 0.7 + delta;
}

static void widget_roundbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	const float rad = 0.25f * U.widget_unit;

	widgetbase_init(&wtb);

	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, rad);

	widgetbase_draw(&wtb, wcol);
}

static void widget_icon_has_anim(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	if (state & (UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN | UI_BUT_REDALERT)) {
		uiWidgetBase wtb;
		float rad;

		widgetbase_init(&wtb);
		wtb.draw_outline = false;

		/* rounded */
		rad = 0.5f * BLI_rcti_size_y(rect);
		round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
		widgetbase_draw(&wtb, wcol);
	}
	else if (but->type == UI_BTYPE_NUM) {
		/* Draw number buttons still with left/right
		 * triangles when field is not embossed */
		widget_numbut_embossn(but, wcol, rect, state, roundboxalign);
	}
}

static void widget_list_itembut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;
	float rad;

	widgetbase_init(&wtb);

	/* rounded, but no outline */
	wtb.draw_outline = false;
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, UI_CNR_ALL, rect, rad);

	widgetbase_draw(&wtb, wcol);
}

static void widget_menu_back(uiWidgetColors *wcol, rcti *rect, int flag, int direction)
{
	uiWidgetBase wtb;
	int roundboxalign = UI_CNR_ALL;

	widgetbase_init(&wtb);

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
	widget_softshadow(rect, roundboxalign, 0.25f * U.widget_unit);

	round_box_edges(&wtb, roundboxalign, rect, 0.25f * U.widget_unit);
	wtb.draw_emboss = false;
	widgetbase_draw(&wtb, wcol);

	glDisable(GL_BLEND);
}

static void widget_menuiconbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;

	widgetbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);

	/* decoration */
	widgetbase_draw(&wtb, wcol);
}

static void widget_menu_itembut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;

	widgetbase_init(&wtb);

	/* not rounded, no outline */
	wtb.draw_outline = false;
	round_box_edges(&wtb, 0, rect, 0.0f);

	widgetbase_draw(&wtb, wcol);
}

static void widget_menu_radial_itembut(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;
	float rad;
	float fac = but->block->pie_data.alphafac;

	widgetbase_init(&wtb);

	wtb.draw_emboss = false;

	rad = 0.5f * BLI_rcti_size_y(rect);
	round_box_edges(&wtb, UI_CNR_ALL, rect, rad);

	wcol->inner[3] *= fac;
	wcol->inner_sel[3] *= fac;
	wcol->item[3] *= fac;
	wcol->text[3] *= fac;
	wcol->text_sel[3] *= fac;
	wcol->outline[3] *= fac;

	widgetbase_draw(&wtb, wcol);
}

static void widget_menunodebut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	/* silly node link button hacks */
	uiWidgetBase wtb;
	uiWidgetColors wcol_backup = *wcol;
	float rad;

	widgetbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);

	wcol->inner[0] = min_ii(wcol->inner[0] + 15, 255);
	wcol->inner[1] = min_ii(wcol->inner[1] + 15, 255);
	wcol->inner[2] = min_ii(wcol->inner[2] + 15, 255);
	wcol->outline[0] = min_ii(wcol->outline[0] + 15, 255);
	wcol->outline[1] = min_ii(wcol->outline[1] + 15, 255);
	wcol->outline[2] = min_ii(wcol->outline[2] + 15, 255);

	/* decoration */
	widgetbase_draw(&wtb, wcol);
	*wcol = wcol_backup;
}

static void widget_menubut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;

	widgetbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);

	/* decoration */
	widget_menu_trias(&wtb.tria1, rect);

	widgetbase_draw(&wtb, wcol);

	/* text space, arrows are about 0.6 height of button */
	rect->xmax -= (6 * BLI_rcti_size_y(rect)) / 10;
}

static void widget_textbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;

	if (state & UI_SELECT)
		SWAP(short, wcol->shadetop, wcol->shadedown);

	widgetbase_init(&wtb);

	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);

	widgetbase_draw(&wtb, wcol);
}

static void widget_numbut_draw(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign, bool emboss)
{
	uiWidgetBase wtb;
	const float rad = 0.5f * BLI_rcti_size_y(rect);
	float textofs = rad * 0.85f;

	if (state & UI_SELECT)
		SWAP(short, wcol->shadetop, wcol->shadedown);

	widgetbase_init(&wtb);

	if (!emboss) {
		round_box_edges(&wtb, roundboxalign, rect, rad);
	}

	/* decoration */
	if (!(state & UI_TEXTINPUT)) {
		widget_num_tria(&wtb.tria1, rect, 0.6f, 'l');
		widget_num_tria(&wtb.tria2, rect, 0.6f, 'r');
	}

	widgetbase_draw(&wtb, wcol);

	if (!(state & UI_TEXTINPUT)) {
		/* text space */
		rect->xmin += textofs;
		rect->xmax -= textofs;
	}
}

/* helper calls *************************************** */

/**
 * Draw number buttons still with triangles when field is not embossed
 */
static void widget_numbut_embossn(uiBut *UNUSED(but), uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	widget_numbut_draw(wcol, rect, state, roundboxalign, true);
}


/* uiWidget struct initialization ********************* */

uiWidgetDrawType drawtype_classic_box = {
	/* state */  NULL,
	/* draw */   NULL,
	/* custom */ widget_box,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_checkbox = {
	/* state */  NULL,
	/* draw */   widget_checkbox,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_exec = {
	/* state */  NULL,
	/* draw */   widget_roundbut,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_icon = {
	/* state */  NULL,
	/* draw */   NULL,
	/* custom */ widget_icon_has_anim,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_listitem = {
	/* state */  NULL,
	/* draw */   widget_list_itembut,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_menu_back = {
	/* state */  NULL,
	/* draw */   widget_menu_back,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_menu_icon_radio = {
	/* state */  NULL,
	/* draw */   widget_menuiconbut,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_menu_item = {
	/* state */  NULL,
	/* draw */   widget_menu_itembut,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_menu_item_radial = {
	/* state */  NULL,
	/* draw */   NULL,
	/* custom */ widget_menu_radial_itembut,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_menu_node_link = {
	/* state */  NULL,
	/* draw */   widget_menunodebut,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_menu_pointer_link = {
	/* state */  NULL,
	/* draw */   widget_menubut,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_menu_radio = {
	/* state */  NULL,
	/* draw */   widget_menubut,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawType drawtype_classic_name = {
	/* state */  NULL,
	/* draw */   widget_textbut,
	/* custom */ NULL,
	/* text */   NULL,
};


uiWidgetDrawStyle WidgetStyle_Classic = {
	/* box */               &drawtype_classic_box,
	/* checkbox */          &drawtype_classic_checkbox,
	/* exec */              &drawtype_classic_exec,
	/* filename */          NULL, /* not used (yet?) */
	/* icon */              &drawtype_classic_icon,
	/* label */             NULL,
	/* listitem */          &drawtype_classic_listitem,
	/* menu_back */         &drawtype_classic_menu_back,
	/* menu_icon_radio */   &drawtype_classic_menu_icon_radio,
	/* menu_item */         &drawtype_classic_menu_item,
	/* menu_item_radial */  &drawtype_classic_menu_item_radial,
	/* menu_node_link */    &drawtype_classic_menu_node_link,
	/* menu_pointer_link */ &drawtype_classic_menu_pointer_link, /* not used (yet?) */
	/* menu_radio */        &drawtype_classic_menu_radio,
	/* name */              &drawtype_classic_name,
	/* name_link */         NULL,
	/* number */            NULL,
	/* pointer_link */      NULL,
	/* progressbar */       NULL,
	/* pulldown */          NULL,
	/* radio */             NULL,
	/* regular */           NULL,
	/* rgb_picker */        NULL,
	/* scroll */            NULL,
	/* slider */            NULL,
	/* swatch */            NULL,
	/* toggle */            NULL,
	/* tooltip */           NULL,
	/* unitvec */           NULL,
};

