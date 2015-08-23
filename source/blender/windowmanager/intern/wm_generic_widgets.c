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

/** \file blender/editors/interface/interface_generic_widgets.c
 *  \ingroup edinterface
 */

#include "RNA_types.h"
#include "RNA_access.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"
#include "DNA_widget_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "WM_types.h"
#include "WM_api.h"

#include "GL/glew.h"
#include "GPU_select.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"

#include "3d_widgets/ui_widget_library.h"

#include "wm.h"
#include "WM_types.h"


/******************************************************
 *            GENERIC WIDGET LIBRARY                  *
 ******************************************************/


/* to use custom arrows exported to arrow_widget.c */
//#define WIDGET_USE_CUSTOM_ARROWS
/* to use custom dials exported to arrow_widget.c */
//#define WIDGET_USE_CUSTOM_DIAS


const float highlight_col[] = {1.0f, 1.0f, 0.45f, 1.0f};

typedef struct WidgetDrawInfo {
	int nverts;
	int ntris;
	float (*verts)[3];
	float (*normals)[3];
	unsigned short *indices;
	bool init;
} WidgetDrawInfo;


#ifdef WIDGET_USE_CUSTOM_ARROWS
WidgetDrawInfo arraw_head_draw_info = {0};
#endif
WidgetDrawInfo cube_draw_info = {0};
#ifdef WIDGET_USE_CUSTOM_DIAS
WidgetDrawInfo dial_draw_info = {0};
#endif

static void widget_draw_intern(WidgetDrawInfo *info, const bool select)
{
	GLuint buf[3];

	const bool use_lighting = !select && ((U.tw_flag & V3D_SHADED_WIDGETS) != 0);

	if (use_lighting)
		glGenBuffers(3, buf);
	else
		glGenBuffers(2, buf);

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * info->nverts, info->verts, GL_STATIC_DRAW);
	glVertexPointer(3, GL_FLOAT, 0, NULL);

	if (use_lighting) {
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, buf[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * info->nverts, info->normals, GL_STATIC_DRAW);
		glNormalPointer(GL_FLOAT, 0, NULL);
		glShadeModel(GL_SMOOTH);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * (3 * info->ntris), info->indices, GL_STATIC_DRAW);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glDrawElements(GL_TRIANGLES, info->ntris * 3, GL_UNSIGNED_SHORT, NULL);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glDisableClientState(GL_VERTEX_ARRAY);

	if (use_lighting) {
		glDisableClientState(GL_NORMAL_ARRAY);
		glShadeModel(GL_FLAT);
		glDeleteBuffers(3, buf);
	}
	else {
		glDeleteBuffers(2, buf);
	}
}

/********* Arrow widget ************/

#define ARROW_UP_VECTOR_SET 1

typedef struct ArrowWidget {
	wmWidget widget;
	int style;
	int flag;
	float direction[3];
	float up[3];
	float color[4];
	float (*line)[3];    /* custom coords for arrow line drawing */
	int tot_line_points; /* amount of points for arrow line drawing */
	float offset;
	/* property range and minimum for constrained arrows */
	float range, min;
} ArrowWidget;

typedef struct ArrowInteraction {
	float orig_origin[3];
	float orig_mouse[2];
	float orig_offset;
	float orig_scale;

	/* direction vector, projected in screen space */
	float proj_direction[2];
} ArrowInteraction;


static void widget_arrow_get_final_pos(wmWidget *widget, float r_pos[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	mul_v3_v3fl(r_pos, arrow->direction, arrow->offset);
	add_v3_v3(r_pos, arrow->widget.origin);
}

static void arrow_draw_geom(const ArrowWidget *arrow, const bool select)
{
	glEnable(GL_MULTISAMPLE_ARB);

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
	else {
#ifdef WIDGET_USE_CUSTOM_ARROWS
		widget_draw_intern(&arraw_head_draw_info, select);
#else
		glLineWidth(arrow->widget.line_width);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, arrow->line);
		glDrawArrays(GL_LINES, 0, arrow->tot_line_points);
		glDisableClientState(GL_VERTEX_ARRAY);
		glLineWidth(1.0);

		/* draw arrow head */

		glTranslatef(UNPACK3(arrow->line[arrow->tot_line_points - 1]));

		if (arrow->style & WIDGET_ARROW_STYLE_BOX) {
			const float size = 0.05f;

			/* draw cube */
			glScalef(size, size, size);
			widget_draw_intern(&cube_draw_info, select);
		}
		else {
			GLUquadricObj *qobj = gluNewQuadric();
			const float len = 0.25f;
			const float width = 0.06f;

			gluQuadricDrawStyle(qobj, GLU_FILL);
			gluCylinder(qobj, width, 0.0, len, 8, 1);
			gluQuadricOrientation(qobj, GLU_INSIDE);
			gluDisk(qobj, 0.0, width, 8, 1);
			gluQuadricOrientation(qobj, GLU_OUTSIDE);
		}

		(void)select;
#endif
	}

	glDisable(GL_MULTISAMPLE_ARB);
}

