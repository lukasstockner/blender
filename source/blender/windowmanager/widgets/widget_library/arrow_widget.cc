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

/** \file blender/windowmanager/widgets/widget_library/arrow_widget.cc
 *  \ingroup wm
 *
 * \name Arrow Widget
 *
 * 3D Widget
 *
 * \brief Simple arrow widget which is dragged into a certain direction. The arrow head can have varying shapes, e.g.
 *        cone, box, etc.
 */

#include <new>

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_math.h"

#include "DNA_view3d_types.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_types.h"
#include "WM_api.h"
#include "wm.h"

#include "../wm_widget.h"
#include "widget_geometry.h"


/* to use custom arrows exported to arrow_widget.c */
//#define WIDGET_USE_CUSTOM_ARROWS


#ifdef WIDGET_USE_CUSTOM_ARROWS
WidgetDrawInfo arrow_head_draw_info = {0};
#endif
WidgetDrawInfo cube_draw_info = {0};

/* ArrowWidget->flag */
enum {
	ARROW_UP_VECTOR_SET    = (1 << 0),
	ARROW_CUSTOM_RANGE_SET = (1 << 1),
};

typedef struct ArrowWidget: wmWidget {
	ArrowWidget(wmWidgetGroup *wgroup, const char *name);

	int style;
	int arrow_flag;

	float len;          /* arrow line length */
	float direction[3];
	float up[3];
	float aspect[2];    /* cone style only */

	float range_fac;      /* factor for arrow min/max distance */
	float arrow_offset;
	/* property range and minimum for constrained arrows */
	float range, min;
} ArrowWidget;

typedef struct ArrowInteraction {
	float orig_value; /* initial property value */
	float orig_origin[3];
	float orig_mouse[2];
	float orig_offset;
	float orig_scale;

	/* offset of last handling step */
	float prev_offset;
	/* Total offset added by precision tweaking.
	 * Needed to allow toggling precision on/off without causing jumps */
	float precision_offset;
} ArrowInteraction;

/* factor for precision tweaking */
#define ARROW_PRECISION_FAC 0.05f


ArrowWidget::ArrowWidget(wmWidgetGroup *wgroup, const char *name)
    : wmWidget(wgroup, name)
{
	
}

static void widget_arrow_get_final_pos(wmWidget *widget, float r_pos[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	mul_v3_v3fl(r_pos, arrow->direction, arrow->arrow_offset);
	add_v3_v3(r_pos, arrow->origin);
}

static void arrow_draw_geom(const ArrowWidget *arrow, const bool select)
{
	if (arrow->style & WIDGET_ARROW_STYLE_CROSS) {
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_LIGHTING);
		glBegin(GL_LINES);
		glVertex2f(-1.0, 0.f);
		glVertex2f(1.0, 0.f);
		glVertex2f(0.f, -1.0);
		glVertex2f(0.f, 1.0);
		glEnd();

		glPopAttrib();
	}
	else if (arrow->style & WIDGET_ARROW_STYLE_CONE) {
		const float unitx = arrow->aspect[0];
		const float unity = arrow->aspect[1];
		const float vec[4][3] = {
			{-unitx, -unity, 0},
			{ unitx, -unity, 0},
			{ unitx,  unity, 0},
			{-unitx,  unity, 0},
		};

		glLineWidth(arrow->line_width);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, vec);
		glDrawArrays(GL_LINE_LOOP, 0, ARRAY_SIZE(vec));
		glDisableClientState(GL_VERTEX_ARRAY);
		glLineWidth(1.0);
	}
	else {
#ifdef WIDGET_USE_CUSTOM_ARROWS
		widget_draw_intern(&arrow_head_draw_info, select);
#else
		const float vec[2][3] = {
			{0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, arrow->len},
		};

		glLineWidth(arrow->line_width);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, vec);
		glDrawArrays(GL_LINE_STRIP, 0, ARRAY_SIZE(vec));
		glDisableClientState(GL_VERTEX_ARRAY);
		glLineWidth(1.0);


		/* *** draw arrow head *** */

		glPushMatrix();

		if (arrow->style & WIDGET_ARROW_STYLE_BOX) {
			const float size = 0.05f;

			/* translate to line end with some extra offset so box starts exactly where line ends */
			glTranslatef(0.0f, 0.0f, arrow->len + size);
			/* scale down to box size */
			glScalef(size, size, size);

			/* draw cube */
			widget_draw(&cube_draw_info, select);
		}
		else {
			GLUquadricObj *qobj = gluNewQuadric();
			const float len = 0.25f;
			const float width = 0.06f;
			const bool use_lighting = select == false && ((U.tw_flag & V3D_SHADED_WIDGETS) != 0);

			/* translate to line end */
			glTranslatef(0.0f, 0.0f, arrow->len);

			if (use_lighting) {
				glShadeModel(GL_SMOOTH);
			}

			gluQuadricDrawStyle(qobj, GLU_FILL);
			gluQuadricOrientation(qobj, GLU_INSIDE);
			gluDisk(qobj, 0.0, width, 8, 1);
			gluQuadricOrientation(qobj, GLU_OUTSIDE);
			gluCylinder(qobj, width, 0.0, len, 8, 1);

			if (use_lighting) {
				glShadeModel(GL_FLAT);
			}
		}

		glPopMatrix();
