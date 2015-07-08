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

/** \file blender/editors/interface/widgets/widgets_draw/widgets_draw.c
 *  \ingroup edinterface
 * 
 * Shared low-level widget drawing functions
 */

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "DNA_screen_types.h"

#include "UI_interface.h"
#include "UI_resources.h"


#include "widgets_draw_intern.h" /* own include */



static const float cornervec[WIDGET_CURVE_RESOLU][2] = {
	{0.0, 0.0}, {0.195, 0.02}, {0.383, 0.067},
	{0.55, 0.169}, {0.707, 0.293}, {0.831, 0.45},
	{0.924, 0.617}, {0.98, 0.805}, {1.0, 1.0}
};

#define WIDGET_AA_JITTER 8
static const float jit[WIDGET_AA_JITTER][2] = {
	{ 0.468813, -0.481430}, {-0.155755, -0.352820},
	{ 0.219306, -0.238501}, {-0.393286, -0.110949},
	{-0.024699,  0.013908}, { 0.343805,  0.147431},
	{-0.272855,  0.269918}, { 0.095909,  0.388710}
};

/* ************************************************* */

void widgetbase_init(uiWidgetBase *wtb)
{
	wtb->totvert = wtb->halfwayvert = 0;
	wtb->tria1.tot = 0;
	wtb->tria2.tot = 0;

	wtb->draw_inner = true;
	wtb->draw_outline = true;
	wtb->draw_emboss = true;
	wtb->draw_shadedir = true;
}

/* this call has 1 extra arg to allow mask outline */
void round_box__edges(uiWidgetBase *wt, int roundboxalign, const rcti *rect, float rad, float radi)
{
	float vec[WIDGET_CURVE_RESOLU][2], veci[WIDGET_CURVE_RESOLU][2];
	float minx = rect->xmin, miny = rect->ymin, maxx = rect->xmax, maxy = rect->ymax;
	float minxi = minx + U.pixelsize; /* boundbox inner */
	float maxxi = maxx - U.pixelsize;
	float minyi = miny + U.pixelsize;
	float maxyi = maxy - U.pixelsize;
	float facxi = (maxxi != minxi) ? 1.0f / (maxxi - minxi) : 0.0f; /* for uv, can divide by zero */
	float facyi = (maxyi != minyi) ? 1.0f / (maxyi - minyi) : 0.0f;
	int a, tot = 0, minsize;
	const int hnum = ((roundboxalign & (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT)) == (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT) ||
	                  (roundboxalign & (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT)) == (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT)) ? 1 : 2;
	const int vnum = ((roundboxalign & (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT)) == (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT) ||
	                  (roundboxalign & (UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT)) == (UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT)) ? 1 : 2;

	minsize = min_ii(BLI_rcti_size_x(rect) * hnum,
	                 BLI_rcti_size_y(rect) * vnum);

	if (2.0f * rad > minsize)
		rad = 0.5f * minsize;

	if (2.0f * (radi + 1.0f) > minsize)
		radi = 0.5f * minsize - U.pixelsize;

	/* mult */
	for (a = 0; a < WIDGET_CURVE_RESOLU; a++) {
		veci[a][0] = radi * cornervec[a][0];
		veci[a][1] = radi * cornervec[a][1];
		vec[a][0] = rad * cornervec[a][0];
		vec[a][1] = rad * cornervec[a][1];
	}

	/* corner left-bottom */
	if (roundboxalign & UI_CNR_BOTTOM_LEFT) {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			wt->inner_v[tot][0] = minxi + veci[a][1];
			wt->inner_v[tot][1] = minyi + radi - veci[a][0];

			wt->outer_v[tot][0] = minx + vec[a][1];
			wt->outer_v[tot][1] = miny + rad - vec[a][0];

			wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0] = minxi;
		wt->inner_v[tot][1] = minyi;

		wt->outer_v[tot][0] = minx;
		wt->outer_v[tot][1] = miny;

		wt->inner_uv[tot][0] = 0.0f;
		wt->inner_uv[tot][1] = 0.0f;

		tot++;
	}

	/* corner right-bottom */
	if (roundboxalign & UI_CNR_BOTTOM_RIGHT) {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			wt->inner_v[tot][0] = maxxi - radi + veci[a][0];
			wt->inner_v[tot][1] = minyi + veci[a][1];

			wt->outer_v[tot][0] = maxx - rad + vec[a][0];
			wt->outer_v[tot][1] = miny + vec[a][1];

			wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0] = maxxi;
		wt->inner_v[tot][1] = minyi;

		wt->outer_v[tot][0] = maxx;
		wt->outer_v[tot][1] = miny;

		wt->inner_uv[tot][0] = 1.0f;
		wt->inner_uv[tot][1] = 0.0f;

		tot++;
	}

	wt->halfwayvert = tot;

	/* corner right-top */
	if (roundboxalign & UI_CNR_TOP_RIGHT) {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			wt->inner_v[tot][0] = maxxi - veci[a][1];
			wt->inner_v[tot][1] = maxyi - radi + veci[a][0];

			wt->outer_v[tot][0] = maxx - vec[a][1];
			wt->outer_v[tot][1] = maxy - rad + vec[a][0];

			wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0] = maxxi;
		wt->inner_v[tot][1] = maxyi;

		wt->outer_v[tot][0] = maxx;
		wt->outer_v[tot][1] = maxy;

		wt->inner_uv[tot][0] = 1.0f;
		wt->inner_uv[tot][1] = 1.0f;

		tot++;
	}

	/* corner left-top */
	if (roundboxalign & UI_CNR_TOP_LEFT) {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			wt->inner_v[tot][0] = minxi + radi - veci[a][0];
			wt->inner_v[tot][1] = maxyi - veci[a][1];

			wt->outer_v[tot][0] = minx + rad - vec[a][0];
			wt->outer_v[tot][1] = maxy - vec[a][1];

			wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0] = minxi;
		wt->inner_v[tot][1] = maxyi;

		wt->outer_v[tot][0] = minx;
		wt->outer_v[tot][1] = maxy;

		wt->inner_uv[tot][0] = 0.0f;
		wt->inner_uv[tot][1] = 1.0f;

		tot++;
	}

	BLI_assert(tot <= WIDGET_SIZE_MAX);

	wt->totvert = tot;
}

