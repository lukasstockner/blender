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
 * \name Cage Widget
 *
 * 2D Widget
 *
 * \brief Rectangular widget acting as a 'cage' around its content. Interacting scales or translates the widget.
 */

#include <new>

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "ED_screen.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"

#include "../wm_widget.h"


/* wmWidget->highlighted_part */
enum {
	WIDGET_RECT_TRANSFORM_INTERSECT_TRANSLATE     = 1,
	WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT   = 2,
	WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT  = 3,
	WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_UP     = 4,
	WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN   = 5
};

#define WIDGET_RECT_MIN_WIDTH 15.0f
#define WIDGET_RESIZER_WIDTH  20.0f

typedef struct RectTransformWidget: wmWidget {
	RectTransformWidget(wmWidgetGroup *wgroup, const char *name, const int max_prop);

	float w, h;      /* dimensions of widget */
	float rotation;  /* rotation of the rectangle */
	float scale[2]; /* scaling for the widget for non-destructive editing. */
	int style;
} RectTransformWidget;


RectTransformWidget::RectTransformWidget(wmWidgetGroup *wgroup, const char *name, const int max_prop)
    : wmWidget(wgroup, name, max_prop)
{
	
}

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

static void rect_transform_draw_interaction(
        const float col[4], const int highlighted,
        const float half_w, const float half_h,
        const float w, const float h, const float line_width)
{
	float verts[4][2];

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
	glColor3fv(col);
	glDrawArrays(GL_LINE_STRIP, 0, 3);
	glLineWidth(1.0);
}

static void widget_rect_transform_draw(const bContext *UNUSED(C), wmWidget *widget)
{
	RectTransformWidget *cage = (RectTransformWidget *)widget;
	rctf r;
	float w = cage->w;
	float h = cage->h;
	float aspx = 1.0f, aspy = 1.0f;
	const float half_w = w / 2.0f;
	const float half_h = h / 2.0f;

	r.xmin = -half_w;
	r.ymin = -half_h;
	r.xmax = half_w;
	r.ymax = half_h;

	glPushMatrix();
	glTranslatef(widget->origin[0] + widget->offset[0], widget->origin[1] + widget->offset[1], 0.0f);
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
	glLineWidth(cage->line_width + 3.0f);

	rect_transform_draw_corners(&r, w, h);

	/* corner widgets */
	glColor3fv(widget->col);
	glLineWidth(cage->line_width);
	rect_transform_draw_corners(&r, w, h);

	rect_transform_draw_interaction(widget->col, widget->highlighted_part, half_w, half_h,
	                                w, h, cage->line_width);

	glLineWidth(1.0);
	glPopMatrix();
}

static int widget_rect_transform_get_cursor(wmWidget *widget)
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

static int widget_rect_transform_intersect(bContext *UNUSED(C), const wmEvent *event, wmWidget *widget)
{
	RectTransformWidget *cage = (RectTransformWidget *)widget;
	const float mouse[2] = {(float)event->mval[0], (float)event->mval[1]};
	//float matrot[2][2];
	float point_local[2];
	float w = cage->w;
	float h = cage->h;
	float half_w = w / 2.0f;
	float half_h = h / 2.0f;
	float aspx = 1.0f, aspy = 1.0f;

	/* rotate mouse in relation to the center and relocate it */
	sub_v2_v2v2(point_local, mouse, widget->origin);
	point_local[0] -= widget->offset[0];
	point_local[1] -= widget->offset[1];
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


	rctf r;

	r.xmin = -half_w + w;
	r.ymin = -half_h + h;
	r.xmax = half_w - w;
	r.ymax = half_h - h;

	bool isect = BLI_rctf_isect_pt_v(&r, point_local);

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
			if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
				*value = RNA_property_float_get(&widget->ptr[slot], widget->props[slot]);
			}
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
	RectTransformWidget *cage = (RectTransformWidget *)widget;
	RectTransformInteraction *data = (RectTransformInteraction *)MEM_callocN(sizeof (RectTransformInteraction),
	                                                                         "cage_interaction");

	copy_v2_v2(data->orig_offset, widget->offset);
	copy_v2_v2(data->orig_scale, cage->scale);

	data->orig_mouse[0] = event->mval[0];
	data->orig_mouse[1] = event->mval[1];

	widget->interaction_data = data;

	return OPERATOR_RUNNING_MODAL;
}

