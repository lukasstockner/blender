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

#ifndef __WIDGETS_DRAW_INTERN_H__
#define __WIDGETS_DRAW_INTERN_H__

/** \file blender/editors/interface/widgets/widgets_draw/widgets_draw_intern.h
 *  \ingroup edinterface
 * 
 * \brief Blender widget drawing module
 */

/* Struct Declarations */

struct uiBut;
struct uiFontStyle;
struct uiWidgetType;
struct rcti;

/* ************** widget base functions ************** */
/**
 * - in: roundbox codes for corner types and radius
 * - return: array of `[size][2][x, y]` points, the edges of the roundbox, + UV coords
 *
 * - draw black box with alpha 0 on exact button boundbox
 * - for every AA step:
 *    - draw the inner part for a round filled box, with color blend codes or texture coords
 *    - draw outline in outline color
 *    - draw outer part, bottom half, extruded 1 pixel to bottom, for emboss shadow
 *    - draw extra decorations
 * - draw background color box with alpha 1 on exact button boundbox
 */

/* fill this struct with polygon info to draw AA'ed */
/* it has outline, back, and two optional tria meshes */

/* max as used by round_box__edges */
#define WIDGET_CURVE_RESOLU 9
#define WIDGET_SIZE_MAX (WIDGET_CURVE_RESOLU * 4)

typedef struct uiWidgetTrias {
	unsigned int tot;

	float vec[16][2];
	const unsigned int (*index)[3];
} uiWidgetTrias;

/* XXX rename to uiWidgetDrawBase or uiWidgetDrawData */
typedef struct uiWidgetBase {
	int totvert, halfwayvert;
	float outer_v[WIDGET_SIZE_MAX][2];
	float inner_v[WIDGET_SIZE_MAX][2];
	float inner_uv[WIDGET_SIZE_MAX][2];

	bool draw_inner, draw_outline, draw_emboss, draw_shadedir;

	uiWidgetTrias tria1;
	uiWidgetTrias tria2;
} uiWidgetBase;

/* widgets_draw.c - shared low-level drawing functions */
void widgetbase_init(uiWidgetBase *wtb);

void round_box_edges(uiWidgetBase *wt, int roundboxalign, const rcti *rect, float rad); /* XXX rename to widgetbase_roundboxedges_set */
void round_box__edges(uiWidgetBase *wt, int roundboxalign, const rcti *rect, float rad, float radi);

void widget_draw_tria_ex( /* XXX tmp, could be static */
        uiWidgetTrias *tria, const rcti *rect, float triasize, char where,
        /* input data */
        const float verts[][2], const int verts_tot,
        const unsigned int tris[][3], const int tris_tot);
void widget_num_tria(uiWidgetTrias *tria, const rcti *rect, float triasize, char where);
void widget_menu_trias(uiWidgetTrias *tria, const rcti *rect);
void widget_check_trias(uiWidgetTrias *tria, const rcti *rect);

void widget_softshadow(const rcti *rect, int roundboxalign, const float radin);

void widgetbase_draw(uiWidgetBase *wtb, struct uiWidgetColors *wcol);

#endif  /* __WIDGETS_DRAW_INTERN_H__ */

