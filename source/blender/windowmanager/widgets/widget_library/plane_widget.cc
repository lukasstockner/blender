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
 * Contributor(s): Julian Eisel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/widgets/widget_library/plane_widget.cc
 *  \ingroup wm
 *
 * \name Plane Widget
 *
 * 3D Widget
 *
 * \brief Flat and rectangular shaped widget for planar interaction. Currently no own handling, use with operator only.
 */

#include <new>

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_math.h"

#include "DNA_view3d_types.h"

#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "wm.h"

#include "../wm_widget.h"

/* PlaneWidget->flag */
#define PLANE_UP_VECTOR_SET 1

typedef struct PlaneWidget: wmWidget {
	PlaneWidget(wmWidgetGroup *wgroup, const char *name);

	float direction[3];
	float up[3];
	int plane_flag;
} PlaneWidget;


PlaneWidget::PlaneWidget(wmWidgetGroup *wgroup, const char *name)
    : wmWidget(wgroup, name)
{
	
}

static void widget_plane_draw_geom(const float col_inner[4], const float col_outer[4])
{
	static float vec[4][3] = {
		{-1, -1, 0},
		{ 1, -1, 0},
		{ 1,  1, 0},
		{-1,  1, 0},
	};

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vec);
	glColor4fv(col_inner);
	glDrawArrays(GL_QUADS, 0, ARRAY_SIZE(vec));
	glColor4fv(col_outer);
	glDrawArrays(GL_LINE_LOOP, 0, ARRAY_SIZE(vec));
	glDisableClientState(GL_VERTEX_ARRAY);
}

static void widget_plane_draw_intern(PlaneWidget *plane, const bool UNUSED(select), const bool highlight)
{
	float col_inner[4], col_outer[4];
	float rot[3][3];
	float mat[4][4];

	if (plane->plane_flag & PLANE_UP_VECTOR_SET) {
		copy_v3_v3(rot[2], plane->direction);
		copy_v3_v3(rot[1], plane->up);
		cross_v3_v3v3(rot[0], plane->up, plane->direction);
	}
	else {
		const float up[3] = {0.0f, 0.0f, 1.0f};
		rotation_between_vecs_to_mat3(rot, up, plane->direction);
	}

	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], plane->origin);
	mul_mat3_m4_fl(mat, plane->scale);

	glPushMatrix();
	glMultMatrixf(mat);

	if (highlight && (plane->flag & WM_WIDGET_DRAW_HOVER) == 0) {
		copy_v4_v4(col_inner, plane->col_hi);
		copy_v4_v4(col_outer, plane->col_hi);
	}
	else {
		copy_v4_v4(col_inner, plane->col);
		copy_v4_v4(col_outer, plane->col);
	}
	col_inner[3] *= 0.5f;

	glEnable(GL_BLEND);
	glTranslate3fv(plane->offset);
	widget_plane_draw_geom(col_inner, col_outer);
	glDisable(GL_BLEND);

	glPopMatrix();
}

static void widget_plane_render_3d_intersect(const bContext *UNUSED(C), wmWidget *widget, int selectionbase)
{
	GPU_select_load_id(selectionbase);
	widget_plane_draw_intern((PlaneWidget *)widget, true, false);
}

static void widget_plane_draw(const bContext *UNUSED(C), wmWidget *widget)
{
	widget_plane_draw_intern((PlaneWidget *)widget, false, (widget->flag & WM_WIDGET_HIGHLIGHT));
}

/** \name Plane Widget API
 *
 * \{ */

wmWidget *WIDGET_plane_new(wmWidgetGroup *wgroup, const char *name, const int UNUSED(style))
{
	PlaneWidget *plane = OBJECT_GUARDED_NEW_CALLOC(PlaneWidget, wgroup, name);
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

	plane->draw                    = widget_plane_draw;
	plane->intersect               = NULL;
	plane->render_3d_intersection  = widget_plane_render_3d_intersect;
	plane->flag                   |= WM_WIDGET_SCALE_3D;

	/* defaults */
	copy_v3_v3(plane->direction, dir_default);

	return (wmWidget *)plane;
}

/**
 * Define direction the plane will point towards
 */
void WIDGET_plane_set_direction(wmWidget *widget, const float direction[3])
{
	PlaneWidget *plane = (PlaneWidget *)widget;

	copy_v3_v3(plane->direction, direction);
	normalize_v3(plane->direction);
}

/**
 * Define up-direction of the plane widget
 */
void WIDGET_plane_set_up_vector(wmWidget *widget, const float direction[3])
{
	PlaneWidget *plane = (PlaneWidget *)widget;

	if (direction) {
		copy_v3_v3(plane->up, direction);
		normalize_v3(plane->up);
		plane->plane_flag |= PLANE_UP_VECTOR_SET;
	}
	else {
		plane->plane_flag &= ~PLANE_UP_VECTOR_SET;
	}
}

/** \} */ // Plane Widget API


/* -------------------------------------------------------------------- */

void fix_linking_widget_plane(void)
{
	(void)0;
}