static void arrow_draw_intern(ArrowWidget *arrow, const bool select, const bool highlight)
{
	const float up[3] = {0.0f, 0.0f, 1.0f};
	float rot[3][3];
	float mat[4][4];
	float final_pos[3];

	widget_arrow_get_final_pos(&arrow->widget, final_pos);

	if (arrow->flag & ARROW_UP_VECTOR_SET) {
		copy_v3_v3(rot[2], arrow->direction);
		copy_v3_v3(rot[1], arrow->up);
		cross_v3_v3v3(rot[0], arrow->up, arrow->direction);
	}
	else {
		rotation_between_vecs_to_mat3(rot, up, arrow->direction);
	}
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], final_pos);
	mul_mat3_m4_fl(mat, arrow->widget.scale);

	glPushMatrix();
	glMultMatrixf(mat);

	if (highlight && !(arrow->widget.flag & WM_WIDGET_DRAW_HOVER)) {
		glColor4fv(highlight_col);
	}
	else {
		glColor4fv(arrow->color);
	}

	glEnable(GL_BLEND);
	arrow_draw_geom(arrow, select);
	glDisable(GL_BLEND);

	glPopMatrix();

	if (arrow->widget.interaction_data) {
		ArrowInteraction *data = arrow->widget.interaction_data;

		copy_m4_m3(mat, rot);
		copy_v3_v3(mat[3], data->orig_origin);
		mul_mat3_m4_fl(mat, data->orig_scale);

		glPushMatrix();
		glMultMatrixf(mat);

		glEnable(GL_BLEND);
		glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
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

#define ARROW_RANGE 1.5f

static int widget_arrow_handler(bContext *C, const wmEvent *event, wmWidget *widget)
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	ArrowInteraction *data = widget->interaction_data;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	float orig_origin[4];
	float viewvec[3], tangent[3], plane[3];
	float offset[4];
	float m_diff[2];
	float dir_2d[2], dir2d_final[2];
	float fac, zfac;
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

	zfac = ED_view3d_calc_zfac(rv3d, orig_origin, NULL);

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

	/* set the property for the operator and call its modal function */
	if (widget->props[ARROW_SLOT_OFFSET_WORLD_SPACE]) {
		PointerRNA ptr = widget->ptr[ARROW_SLOT_OFFSET_WORLD_SPACE];
		PropertyRNA *prop = widget->props[ARROW_SLOT_OFFSET_WORLD_SPACE];
		float value;

		value = data->orig_offset + facdir * len_v3(offset);
		if (arrow->style & WIDGET_ARROW_STYLE_CONSTRAINED) {
			if (arrow->style & WIDGET_ARROW_STYLE_INVERTED)
				value = arrow->min + arrow->range - (value * arrow->range / ARROW_RANGE);
			else
				value = arrow->min + (value * arrow->range / ARROW_RANGE);
		}

		RNA_property_float_set(&ptr, prop, value);
		RNA_property_update(C, &ptr, prop);
		/* get clamped value */
		value = RNA_property_float_get(&ptr, prop);

		/* accounts for clamping properly */
		if (arrow->style & WIDGET_ARROW_STYLE_CONSTRAINED) {
			if (arrow->style & WIDGET_ARROW_STYLE_INVERTED)
				arrow->offset = ARROW_RANGE * (arrow->min + arrow->range - value) / arrow->range;
			else
				arrow->offset = ARROW_RANGE * ((value - arrow->min) / arrow->range);
		}
		else
			arrow->offset = value;
	}
	else {
		arrow->offset = facdir * len_v3(offset);
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(ar);

	return OPERATOR_PASS_THROUGH;
}


static int widget_arrow_invoke(bContext *UNUSED(C), const wmEvent *event, wmWidget *widget)
{
	ArrowWidget *arrow = (ArrowWidget *) widget;
	ArrowInteraction *data = MEM_callocN(sizeof (ArrowInteraction), "arrow_interaction");

	data->orig_offset = arrow->offset;

	data->orig_mouse[0] = event->mval[0];
	data->orig_mouse[1] = event->mval[1];

	data->orig_scale = widget->scale;

	widget_arrow_get_final_pos(widget, data->orig_origin);

	widget->interaction_data = data;

	return OPERATOR_RUNNING_MODAL;
}

static void widget_arrow_bind_to_prop(wmWidget *widget, const int UNUSED(slot))
{
	ArrowWidget *arrow = (ArrowWidget *) widget;
	PointerRNA ptr = widget->ptr[ARROW_SLOT_OFFSET_WORLD_SPACE];
	PropertyRNA *prop = widget->props[ARROW_SLOT_OFFSET_WORLD_SPACE];

	if (prop) {
		const float float_prop = RNA_property_float_get(&ptr, prop);

		if (arrow->style & WIDGET_ARROW_STYLE_CONSTRAINED) {
			float min, max, step, precision;

			RNA_property_float_ui_range(&ptr, prop, &min, &max, &step, &precision);
			arrow->range = max - min;
			arrow->min = min;

			if (arrow->style & WIDGET_ARROW_STYLE_INVERTED) {
				arrow->offset = ARROW_RANGE * (max - float_prop) / arrow->range;
			}
			else {
				arrow->offset = ARROW_RANGE * ((float_prop - arrow->min) / arrow->range);
			}
		}
		else {
			/* we'd need to check the property type here but for now assume always float */
			arrow->offset = float_prop;
		}
	}
	else
		arrow->offset = 0.0f;
}

wmWidget *WIDGET_arrow_new(wmWidgetGroup *wgroup, const char *name, const int style)
{
	ArrowWidget *arrow;
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};
	const float line_default[2][3] = {
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f}
	};
	int real_style = style;