#endif
	}
}

static void arrow_draw_intern(ArrowWidget *arrow, const bool select, const bool highlight)
{
	const float up[3] = {0.0f, 0.0f, 1.0f};
	float rot[3][3];
	float mat[4][4];
	float final_pos[3];

	widget_arrow_get_final_pos(arrow, final_pos);

	if (arrow->arrow_flag & ARROW_UP_VECTOR_SET) {
		copy_v3_v3(rot[2], arrow->direction);
		copy_v3_v3(rot[1], arrow->up);
		cross_v3_v3v3(rot[0], arrow->up, arrow->direction);
	}
	else {
		rotation_between_vecs_to_mat3(rot, up, arrow->direction);
	}
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], final_pos);
	mul_mat3_m4_fl(mat, arrow->scale);

	glPushMatrix();
	glMultMatrixf(mat);

	if (highlight && !(arrow->flag & WM_WIDGET_DRAW_HOVER)) {
		glColor4fv(arrow->col_hi);
	}
	else {
		glColor4fv(arrow->col);
	}

	glEnable(GL_BLEND);
	glTranslate3fv(arrow->offset);
	arrow_draw_geom(arrow, select);
	glDisable(GL_BLEND);

	glPopMatrix();

	if (arrow->interaction_data) {
		ArrowInteraction *data = (ArrowInteraction *)arrow->interaction_data;

		copy_m4_m3(mat, rot);
		copy_v3_v3(mat[3], data->orig_origin);
		mul_mat3_m4_fl(mat, data->orig_scale);

		glPushMatrix();
		glMultMatrixf(mat);

		glEnable(GL_BLEND);
		glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
		glTranslate3fv(arrow->offset);
		arrow_draw_geom(arrow, select);
		glDisable(GL_BLEND);

		glPopMatrix();
	}
}

static void widget_arrow_render_3d_intersect(const bContext *UNUSED(C), wmWidget *widget, int selectionbase)
{
	GPU_select_load_id(selectionbase);
	arrow_draw_intern((ArrowWidget *)widget, true, false);
}

static void widget_arrow_draw(const bContext *UNUSED(C), wmWidget *widget)
{
	arrow_draw_intern((ArrowWidget *)widget, false, (widget->flag & WM_WIDGET_HIGHLIGHT) != 0);
}

/**
 * Calculate arrow offset independent from prop min value,
 * meaning the range will not be offset by min value first.
 */
#define USE_ABS_HANDLE_RANGE

