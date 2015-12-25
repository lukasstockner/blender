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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, Julian Eisel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/widgets/widget_library/dial_widget.cc
 *  \ingroup wm
 *
 * \name Dial Widget
 *
 * 3D Widget
 *
 * \brief Circle shaped widget for circular interaction. Currently no own handling, use with operator only.
 */

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_math.h"

#include "DNA_view3d_types.h"

#include "ED_screen.h"

#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "wm.h"

#include "../wm_widget.h"
#include "widget_geometry.h"


/* to use custom dials exported to dial_widget.c */
//#define WIDGET_USE_CUSTOM_DIAS

#ifdef WIDGET_USE_CUSTOM_DIAS
WidgetDrawInfo dial_draw_info = {0};
#endif

typedef struct DialWidget {
	wmWidget widget;
	int style;
	float direction[3];
} DialWidget;


static void dial_draw_geom(const DialWidget *dial, const bool select)
{
#ifdef WIDGET_USE_CUSTOM_DIAS
	widget_draw_intern(&dial_draw_info, select);
#else
	GLUquadricObj *qobj = gluNewQuadric();
	const float width = 1.0f;
	const int resol = 32;

	glLineWidth(dial->widget.line_width);
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE);
	gluDisk(qobj, 0.0, width, resol, 1);
	glLineWidth(1.0);

	UNUSED_VARS(select);
#endif
}

static void dial_draw_intern(DialWidget *dial, const bool select, const bool highlight, const float scale)
{
	float rot[3][3];
	float mat[4][4];
	const float up[3] = {0.0f, 0.0f, 1.0f};

	rotation_between_vecs_to_mat3(rot, up, dial->direction);
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], dial->widget.origin);
	mul_mat3_m4_fl(mat, scale);

	glPushMatrix();
	glMultMatrixf(mat);

	if (highlight)
		glColor4fv(dial->widget.col_hi);
	else
		glColor4fv(dial->widget.col);

	glTranslate3fv(dial->widget.offset);
	dial_draw_geom(dial, select);

	glPopMatrix();

}

static void widget_dial_render_3d_intersect(const bContext *C, wmWidget *widget, int selectionbase)
{
	DialWidget *dial = (DialWidget *)widget;

	/* enable clipping if needed */
	if (dial->style == WIDGET_DIAL_STYLE_RING_CLIPPED) {
		ARegion *ar = CTX_wm_region(C);
		RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
		double plane[4];

		copy_v3db_v3fl(plane, rv3d->viewinv[2]);
		plane[3] = -dot_v3v3(rv3d->viewinv[2], widget->origin);
		glClipPlane(GL_CLIP_PLANE0, plane);
		glEnable(GL_CLIP_PLANE0);
	}

	GPU_select_load_id(selectionbase);
	dial_draw_intern(dial, true, false, dial->widget.scale);

	if (dial->style == WIDGET_DIAL_STYLE_RING_CLIPPED) {
		glDisable(GL_CLIP_PLANE0);
	}
}

static void widget_dial_draw(const bContext *C, wmWidget *widget)
{
	DialWidget *dial = (DialWidget *)widget;

	/* enable clipping if needed */
	if (dial->style == WIDGET_DIAL_STYLE_RING_CLIPPED) {
		double plane[4];
		ARegion *ar = CTX_wm_region(C);
		RegionView3D *rv3d = (RegionView3D *)ar->regiondata;

		copy_v3db_v3fl(plane, rv3d->viewinv[2]);
		plane[3] = -dot_v3v3(rv3d->viewinv[2], widget->origin);
		glClipPlane(GL_CLIP_PLANE0, plane);
		glEnable(GL_CLIP_PLANE0);
	}

	glEnable(GL_BLEND);
	dial_draw_intern(dial, false, (widget->flag & WM_WIDGET_HIGHLIGHT) != 0, widget->scale);
	glDisable(GL_BLEND);

	if (dial->style == WIDGET_DIAL_STYLE_RING_CLIPPED) {
		glDisable(GL_CLIP_PLANE0);
	}
}

/** \name Dial Widget API
 *
 * \{ */

wmWidget *WIDGET_dial_new(wmWidgetGroup *wgroup, const char *name, const int style)
{
	DialWidget *dial = (DialWidget *)MEM_callocN(sizeof(DialWidget), name);
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

#ifdef WIDGET_USE_CUSTOM_DIAS
	if (!dial_draw_info.init) {
		dial_draw_info.nverts = _WIDGET_nverts_dial,
		dial_draw_info.ntris = _WIDGET_ntris_dial,
		dial_draw_info.verts = _WIDGET_verts_dial,
		dial_draw_info.normals = _WIDGET_normals_dial,
		dial_draw_info.indices = _WIDGET_indices_dial,
		dial_draw_info.init = true;
	}
#endif

	dial->widget.draw = widget_dial_draw;
	dial->widget.intersect = NULL;
	dial->widget.render_3d_intersection = widget_dial_render_3d_intersect;
	dial->widget.flag |= WM_WIDGET_SCALE_3D;

	dial->style = style;

	/* defaults */
	copy_v3_v3(dial->direction, dir_default);

	wm_widget_register(wgroup, &dial->widget, name);

	return (wmWidget *)dial;
}

/**
 * Define up-direction of the dial widget
 */
void WIDGET_dial_set_up_vector(wmWidget *widget, const float direction[3])
{
	DialWidget *dial = (DialWidget *)widget;

	copy_v3_v3(dial->direction, direction);
	normalize_v3(dial->direction);
}

/** \} */ // Dial Widget API


/* -------------------------------------------------------------------- */

void fix_linking_widget_dial(void)
{
	(void)0;
}