#ifdef WIDGET_USE_CUSTOM_ARROWS
	if (!arraw_head_draw_info.init) {
		arraw_head_draw_info.nverts = _WIDGET_nverts_arrow,
		arraw_head_draw_info.ntris = _WIDGET_ntris_arrow,
		arraw_head_draw_info.verts = _WIDGET_verts_arrow,
		arraw_head_draw_info.normals = _WIDGET_normals_arrow,
		arraw_head_draw_info.indices = _WIDGET_indices_arrow,
		arraw_head_draw_info.init = true;
	}
#endif
	if (!cube_draw_info.init) {
		cube_draw_info.nverts = _WIDGET_nverts_cube,
		cube_draw_info.ntris = _WIDGET_ntris_cube,
		cube_draw_info.verts = _WIDGET_verts_cube,
		cube_draw_info.normals = _WIDGET_normals_cube,
		cube_draw_info.indices = _WIDGET_indices_cube,
		cube_draw_info.init = true;
	}

	/* inverted only makes sense in a constrained arrow */
	if (real_style & WIDGET_ARROW_STYLE_INVERTED) {
		real_style |= WIDGET_ARROW_STYLE_CONSTRAINED;
	}


	arrow = MEM_callocN(sizeof(ArrowWidget), name);

	arrow->widget.draw = widget_arrow_draw;
	arrow->widget.get_final_position = 	widget_arrow_get_final_pos;
	arrow->widget.intersect = NULL;
	arrow->widget.handler = widget_arrow_handler;
	arrow->widget.invoke = widget_arrow_invoke;
	arrow->widget.render_3d_intersection = widget_arrow_render_3d_intersect;
	arrow->widget.bind_to_prop = widget_arrow_bind_to_prop;
	arrow->widget.flag |= WM_WIDGET_SCALE_3D;
	arrow->style = real_style;
	copy_v3_v3(arrow->direction, dir_default);
	arrow->tot_line_points = ARRAY_SIZE(line_default);
	arrow->line = MEM_mallocN(sizeof(line_default), __func__);
	memcpy(arrow->line, line_default, sizeof(line_default));

	wm_widget_register(wgroup, &arrow->widget, name);

	return (wmWidget *)arrow;
}

void WIDGET_arrow_set_color(wmWidget *widget, const float color[4])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	
	copy_v4_v4(arrow->color, color);
}

void WIDGET_arrow_set_direction(wmWidget *widget, const float direction[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	
	copy_v3_v3(arrow->direction, direction);
	normalize_v3(arrow->direction);
}

void WIDGET_arrow_set_up_vector(wmWidget *widget, const float direction[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	if (direction) {
		copy_v3_v3(arrow->up, direction);
		normalize_v3(arrow->up);
		arrow->flag |= ARROW_UP_VECTOR_SET;
	}
	else {
		arrow->flag &= ~ARROW_UP_VECTOR_SET;
	}
}

/**
 * Define a custom coord vec for arrow line drawing
 */
void WIDGET_arrow_set_line_vec(wmWidget *widget, const float (*vec)[3], const int tot_points)
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	const size_t vec_size = 3 * tot_points * sizeof(float);

	arrow->tot_line_points = tot_points;
	arrow->line = MEM_reallocN(arrow->line, vec_size);
	memcpy(arrow->line, vec, vec_size);
}


/********* Dial widget ************/

typedef struct DialWidget {
	wmWidget widget;
	int style;
	float direction[3];
	float color[4];
} DialWidget;

static void dial_draw_geom(const DialWidget *dial, const bool select)
{
	GLUquadricObj *qobj = gluNewQuadric();
	const float width = 1.0f;
	const int resol = 32;

	glEnable(GL_MULTISAMPLE_ARB);

#ifdef WIDGET_USE_CUSTOM_DIAS
	widget_draw_intern(&dial_draw_info, select);

	(void)qobj; (void)width; (void)resol;
#else

	glLineWidth(dial->widget.line_width);
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE);
	gluDisk(qobj, 0.0, width, resol, 1);
	glLineWidth(1.0);

	(void)select;
#endif

	glDisable(GL_MULTISAMPLE_ARB);
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
		glColor4fv(highlight_col);
	else
		glColor4fv(dial->color);

	dial_draw_geom(dial, select);

	glPopMatrix();

}