static int widget_arrow_handler(bContext *C, const wmEvent *event, wmWidget *widget, const int flag)
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	ArrowInteraction *data = (ArrowInteraction *)widget->interaction_data;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = (RegionView3D *)ar->regiondata;

	float orig_origin[4];
	float viewvec[3], tangent[3], plane[3];
	float offset[4];
	float m_diff[2];
	float dir_2d[2], dir2d_final[2];
	float facdir = 1.0f;
	bool use_vertical = false;


	copy_v3_v3(orig_origin, data->orig_origin);
	orig_origin[3] = 1.0f;
	add_v3_v3v3(offset, orig_origin, arrow->direction);
	offset[3] = 1.0f;

	/* calculate view vector */
	if (rv3d->is_persp) {
		sub_v3_v3v3(viewvec, orig_origin, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(viewvec, rv3d->viewinv[2]);
	}
	normalize_v3(viewvec);

	/* first determine if view vector is really close to the direction. If it is, we use
	 * vertical movement to determine offset, just like transform system does */
	if (RAD2DEG(acos(dot_v3v3(viewvec, arrow->direction))) > 5.0f) {
		/* multiply to projection space */
		mul_m4_v4(rv3d->persmat, orig_origin);
		mul_v4_fl(orig_origin, 1.0f/orig_origin[3]);
		mul_m4_v4(rv3d->persmat, offset);
		mul_v4_fl(offset, 1.0f/offset[3]);

		sub_v2_v2v2(dir_2d, offset, orig_origin);
		dir_2d[0] *= ar->winx;
		dir_2d[1] *= ar->winy;
		normalize_v2(dir_2d);
	}
	else {
		dir_2d[0] = 0.0f;
		dir_2d[1] = 1.0f;
		use_vertical = true;
	}

	/* find mouse difference */
	m_diff[0] = event->mval[0] - data->orig_mouse[0];
	m_diff[1] = event->mval[1] - data->orig_mouse[1];

	/* project the displacement on the screen space arrow direction */
	project_v2_v2v2(dir2d_final, m_diff, dir_2d);

	float zfac = ED_view3d_calc_zfac(rv3d, orig_origin, NULL);
	ED_view3d_win_to_delta(ar, dir2d_final, offset, zfac);

	add_v3_v3v3(orig_origin, offset, data->orig_origin);

	/* calculate view vector for the new position */
	if (rv3d->is_persp) {
		sub_v3_v3v3(viewvec, orig_origin, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(viewvec, rv3d->viewinv[2]);
	}

	normalize_v3(viewvec);
	if (!use_vertical) {
		float fac;
		/* now find a plane parallel to the view vector so we can intersect with the arrow direction */
		cross_v3_v3v3(tangent, viewvec, offset);
		cross_v3_v3v3(plane, tangent, viewvec);
		fac = dot_v3v3(plane, offset) / dot_v3v3(arrow->direction, plane);

		facdir = (fac < 0.0) ? -1.0 : 1.0;
		mul_v3_v3fl(offset, arrow->direction, fac);
	}
	else {
		facdir = (m_diff[1] < 0.0) ? -1.0 : 1.0;
	}


	const float ofs_new = facdir * len_v3(offset);

	/* set the property for the operator and call its modal function */
	if (widget->props[ARROW_SLOT_OFFSET_WORLD_SPACE]) {
		float max = arrow->min + arrow->range;
		float value;

		if (flag & WM_WIDGET_TWEAK_PRECISE) {
			/* add delta offset of this step to total precision_offset */
			data->precision_offset += ofs_new - data->prev_offset;
		}
		data->prev_offset = ofs_new;

		value = data->orig_offset + ofs_new - data->precision_offset * (1.0f - ARROW_PRECISION_FAC);

		if (arrow->style & WIDGET_ARROW_STYLE_CONSTRAINED) {
			if (arrow->style & WIDGET_ARROW_STYLE_INVERTED)
				value = max - (value * arrow->range / arrow->range_fac);
			else
#ifdef USE_ABS_HANDLE_RANGE
				value = value * arrow->range / arrow->range_fac;
#else
				value = arrow->min + (value * arrow->range / arrow->range_fac);
#endif
		}

		/* clamp to custom range */
		if (arrow->arrow_flag & ARROW_CUSTOM_RANGE_SET) {
			CLAMP(value, arrow->min, max);
		}


		PointerRNA ptr = widget->ptr[ARROW_SLOT_OFFSET_WORLD_SPACE];
		PropertyRNA *prop = widget->props[ARROW_SLOT_OFFSET_WORLD_SPACE];

		RNA_property_float_set(&ptr, prop, value);
		RNA_property_update(C, &ptr, prop);
		/* get clamped value */
		value = RNA_property_float_get(&ptr, prop);

		/* accounts for clamping properly */
		if (arrow->style & WIDGET_ARROW_STYLE_CONSTRAINED) {
			if (arrow->style & WIDGET_ARROW_STYLE_INVERTED)
				arrow->arrow_offset = arrow->range_fac * (max - value) / arrow->range;
			else
#ifdef USE_ABS_HANDLE_RANGE
				arrow->arrow_offset = arrow->range_fac * (value / arrow->range);
#else
				arrow->offset = arrow->range_fac * ((value - arrow->min) / arrow->range);
#endif
		}
		else
			arrow->arrow_offset = value;
	}
	else {
		arrow->arrow_offset = ofs_new;
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(ar);
	WM_event_add_mousemove(C);

	return OPERATOR_PASS_THROUGH;
}


static int widget_arrow_invoke(bContext *UNUSED(C), const wmEvent *event, wmWidget *widget)
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	ArrowInteraction *data = (ArrowInteraction *)MEM_callocN(sizeof(ArrowInteraction), "arrow_interaction");
	PointerRNA ptr = widget->ptr[ARROW_SLOT_OFFSET_WORLD_SPACE];
	PropertyRNA *prop = widget->props[ARROW_SLOT_OFFSET_WORLD_SPACE];

	if (prop) {
		data->orig_value = RNA_property_float_get(&ptr, prop);
	}

	data->orig_offset = arrow->arrow_offset;

	data->orig_mouse[0] = event->mval[0];
	data->orig_mouse[1] = event->mval[1];

	data->orig_scale = widget->scale;

	widget_arrow_get_final_pos(widget, data->orig_origin);

	widget->interaction_data = data;

	return OPERATOR_RUNNING_MODAL;
}

static void widget_arrow_bind_to_prop(wmWidget *widget, const int UNUSED(slot))
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	PointerRNA ptr = widget->ptr[ARROW_SLOT_OFFSET_WORLD_SPACE];
	PropertyRNA *prop = widget->props[ARROW_SLOT_OFFSET_WORLD_SPACE];

	if (prop) {
		float float_prop = RNA_property_float_get(&ptr, prop);

		if (arrow->style & WIDGET_ARROW_STYLE_CONSTRAINED) {
			float min, max;

			if (arrow->arrow_flag & ARROW_CUSTOM_RANGE_SET) {
				max = arrow->min + arrow->range;
			}
			else {
				float step, precision;
				RNA_property_float_ui_range(&ptr, prop, &min, &max, &step, &precision);
				arrow->range = max - min;
				arrow->min = min;
			}

			if (arrow->style & WIDGET_ARROW_STYLE_INVERTED) {
				arrow->arrow_offset = arrow->range_fac * (max - float_prop) / arrow->range;
			}
			else {
#ifdef USE_ABS_HANDLE_RANGE
				arrow->arrow_offset = arrow->range_fac * (float_prop / arrow->range);
#else
				arrow->offset = arrow->range_fac * ((float_prop - arrow->min) / arrow->range);
#endif
			}
		}
		else {
			/* we'd need to check the property type here but for now assume always float */
			arrow->arrow_offset = float_prop;
		}
	}
	else
		arrow->arrow_offset = 0.0f;
}