void round_box_edges(uiWidgetBase *wt, int roundboxalign, const rcti *rect, float rad)
{
	round_box__edges(wt, roundboxalign, rect, rad, rad - U.pixelsize);
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

static void widget_verts_to_triangle_strip(uiWidgetBase *wtb, const int totvert, float triangle_strip[WIDGET_SIZE_MAX * 2 + 2][2])
{
	int a;
	for (a = 0; a < totvert; a++) {
		copy_v2_v2(triangle_strip[a * 2], wtb->outer_v[a]);
		copy_v2_v2(triangle_strip[a * 2 + 1], wtb->inner_v[a]);
	}
	copy_v2_v2(triangle_strip[a * 2], wtb->outer_v[0]);
	copy_v2_v2(triangle_strip[a * 2 + 1], wtb->inner_v[0]);
}

static void widget_verts_to_triangle_strip_open(uiWidgetBase *wtb, const int totvert, float triangle_strip[WIDGET_SIZE_MAX * 2][2])
{
	int a;
	for (a = 0; a < totvert; a++) {
		triangle_strip[a * 2][0] = wtb->outer_v[a][0];
		triangle_strip[a * 2][1] = wtb->outer_v[a][1];
		triangle_strip[a * 2 + 1][0] = wtb->outer_v[a][0];
		triangle_strip[a * 2 + 1][1] = wtb->outer_v[a][1] - 1.0f;
	}
}

static void widget_trias_draw(uiWidgetTrias *tria)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, tria->vec);
	glDrawElements(GL_TRIANGLES, tria->tot * 3, GL_UNSIGNED_INT, tria->index);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void widgetbase_draw(uiWidgetBase *wtb, uiWidgetColors *wcol)
{
	int j, a;

	glEnable(GL_BLEND);

	/* backdrop non AA */
	if (wtb->draw_inner) {
		if (wcol->shaded == 0) {
			if (wcol->alpha_check) {
				float inner_v_half[WIDGET_SIZE_MAX][2];
				float x_mid = 0.0f; /* used for dumb clamping of values */

				/* dark checkers */
				glColor4ub(UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, 255);
				glEnableClientState(GL_VERTEX_ARRAY);
				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);

				/* light checkers */
				glEnable(GL_POLYGON_STIPPLE);
				glColor4ub(UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, 255);
				glPolygonStipple(stipple_checker_8px);

				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);

				glDisable(GL_POLYGON_STIPPLE);

				/* alpha fill */
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				glColor4ubv((unsigned char *)wcol->inner);

				for (a = 0; a < wtb->totvert; a++) {
					x_mid += wtb->inner_v[a][0];
				}
				x_mid /= wtb->totvert;

				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);

				/* 1/2 solid color */
				glColor4ub(wcol->inner[0], wcol->inner[1], wcol->inner[2], 255);

				for (a = 0; a < wtb->totvert; a++) {
					inner_v_half[a][0] = MIN2(wtb->inner_v[a][0], x_mid);
					inner_v_half[a][1] = wtb->inner_v[a][1];
				}

				glVertexPointer(2, GL_FLOAT, 0, inner_v_half);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);
				glDisableClientState(GL_VERTEX_ARRAY);
			}
			else {
				/* simple fill */
				glColor4ubv((unsigned char *)wcol->inner);

				glEnableClientState(GL_VERTEX_ARRAY);
				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);
				glDisableClientState(GL_VERTEX_ARRAY);
			}
		}
		else {
			char col1[4], col2[4];
			unsigned char col_array[WIDGET_SIZE_MAX * 4];
			unsigned char *col_pt = col_array;

			shadecolors4(col1, col2, wcol->inner, wcol->shadetop, wcol->shadedown);

			glShadeModel(GL_SMOOTH);
			for (a = 0; a < wtb->totvert; a++, col_pt += 4) {
				round_box_shade_col4_r(col_pt, col1, col2, wtb->inner_uv[a][wtb->draw_shadedir ? 1 : 0]);
			}

			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, col_array);
			glDrawArrays(GL_POLYGON, 0, wtb->totvert);
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);

			glShadeModel(GL_FLAT);
		}
	}

	/* for each AA step */
	if (wtb->draw_outline) {
		float triangle_strip[WIDGET_SIZE_MAX * 2 + 2][2]; /* + 2 because the last pair is wrapped */
		float triangle_strip_emboss[WIDGET_SIZE_MAX * 2][2]; /* only for emboss */

		const unsigned char tcol[4] = {wcol->outline[0],
		                               wcol->outline[1],
		                               wcol->outline[2],
		                               wcol->outline[3] / WIDGET_AA_JITTER};

		widget_verts_to_triangle_strip(wtb, wtb->totvert, triangle_strip);

		if (wtb->draw_emboss) {
			widget_verts_to_triangle_strip_open(wtb, wtb->halfwayvert, triangle_strip_emboss);
		}

		glEnableClientState(GL_VERTEX_ARRAY);

		for (j = 0; j < WIDGET_AA_JITTER; j++) {
			unsigned char emboss[4];

			glTranslatef(jit[j][0], jit[j][1], 0.0f);

			/* outline */
			glColor4ubv(tcol);

			glVertexPointer(2, GL_FLOAT, 0, triangle_strip);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, wtb->totvert * 2 + 2);

			/* emboss bottom shadow */
			if (wtb->draw_emboss) {
				UI_GetThemeColor4ubv(TH_WIDGET_EMBOSS, emboss);

				if (emboss[3]) {
					glColor4ubv(emboss);
					glVertexPointer(2, GL_FLOAT, 0, triangle_strip_emboss);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, wtb->halfwayvert * 2);
				}
			}

			glTranslatef(-jit[j][0], -jit[j][1], 0.0f);
		}

		glDisableClientState(GL_VERTEX_ARRAY);
	}

	/* decoration */
	if (wtb->tria1.tot || wtb->tria2.tot) {
		const unsigned char tcol[4] = {wcol->item[0],
		                               wcol->item[1],
		                               wcol->item[2],
		                               (unsigned char)((float)wcol->item[3] / WIDGET_AA_JITTER)};

		/* for each AA step */
		for (j = 0; j < WIDGET_AA_JITTER; j++) {
			glTranslatef(jit[j][0], jit[j][1], 0.0f);

			if (wtb->tria1.tot) {
				glColor4ubv(tcol);
				widget_trias_draw(&wtb->tria1);
			}
			if (wtb->tria2.tot) {
				glColor4ubv(tcol);
				widget_trias_draw(&wtb->tria2);
			}

			glTranslatef(-jit[j][0], -jit[j][1], 0.0f);
		}
	}

	glDisable(GL_BLEND);
}