static void widget_dial_render_3d_intersect(const bContext *C, wmWidget *widget, int selectionbase)
{
	DialWidget *dial = (DialWidget *)widget;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	/* enable clipping if needed */
	if (dial->style == WIDGET_DIAL_STYLE_RING_CLIPPED) {
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
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	/* enable clipping if needed */
	if (dial->style == WIDGET_DIAL_STYLE_RING_CLIPPED) {
		double plane[4];
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

wmWidget *WIDGET_dial_new(wmWidgetGroup *wgroup, const char *name, const int style)
{
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};
	DialWidget *dial;

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

	dial = MEM_callocN(sizeof(ArrowWidget), name);

	dial->widget.draw = widget_dial_draw;
	dial->widget.intersect = NULL;
	dial->widget.render_3d_intersection = widget_dial_render_3d_intersect;
	dial->widget.flag |= WM_WIDGET_SCALE_3D;

	dial->style = style;
	copy_v3_v3(dial->direction, dir_default);

	wm_widget_register(wgroup, &dial->widget, name);

	return (wmWidget *)dial;
}

void WIDGET_dial_set_color(wmWidget *widget, const float color[4])
{
	DialWidget *arrow = (DialWidget *)widget;

	copy_v4_v4(arrow->color, color);
}

void WIDGET_dial_set_direction(wmWidget *widget, const float direction[3])
{
	DialWidget *dial = (DialWidget *)widget;

	copy_v3_v3(dial->direction, direction);
	normalize_v3(dial->direction);
}

/********* Plane widget ************/

typedef struct PlaneWidget {
	wmWidget widget;

	float direction[3];
	float color[4];
} PlaneWidget;


static void widget_plane_draw_geom(const float col_inner[4], const float col_outer[4])
{
	static float vec[4][3] = {
		{-1, -1, 0},
		{ 1, -1, 0},
		{ 1,  1, 0},
		{-1,  1, 0},
	};

	glEnable(GL_MULTISAMPLE_ARB);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vec);
	glColor4fv(col_inner);
	glDrawArrays(GL_QUADS, 0, ARRAY_SIZE(vec));
	glColor4fv(col_outer);
	glDrawArrays(GL_LINE_LOOP, 0, ARRAY_SIZE(vec));
	glDisableClientState(GL_VERTEX_ARRAY);

	glDisable(GL_MULTISAMPLE_ARB);
}

static void widget_plane_draw_intern(PlaneWidget *plane, const bool UNUSED(select), const bool highlight)
{
	const float up[3] = {0.0f, 0.0f, 1.0f};
	float col_inner[4], col_outer[4];
	float rot[3][3];
	float mat[4][4];

	rotation_between_vecs_to_mat3(rot, up, plane->direction);

	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], plane->widget.origin);
	mul_mat3_m4_fl(mat, plane->widget.scale);

	glPushMatrix();
	glMultMatrixf(mat);

	if (highlight && !(plane->widget.flag & WM_WIDGET_DRAW_HOVER)) {
		copy_v4_v4(col_inner, highlight_col);
		copy_v4_v4(col_outer, highlight_col);
	}
	else {
		copy_v4_v4(col_inner, plane->color);
		copy_v4_v4(col_outer, plane->color);
	}
	col_inner[3] *= 0.5f;

	glEnable(GL_BLEND);
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

wmWidget *WIDGET_plane_new(wmWidgetGroup *wgroup, const char *name, const int UNUSED(style))
{
	PlaneWidget *plane;
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

	plane = MEM_callocN(sizeof(PlaneWidget), name);

	plane->widget.draw = widget_plane_draw;
	plane->widget.intersect = NULL;
	plane->widget.render_3d_intersection = widget_plane_render_3d_intersect;
	plane->widget.flag |= WM_WIDGET_SCALE_3D;
	copy_v3_v3(plane->direction, dir_default);

	wm_widget_register(wgroup, &plane->widget, name);

	return (wmWidget *)plane;
}