static void widget_arrow_cancel(bContext *C, wmWidget *widget)
{
	PointerRNA ptr = widget->ptr[ARROW_SLOT_OFFSET_WORLD_SPACE];
	PropertyRNA *prop = widget->props[ARROW_SLOT_OFFSET_WORLD_SPACE];
	ArrowInteraction *data = (ArrowInteraction *)widget->interaction_data;

	/* reset property */
	RNA_property_float_set(&ptr, prop, data->orig_value);
	RNA_property_update(C, &ptr, prop);
}

/** \name Arrow Widget API
 *
 * \{ */

wmWidget *WIDGET_arrow_new(wmWidgetGroup *wgroup, const char *name, const int style)
{
	int real_style = style;

#ifdef WIDGET_USE_CUSTOM_ARROWS
	if (!arrow_head_draw_info.init) {
		arrow_head_draw_info.nverts  = _WIDGET_nverts_arrow,
		arrow_head_draw_info.ntris   = _WIDGET_ntris_arrow,
		arrow_head_draw_info.verts   = _WIDGET_verts_arrow,
		arrow_head_draw_info.normals = _WIDGET_normals_arrow,
		arrow_head_draw_info.indices = _WIDGET_indices_arrow,
		arrow_head_draw_info.init    = true;
	}
#endif
	if (!cube_draw_info.init) {
		cube_draw_info.nverts  = _WIDGET_nverts_cube,
		cube_draw_info.ntris   = _WIDGET_ntris_cube,
		cube_draw_info.verts   = _WIDGET_verts_cube,
		cube_draw_info.normals = _WIDGET_normals_cube,
		cube_draw_info.indices = _WIDGET_indices_cube,
		cube_draw_info.init    = true;
	}

	/* inverted only makes sense in a constrained arrow */
	if (real_style & WIDGET_ARROW_STYLE_INVERTED) {
		real_style |= WIDGET_ARROW_STYLE_CONSTRAINED;
	}


	ArrowWidget *arrow = OBJECT_GUARDED_NEW_CALLOC(ArrowWidget, wgroup, name);
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

	arrow->draw                    = widget_arrow_draw;
	arrow->intersect               = NULL;
	arrow->get_final_position      = widget_arrow_get_final_pos;
	arrow->handler                 = widget_arrow_handler;
	arrow->invoke                  = widget_arrow_invoke;
	arrow->render_3d_intersection  = widget_arrow_render_3d_intersect;
	arrow->bind_to_prop            = widget_arrow_bind_to_prop;
	arrow->cancel                  = widget_arrow_cancel;
	arrow->flag                   |= (WM_WIDGET_SCALE_3D | WM_WIDGET_DRAW_ACTIVE);

	arrow->style     = real_style;
	arrow->len       = 1.0f;
	arrow->range_fac = 1.0f;
	copy_v3_v3(arrow->direction, dir_default);

	return (wmWidget *)arrow;
}