static int widget_rect_transform_handler(bContext *C, const wmEvent *event, wmWidget *widget, const int UNUSED(flag))
{
	RectTransformWidget *cage = (RectTransformWidget *)widget;
	RectTransformInteraction *data = (RectTransformInteraction *)widget->interaction_data;
	/* needed here as well in case clamping occurs */
	const float orig_ofx = widget->offset[0], orig_ofy = widget->offset[1];

	const float valuex = (event->mval[0] - data->orig_mouse[0]);
	const float valuey = (event->mval[1] - data->orig_mouse[1]);


	if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_TRANSLATE) {
		widget->offset[0] = data->orig_offset[0] + valuex;
		widget->offset[1] = data->orig_offset[1] + valuey;
	}
	else if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT) {
		widget->offset[0] = data->orig_offset[0] + valuex / 2.0;
		cage->scale[0] = (cage->w * data->orig_scale[0] - valuex) / cage->w;
	}
	else if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT) {
		widget->offset[0] = data->orig_offset[0] + valuex / 2.0;
		cage->scale[0] = (cage->w * data->orig_scale[0] + valuex) / cage->w;
	}
	else if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN) {
		widget->offset[1] = data->orig_offset[1] + valuey / 2.0;

		if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
			cage->scale[0] = (cage->h * data->orig_scale[0] - valuey) / cage->h;
		}
		else {
			cage->scale[1] = (cage->h * data->orig_scale[1] - valuey) / cage->h;
		}
	}
	else if (widget->highlighted_part == WIDGET_RECT_TRANSFORM_INTERSECT_SCALEY_UP) {
		widget->offset[1] = data->orig_offset[1] + valuey / 2.0;

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
			widget->offset[0] = orig_ofx;
			widget->offset[1] = orig_ofy;
		}
	}
	else {
		if (cage->scale[0] < WIDGET_RECT_MIN_WIDTH / cage->w) {
			cage->scale[0] = WIDGET_RECT_MIN_WIDTH / cage->w;
			widget->offset[0] = orig_ofx;
		}
		if (cage->scale[1] < WIDGET_RECT_MIN_WIDTH / cage->h) {
			cage->scale[1] = WIDGET_RECT_MIN_WIDTH / cage->h;
			widget->offset[1] = orig_ofy;
		}
	}

	if (widget->props[RECT_TRANSFORM_SLOT_OFFSET]) {
		PointerRNA ptr = widget->ptr[RECT_TRANSFORM_SLOT_OFFSET];
		PropertyRNA *prop = widget->props[RECT_TRANSFORM_SLOT_OFFSET];

		RNA_property_float_set_array(&ptr, prop, widget->offset);
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
	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_PASS_THROUGH;
}

static void widget_rect_transform_bind_to_prop(wmWidget *widget, const int slot)
{
	RectTransformWidget *cage = (RectTransformWidget *)widget;

	if (slot == RECT_TRANSFORM_SLOT_OFFSET)
		widget_rect_transform_get_property(widget, RECT_TRANSFORM_SLOT_OFFSET, widget->offset);
	if (slot == RECT_TRANSFORM_SLOT_SCALE)
		widget_rect_transform_get_property(widget, RECT_TRANSFORM_SLOT_SCALE, cage->scale);
}

static void widget_rect_transform_cancel(bContext *C, wmWidget *widget)
{
	RectTransformWidget *cage = (RectTransformWidget *)widget;
	RectTransformInteraction *data = (RectTransformInteraction *)widget->interaction_data;

	/* reset properties */
	if (widget->props[RECT_TRANSFORM_SLOT_OFFSET]) {
		PointerRNA ptr = widget->ptr[RECT_TRANSFORM_SLOT_OFFSET];
		PropertyRNA *prop = widget->props[RECT_TRANSFORM_SLOT_OFFSET];

		RNA_property_float_set_array(&ptr, prop, data->orig_offset);
		RNA_property_update(C, &ptr, prop);
	}
	if (widget->props[RECT_TRANSFORM_SLOT_SCALE]) {
		PointerRNA ptr = widget->ptr[RECT_TRANSFORM_SLOT_SCALE];
		PropertyRNA *prop = widget->props[RECT_TRANSFORM_SLOT_SCALE];

		if (cage->style & WIDGET_RECT_TRANSFORM_STYLE_SCALE_UNIFORM){
			RNA_property_float_set(&ptr, prop, data->orig_scale[0]);
		}
		else {
			RNA_property_float_set_array(&ptr, prop, data->orig_scale);
		}
		RNA_property_update(C, &ptr, prop);
	}
}

/** \name Cage Widget API
 *
 * \{ */

wmWidget *WIDGET_rect_transform_new(
        wmWidgetGroup *wgroup, const char *name, const int style,
        const float width, const float height)
{
	RectTransformWidget *cage = OBJECT_GUARDED_NEW_CALLOC(RectTransformWidget, wgroup, name, 2);

	cage->draw          = widget_rect_transform_draw;
	cage->invoke        = widget_rect_transform_invoke;
	cage->bind_to_prop  = widget_rect_transform_bind_to_prop;
	cage->handler       = widget_rect_transform_handler;
	cage->intersect     = widget_rect_transform_intersect;
	cage->cancel        = widget_rect_transform_cancel;
	cage->get_cursor    = widget_rect_transform_get_cursor;
	cage->flag         |= WM_WIDGET_DRAW_ACTIVE;
	cage->scale[0]      = cage->scale[1] = 1.0f;
	cage->style         = style;
	cage->w             = width;
	cage->h             = height;

	return (wmWidget *)cage;
}

/** \} */ // Cage Widget API


/* -------------------------------------------------------------------- */

void fix_linking_widget_cage(void)
{
	(void)0;
}