void WIDGET_plane_set_direction(wmWidget *widget, const float direction[3])
{
	PlaneWidget *plane = (PlaneWidget *)widget;

	copy_v3_v3(plane->direction, direction);
	normalize_v3(plane->direction);
}

void WIDGET_plane_set_color(wmWidget *widget, const float color[4])
{
	PlaneWidget *plane = (PlaneWidget *)widget;

	copy_v4_v4(plane->color, color);
}

/********* Cage widget ************/

enum {
	WIDGET_RECT_TRANSFORM_INTERSECT_TRANSLATE     = 1,
	WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT   = 2,
	WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT  = 3,
	WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_UP     = 4,
	WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN   = 5
};

#define WIDGET_RECT_MIN_WIDTH 15.0f
#define WIDGET_RESIZER_WIDTH  20.0f

typedef struct RectTransformWidget {
	wmWidget widget;
	float offset[2]; /* position of widget */
	float w, h;      /* dimensions of widget */
	float rotation;  /* rotation of the rectangle */
	float scale[2]; /* scaling for the widget for non-destructive editing. */
	int style;
} RectTransformWidget;

static void rect_transform_draw_corners(rctf *r, const float offsetx, const float offsety)
{
	glBegin(GL_LINES);
	glVertex2f(r->xmin, r->ymin + offsety);
	glVertex2f(r->xmin, r->ymin);
	glVertex2f(r->xmin, r->ymin);
	glVertex2f(r->xmin + offsetx, r->ymin);

	glVertex2f(r->xmax, r->ymin + offsety);
	glVertex2f(r->xmax, r->ymin);
	glVertex2f(r->xmax, r->ymin);
	glVertex2f(r->xmax - offsetx, r->ymin);

	glVertex2f(r->xmax, r->ymax - offsety);
	glVertex2f(r->xmax, r->ymax);
	glVertex2f(r->xmax, r->ymax);
	glVertex2f(r->xmax - offsetx, r->ymax);

	glVertex2f(r->xmin, r->ymax - offsety);
	glVertex2f(r->xmin, r->ymax);
	glVertex2f(r->xmin, r->ymax);
	glVertex2f(r->xmin + offsetx, r->ymax);
	glEnd();
}

static void rect_transform_draw_interaction(const int highlighted, const float half_w, const float half_h,
                                            const float w, const float h, const float line_width)
{
	float verts[4][2];
	unsigned short elems[4] = {0, 1, 3, 2};
	
	switch (highlighted) {
		case WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT:
			verts[0][0] = -half_w + w;
			verts[0][1] = -half_h;
			verts[1][0] = -half_w;
			verts[1][1] = -half_h;
			verts[2][0] = -half_w;
			verts[2][1] = half_h;
			verts[3][0] = -half_w + w;
			verts[3][1] = half_h;
			break;
			
		case WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT:
			verts[0][0] = half_w - w;
			verts[0][1] = -half_h;
			verts[1][0] = half_w;
			verts[1][1] = -half_h;
			verts[2][0] = half_w;
			verts[2][1] = half_h;
			verts[3][0] = half_w - w;
			verts[3][1] = half_h;
			break;
			
		case WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN:
			verts[0][0] = -half_w;
			verts[0][1] = -half_h + h;
			verts[1][0] = -half_w;
			verts[1][1] = -half_h;
			verts[2][0] = half_w;
			verts[2][1] = -half_h;
			verts[3][0] = half_w;
			verts[3][1] = -half_h + h;
			break;
			
		case WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_UP:
			verts[0][0] = -half_w;
			verts[0][1] = half_h - h;
			verts[1][0] = -half_w;
			verts[1][1] = half_h;
			verts[2][0] = half_w;
			verts[2][1] = half_h;
			verts[3][0] = half_w;
			verts[3][1] = half_h - h;
			break;
			
		default:
			return;
	}
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glLineWidth(line_width + 3.0);
	glColor3f(0.0, 0.0, 0.0);
	glDrawArrays(GL_LINE_STRIP, 0, 3);
	glLineWidth(line_width);
	glColor3f(1.0, 1.0, 1.0);
	glDrawArrays(GL_LINE_STRIP, 0, 3);
	glLineWidth(1.0);

	(void)elems;
}