/**
 * Define direction the arrow will point towards
 */
void WIDGET_arrow_set_direction(wmWidget *widget, const float direction[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	copy_v3_v3(arrow->direction, direction);
	normalize_v3(arrow->direction);
}

/**
 * Define up-direction of the arrow widget
 */
void WIDGET_arrow_set_up_vector(wmWidget *widget, const float direction[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	if (direction) {
		copy_v3_v3(arrow->up, direction);
		normalize_v3(arrow->up);
		arrow->arrow_flag |= ARROW_UP_VECTOR_SET;
	}
	else {
		arrow->arrow_flag &= ~ARROW_UP_VECTOR_SET;
	}
}

/**
 * Define a custom arrow line length
 */
void WIDGET_arrow_set_line_len(wmWidget *widget, const float len)
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	arrow->len = len;
}

/**
 * Define a custom property UI range
 *
 * \note Needs to be called before WM_widget_set_property!
 */
void WIDGET_arrow_set_ui_range(wmWidget *widget, const float min, const float max)
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	BLI_assert(min < max);
	BLI_assert(!(arrow->props[0] && "Make sure this function is called before WM_widget_set_property"));

	arrow->range = max - min;
	arrow->min = min;
	arrow->arrow_flag |= ARROW_CUSTOM_RANGE_SET;
}

/**
 * Define a custom factor for arrow min/max distance
 *
 * \note Needs to be called before WM_widget_set_property!
 */
void WIDGET_arrow_set_range_fac(wmWidget *widget, const float range_fac)
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	BLI_assert(!(arrow->props[0] && "Make sure this function is called before WM_widget_set_property"));

	arrow->range_fac = range_fac;
}

/**
 * Define xy-aspect for arrow cone
 */
void WIDGET_arrow_cone_set_aspect(wmWidget *widget, const float aspect[2])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	copy_v2_v2(arrow->aspect, aspect);
}

/** \} */ // Arrow Widget API


/* -------------------------------------------------------------------- */

void fix_linking_widget_arrow(void)
{
	(void)0;
}