static void widget_rect_transform_draw(const bContext *UNUSED(C), wmWidget *widget)
{
	RectTransformWidget *cage = (RectTransformWidget *)widget;
	rctf r;
	float w = cage->w;
	float h = cage->h;
	float half_w = w / 2.0f;
	float half_h = h / 2.0f;
	float aspx = 1.0f, aspy = 1.0f;
	
	r.xmin = -half_w;
	r.ymin = -half_h;
	r.xmax = half_w;
	r.ymax = half_h;
	
	glPushMatrix();
	glTranslatef(widget->origin[0] + cage->offset[0], widget->origin[1] + cage->offset[1], 0.0f);
	if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM)
		glScalef(cage->scale[0], cage->scale[0], 1.0);
	else
		glScalef(cage->scale[0], cage->scale[1], 1.0);

	if (w > h)
		aspx = h / w;
	else
		aspy = w / h;
	w = min_ff(aspx * w / WIDGET_RESIZER_WIDTH, WIDGET_RESIZER_WIDTH / cage->scale[0]);
	h = min_ff(aspy * h / WIDGET_RESIZER_WIDTH, WIDGET_RESIZER_WIDTH / 
	           ((cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) ? cage->scale[0] : cage->scale[1]));

	/* corner widgets */
	glColor3f(0.0, 0.0, 0.0);
	glLineWidth(cage->widget.line_width + 3.0f);

	rect_transform_draw_corners(&r, w, h);

	/* corner widgets */
	glColor3f(1.0, 1.0, 1.0);
	glLineWidth(cage->widget.line_width);
	rect_transform_draw_corners(&r, w, h);

	rect_transform_draw_interaction(widget->highlighted_part, half_w, half_h, w, h, cage->widget.line_width);

	glLineWidth(1.0);
	glPopMatrix();
}

static int widget_rect_tranfrorm_get_cursor(wmWidget *widget)
{
	switch (widget->highlighted_part) {
		case WIDGET_RECT_TRANSFORM_INTERSECT_TRANSLATE:
			return BC_HANDCURSOR;
		case WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT:
		case WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT:
			return CURSOR_X_MOVE;
		case WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN:
		case WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_UP:
			return CURSOR_Y_MOVE;
		default:
			return CURSOR_STD;
	}
}

static int widget_rect_tranfrorm_intersect(bContext *UNUSED(C), const wmEvent *event, wmWidget *widget)
{
	RectTransformWidget *cage = (RectTransformWidget *)widget;
	const float mouse[2] = {event->mval[0], event->mval[1]};
	float point_local[2];
	float w = cage->w;
	float h = cage->h;
	float half_w = w / 2.0f;
	float half_h = h / 2.0f;
	//float matrot[2][2];
	bool isect;
	rctf r;
	float aspx = 1.0f, aspy = 1.0f;
	
	/* rotate mouse in relation to the center and relocate it */
	sub_v2_v2v2(point_local, mouse, widget->origin);
	point_local[0] -= cage->offset[0];
	point_local[1] -= cage->offset[1];
	//rotate_m2(matrot, -cage->transform.rotation);

	if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM)
		mul_v2_fl(point_local, 1.0f/cage->scale[0]);
	else {
		point_local[0] /= cage->scale[0];
		point_local[1] /= cage->scale[0];
	}
	
	if (cage->w > cage->h)
		aspx = h / w;
	else
		aspy = w / h;
	w = min_ff(aspx * w / WIDGET_RESIZER_WIDTH, WIDGET_RESIZER_WIDTH / cage->scale[0]);
	h = min_ff(aspy * h / WIDGET_RESIZER_WIDTH, WIDGET_RESIZER_WIDTH / 
	           ((cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) ? cage->scale[0] : cage->scale[1]));

	r.xmin = -half_w + w;
	r.ymin = -half_h + h;
	r.xmax = half_w - w;
	r.ymax = half_h - h;
	
	isect = BLI_rctf_isect_pt_v(&r, point_local);
	
	if (isect)
		return WIDGET_RECT_TRANSFORM_INTERSECT_TRANSLATE;

	/* if widget does not have a scale intersection, don't do it */
	if (cage->style & (WIDGET_RECT_TRANSFORM_STYLE_SCALE | WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM)) {
		r.xmin = -half_w;
		r.ymin = -half_h;
		r.xmax = -half_w + w;
		r.ymax = half_h;
		
		isect = BLI_rctf_isect_pt_v(&r, point_local);
		
		if (isect)
			return WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT;
		
		r.xmin = half_w - w;
		r.ymin = -half_h;
		r.xmax = half_w;
		r.ymax = half_h;
		
		isect = BLI_rctf_isect_pt_v(&r, point_local);
		
		if (isect)
			return WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT;
		
		r.xmin = -half_w;
		r.ymin = -half_h;
		r.xmax = half_w;
		r.ymax = -half_h + h;
		
		isect = BLI_rctf_isect_pt_v(&r, point_local);
		
		if (isect)
			return WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN;
		
		r.xmin = -half_w;
		r.ymin = half_h - h;
		r.xmax = half_w;
		r.ymax = half_h;
		
		isect = BLI_rctf_isect_pt_v(&r, point_local);
		
		if (isect)
			return WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_UP;
	}
	
	return 0;
}

typedef struct RectTransformInteraction {
	float orig_mouse[2];
	float orig_offset[2];
	float orig_scale[2];
} RectTransformInteraction;

static bool widget_rect_transform_get_property(wmWidget *widget, const int slot, float *value)
{
	PropertyType type = RNA_property_type(widget->props[slot]);

	if (type != PROP_FLOAT) {
		fprintf(stderr, "Rect Transform widget can only be bound to float properties");
		return false;
	}
	else {
		if (slot == RECT_TRANSFORM_SLOT_OFFSET) {
			if (RNA_property_array_length(&widget->ptr[slot], widget->props[slot]) != 2) {
				fprintf(stderr, "Rect Transform widget offset not only be bound to array float property");
				return false;
			}
			
			RNA_property_float_get_array(&widget->ptr[slot], widget->props[slot], value);
		}
		else if (slot == RECT_TRANSFORM_SLOT_SCALE) {
			RectTransformWidget *cage = (RectTransformWidget *)widget;
			if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM)
				*value = RNA_property_float_get(&widget->ptr[slot], widget->props[slot]);
			else {
				if (RNA_property_array_length(&widget->ptr[slot], widget->props[slot]) != 2) {
					fprintf(stderr, "Rect Transform widget scale not only be bound to array float property");
					return false;
				}
				RNA_property_float_get_array(&widget->ptr[slot], widget->props[slot], value);
			}
		}
	}
	
	return true;
}

static int widget_rect_transform_invoke(bContext *UNUSED(C), const wmEvent *event, wmWidget *widget)
{
	RectTransformWidget *cage = (RectTransformWidget *) widget;
	RectTransformInteraction *data = MEM_callocN(sizeof (RectTransformInteraction), "cage_interaction");
	
	copy_v2_v2(data->orig_offset, cage->offset);
	copy_v2_v2(data->orig_scale, cage->scale);
	
	data->orig_mouse[0] = event->mval[0];
	data->orig_mouse[1] = event->mval[1];
	
	widget->interaction_data = data;
	
	return OPERATOR_RUNNING_MODAL;
}

static int widget_rect_transform_handler(bContext *C, const wmEvent *event, wmWidget *widget)
{
	RectTransformWidget *cage = (RectTransformWidget *) widget;
	RectTransformInteraction *data = widget->interaction_data;
	ARegion *ar = CTX_wm_region(C);
	float valuex, valuey;
	/* needed here as well in case clamping occurs */
	const float orig_ofx = cage->offset[0], orig_ofy = cage->offset[1];
	
	valuex = (event->mval[0] - data->orig_mouse[0]);
	valuey = (event->mval[1] - data->orig_mouse[1]);
	
	if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_TRANSLATE) {
		cage->offset[0] = data->orig_offset[0] + valuex;
		cage->offset[1] = data->orig_offset[1] + valuey;
	}
	else if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT) {
		cage->offset[0] = data->orig_offset[0] + valuex / 2.0;
		cage->scale[0] = (cage->w * data->orig_scale[0] - valuex) / cage->w;
	}
	else if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT) {
		cage->offset[0] = data->orig_offset[0] + valuex / 2.0;
		cage->scale[0] = (cage->w * data->orig_scale[0] + valuex) / cage->w;
	}
	else if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN) {
		cage->offset[1] = data->orig_offset[1] + valuey / 2.0;
		
		if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
			cage->scale[0] = (cage->h * data->orig_scale[0] - valuey) / cage->h;
		}
		else {
			cage->scale[1] = (cage->h * data->orig_scale[1] - valuey) / cage->h;
		}
	}
	else if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_UP) {
		cage->offset[1] = data->orig_offset[1] + valuey / 2.0;
		
		if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
			cage->scale[0] = (cage->h * data->orig_scale[0] + valuey) / cage->h;
		}
		else {
			cage->scale[1] = (cage->h * data->orig_scale[1] + valuey) / cage->h;
		}
	}
	
	/* clamping - make sure widget is at least 5 pixels wide */
	if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
		if (cage->scale[0] < WIDGET_RECT_MIN_WIDTH / cage->h || 
		    cage->scale[0] < WIDGET_RECT_MIN_WIDTH / cage->w) 
		{
			cage->scale[0] = max_ff(WIDGET_RECT_MIN_WIDTH / cage->h, WIDGET_RECT_MIN_WIDTH / cage->w);
			cage->offset[0] = orig_ofx;
			cage->offset[1] = orig_ofy;
		}
	}
	else {
		if (cage->scale[0] < WIDGET_RECT_MIN_WIDTH / cage->w) {
			cage->scale[0] = WIDGET_RECT_MIN_WIDTH / cage->w;
			cage->offset[0] = orig_ofx;
		}
		if (cage->scale[1] < WIDGET_RECT_MIN_WIDTH / cage->h) {
			cage->scale[1] = WIDGET_RECT_MIN_WIDTH / cage->h;
			cage->offset[1] = orig_ofy;
		}
	}
	
	if (widget->props[RECT_TRANSFORM_SLOT_OFFSET]) {
		PointerRNA ptr = widget->ptr[RECT_TRANSFORM_SLOT_OFFSET];
		PropertyRNA *prop = widget->props[RECT_TRANSFORM_SLOT_OFFSET];

		RNA_property_float_set_array(&ptr, prop, cage->offset);
		RNA_property_update(C, &ptr, prop);
	}

	if (widget->props[RECT_TRANSFORM_SLOT_SCALE]) {
		PointerRNA ptr = widget->ptr[RECT_TRANSFORM_SLOT_SCALE];
		PropertyRNA *prop = widget->props[RECT_TRANSFORM_SLOT_SCALE];

		if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM){
			RNA_property_float_set(&ptr, prop, cage->scale[0]);
		}
		else {
			RNA_property_float_set_array(&ptr, prop, cage->scale);
		}
		RNA_property_update(C, &ptr, prop);
	}
	
	/* tag the region for redraw */
	ED_region_tag_redraw(ar);
	
	return OPERATOR_PASS_THROUGH;
}

static void widget_rect_transform_bind_to_prop(wmWidget *widget, const int slot)
{
	RectTransformWidget *cage = (RectTransformWidget *) widget;
	
	if (slot == RECT_TRANSFORM_SLOT_OFFSET)
		widget_rect_transform_get_property(widget, RECT_TRANSFORM_SLOT_OFFSET, cage->offset);
	if (slot == RECT_TRANSFORM_SLOT_SCALE)
		widget_rect_transform_get_property(widget, RECT_TRANSFORM_SLOT_SCALE, cage->scale);
}

wmWidget *WIDGET_rect_transform_new(
        wmWidgetGroup *wgroup, const char *name, const int style,
        const float width, const float height)
{
	RectTransformWidget *cage = MEM_callocN(sizeof(RectTransformWidget), name);

	cage->widget.draw = widget_rect_transform_draw;
	cage->widget.invoke = widget_rect_transform_invoke;
	cage->widget.bind_to_prop = widget_rect_transform_bind_to_prop;
	cage->widget.handler = widget_rect_transform_handler;
	cage->widget.intersect = widget_rect_tranfrorm_intersect;
	cage->widget.get_cursor = widget_rect_tranfrorm_get_cursor;
	cage->widget.max_prop = 2;
	cage->scale[0] = cage->scale[1] = 1.0f;
	cage->style = style;
	cage->w = width;
	cage->h = height;
	
	wm_widget_register(wgroup, &cage->widget, name);
	
	return (wmWidget *)cage;
}

void WIDGET_rect_transform_set_offset(wmWidget *widget, const float offset[2])
{
	RectTransformWidget *cage = (RectTransformWidget *)widget;

	copy_v2_v2(cage->offset, offset);
}

/********* Facemap widget ************/

typedef struct FacemapWidget {
	wmWidget widget;
	Object *ob;
	int facemap;
	int style;
	float color[4];
} FacemapWidget;


static void widget_facemap_draw(const bContext *C, wmWidget *widget)
{
	FacemapWidget *fmap_widget = (FacemapWidget *)widget;
	glPushMatrix();
	glMultMatrixf(&fmap_widget->ob->obmat[0][0]);
	ED_draw_object_facemap(CTX_data_scene(C), fmap_widget->ob, fmap_widget->facemap);
	glPopMatrix();
}

static void widget_facemap_render_3d_intersect(const bContext *C, wmWidget *widget, int selectionbase)
{
	GPU_select_load_id(selectionbase);
	widget_facemap_draw(C, widget);
}


void WIDGET_facemap_set_color(wmWidget *widget, const float color[4])
{
	FacemapWidget *fmap_widget = (FacemapWidget *)widget;
	copy_v4_v4(fmap_widget->color, color);
}

struct wmWidget *WIDGET_facemap_new(
        wmWidgetGroup *wgroup, const char *name, const int style,
        Object *ob, const int facemap)
{
	FacemapWidget *fmap_widget = MEM_callocN(sizeof(RectTransformWidget), "CageWidget");

	fmap_widget->widget.draw = widget_facemap_draw;
//	fmap_widget->widget.invoke = NULL;
//	fmap_widget->widget.bind_to_prop = NULL;
//	fmap_widget->widget.handler = NULL;
	fmap_widget->widget.render_3d_intersection = widget_facemap_render_3d_intersect;
	fmap_widget->ob = ob;
	fmap_widget->facemap = facemap;
	fmap_widget->style = style;
	
	wm_widget_register(wgroup, &fmap_widget->widget, name);
	
	return (wmWidget *)fmap_widget;
}


void fix_linking_widget_lib(void)
{
	(void) 0;
}
