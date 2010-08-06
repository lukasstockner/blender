/*
 * $Id$
 *
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): None
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_paint.h"

#include "BLI_math.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "UI_resources.h"

#include "paint_intern.h"

#include <math.h>

int paint_sample_overlay(PaintStroke *stroke, float col[3], float co[2])
{
	ViewContext *vc = paint_stroke_view_context(stroke);
	PaintOverlay *overlay = &vc->scene->toolsettings->paint_overlay;

	col[0] = col[1] = col[2] = col[3] = 0;

	if(overlay->use && overlay->img) {
		ImBuf *ibuf = BKE_image_get_ibuf(overlay->img, NULL);

		if(ibuf) {
			int x, y;
			int offset, trans[2];
			float uco[3], proj[2];

			paint_stroke_symmetry_unflip(stroke, uco, co);
			paint_stroke_project(stroke, uco, proj);


			paint_overlay_transform(overlay, vc->ar, ibuf, 
						trans, proj, 1, 1);
			x = trans[0];
			y = trans[1];
				
			if(x >= 0 && x < ibuf->x && y >= 0 && y < ibuf->y) {
				offset = y*ibuf->x + x;

				if(ibuf->rect) {
					char *ccol = ((char*)ibuf->rect) + offset*4;
				
					col[0] = ccol[0] / 255.0;
					col[1] = ccol[1] / 255.0;
					col[2] = ccol[2] / 255.0;
					col[3] = ccol[3] / 255.0;
				}
			}
		}

		return 1;
	}

	return 0;
}

static int paint_overlay_poll(bContext *C)
{	
	PaintOverlay *overlay = &CTX_data_tool_settings(C)->paint_overlay;

	if(vertex_paint_poll(C) || image_texture_paint_poll(C))
		return overlay->img && overlay->use;

	return 0;
}

void ED_paint_update_overlay(PaintOverlay *overlay)
{
	if(overlay->gltex) {
		glDeleteTextures(1, &overlay->gltex);
		overlay->gltex = 0;
	}
}

void ED_paint_overlay_draw(const bContext *C, ARegion *ar)
{
	PaintOverlay *overlay = &CTX_data_tool_settings(C)->paint_overlay;
	ImBuf *ibuf;
	int center[2];
	int sx, sy;

	if(!paint_overlay_poll((bContext*)C))
		return;

	ibuf = BKE_image_get_ibuf(overlay->img, NULL);

	if(!ibuf)
		return;

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	if(!overlay->gltex) {
		/* update gl texture */

		/* doing this manually because it looks like the GPU
		   Image stuff is customized on mesh tex? */

		glGenTextures(1, (GLuint*)&overlay->gltex);
		glBindTexture(GL_TEXTURE_2D, overlay->gltex);

		if ((ibuf->rect==NULL) && ibuf->rect_float)
			IMB_rect_from_float(ibuf);

		{
			char *bc = (char*)ibuf->rect;
			char transp[3] = {overlay->transp_col[0] * 255,
					  overlay->transp_col[1] * 255,
					  overlay->transp_col[2] * 255};
			int i;

			for(i = 0; i < ibuf->y*ibuf->x; ++i, bc+=4) {
				float d[3] = {fabs(bc[2]-transp[0]),
					      fabs(bc[1]-transp[1]),
					      fabs(bc[0]-transp[2])};
			       
				if(d[0] < overlay->transp_tol &&
				   d[1] < overlay->transp_tol &&
				   d[2] < overlay->transp_tol)
					bc[3] = 0;
				else
					bc[3] = 255;
			}
		}

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0,  GL_RGBA,  ibuf->x, ibuf->y,
			     0, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
	}

	glBindTexture(GL_TEXTURE_2D, overlay->gltex);
	glColor4f(1, 1, 1, 0.5);

	glPushMatrix();
	center[0] = ar->winx/2 + overlay->offset[0];
	center[1] = ar->winy/2 + overlay->offset[1];
	sx = overlay->size[0] / 2;
	sy = overlay->size[1] / 2;

	glTranslatef(center[0], center[1], 0);
	glRotatef(overlay->angle * 180/M_PI, 0, 0, 1);

	/* draw textured quad */
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(-sx, -sy);
	glTexCoord2f(1, 0);
	glVertex2f(+sx, -sy);
	glTexCoord2f(1, 1);
	glVertex2f(+sx, +sy);
	glTexCoord2f(0, 1);
	glVertex2f(-sx, +sy);
	glEnd();
	glPopMatrix();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}


/* convert screen-space coords to ibuf coords */
void paint_overlay_transform(PaintOverlay *overlay, ARegion *ar, ImBuf *ibuf,
			     int out[2], float vec[2], int scale, int rotate)
{
	float center[2], size[2], org[2], t1[2], t2[2];
	float sina, cosa;

	center[0] = ar->winx/2 + overlay->offset[0];
	center[1] = ar->winy/2 + overlay->offset[1];
	size[0] = overlay->size[0];
	size[1] = overlay->size[1];
	org[0] = center[0] - size[0]/2;
	org[1] = center[1] - size[1]/2;
	sina = sin(overlay->angle);
	cosa = cos(overlay->angle);

	/* make overlay center origin */
	sub_v2_v2v2(t1, vec, center);

	/* apply rotation */
	if(rotate) {
		t2[0] = cosa*t1[0] + sina*t1[1];
		t2[1] = -sina*t1[0] + cosa*t1[1];
	}
	else {
		out[0] = t1[0];
		out[1] = t1[1];
		t2[0] = t1[0];
		t2[1] = t1[1];
	}
	
	/* translation */
	if(scale) {
		out[0] = t2[0] + size[0]/2;
		out[1] = t2[1] + size[1]/2;

	/* scale */
		out[0] *= (ibuf->x / size[0]);
		out[1] *= (ibuf->y / size[1]);
	}
	else {
		out[0] = t2[0];
		out[1] = t2[1];
	}
}

typedef struct {
	int x, y;
	int offset[2];
	int size[2];
	float orig_angle, start_angle;

	ImBuf *ibuf;
	int ibuf_mouse[2];

	void *draw_cb_handle;
	struct ARegionType *draw_cb_type;
	/* data needed for manip draw */
	PaintManipAction action;
	int cur_mouse[2];
} VPaintManipData;

/* when rotating or scaling, draw hashed line to center */
static void paint_overlay_manip_draw(const bContext *C, ARegion *ar, void *data_v)
{
	PaintOverlay *overlay = &CTX_data_tool_settings(C)->paint_overlay;
	VPaintManipData *data = data_v;

	if(data->action != PAINT_MANIP_GRAB) {
		UI_ThemeColor(TH_WIRE);
		setlinestyle(3);

		glBegin(GL_LINES);
		glVertex2i(data->cur_mouse[0] - ar->winrct.xmin,
			   data->cur_mouse[1] - ar->winrct.ymin);
		glVertex2i(ar->winx/2 + overlay->offset[0],
			   ar->winy/2 + overlay->offset[1]);
		glEnd();

		setlinestyle(0);
	}
}

static int paint_overlay_manip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	PaintOverlay *overlay = &CTX_data_tool_settings(C)->paint_overlay;
	ARegion *ar = CTX_wm_region(C);
	VPaintManipData *data;
	float mouse[2] = {event->x - ar->winrct.xmin,
			  mouse[1] = event->y - ar->winrct.ymin};
	int angle_mouse[2];

	op->customdata = data = MEM_callocN(sizeof(VPaintManipData), "VPaintManipData");

	data->x = event->x;
	data->y = event->y;
	data->offset[0] = overlay->offset[0];
	data->offset[1] = overlay->offset[1];
	data->size[0] = overlay->size[0];
	data->size[1] = overlay->size[1];
	data->orig_angle = overlay->angle;
	data->ibuf = BKE_image_get_ibuf(overlay->img, NULL);
	data->action = RNA_enum_get(op->ptr, "action");
	data->cur_mouse[0] = event->x;
	data->cur_mouse[1] = event->y;

	paint_overlay_transform(overlay, ar, data->ibuf, data->ibuf_mouse, mouse, 0, 1);

	paint_overlay_transform(overlay, ar, data->ibuf, angle_mouse, mouse, 0, 0);
	data->start_angle = atan2(angle_mouse[0], angle_mouse[1]);
	
	data->draw_cb_type = ar->type;
	data->draw_cb_handle = ED_region_draw_cb_activate(ar->type,
							  paint_overlay_manip_draw,
							  data,
							  REGION_DRAW_POST_PIXEL);
	
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int paint_overlay_manip_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	PaintOverlay *overlay = &CTX_data_tool_settings(C)->paint_overlay;
	ARegion *ar = CTX_wm_region(C);
	float mouse[2] = {event->x - ar->winrct.xmin,
			  event->y - ar->winrct.ymin};
	int ibuf_mouse[2];
	int *co = overlay->offset;
	int *size = overlay->size;
	float *angle = &overlay->angle;
	VPaintManipData *data = op->customdata;
	int dx, dy;

	dx = event->x - data->x;
	dy = event->y - data->y;
	data->cur_mouse[0] = event->x;
	data->cur_mouse[1] = event->y;

	switch(data->action) {
	case PAINT_MANIP_GRAB:
		co[0] = data->offset[0] + dx;
		co[1] = data->offset[1] + dy;
		break;
	case PAINT_MANIP_SCALE:
		{
			float d[2];

			paint_overlay_transform(overlay, ar, data->ibuf, ibuf_mouse, mouse, 0, 1);

			d[0] = fabs(ibuf_mouse[0]) - fabs(data->ibuf_mouse[0]);
			d[1] = fabs(ibuf_mouse[1]) - fabs(data->ibuf_mouse[1]);

			size[0] = data->size[0] + d[0];
			size[1] = data->size[1] + d[1];
		}
		break;
	case PAINT_MANIP_ROTATE:
		paint_overlay_transform(overlay, ar, data->ibuf, ibuf_mouse, mouse, 0, 0);
		*angle = data->orig_angle + data->start_angle - atan2(ibuf_mouse[0], ibuf_mouse[1]);

		break;
	}

	ED_region_tag_redraw(CTX_wm_region(C));

	if(event->type == LEFTMOUSE) {
		ED_region_draw_cb_exit(data->draw_cb_type, data->draw_cb_handle);
		MEM_freeN(data);
		return OPERATOR_FINISHED;
	}
	else if(event->type == RIGHTMOUSE) {
		co[0] = data->offset[0];
		co[1] = data->offset[1];
		size[0] = data->size[0];
		size[1] = data->size[1];
		*angle = data->orig_angle;
		ED_region_draw_cb_exit(data->draw_cb_type, data->draw_cb_handle);
		MEM_freeN(data);
		return OPERATOR_CANCELLED;
	}
	else
		return OPERATOR_RUNNING_MODAL;
}

void PAINT_OT_overlay_manipulate(wmOperatorType *ot)
{
	static EnumPropertyItem action_items[]= {
		{PAINT_MANIP_GRAB, "GRAB", 0, "Grab", ""},
		{PAINT_MANIP_SCALE, "SCALE", 0, "Scale", ""},
		{PAINT_MANIP_ROTATE, "Rotate", 0, "Rotate", ""},

		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Paint Overlay Manipulate";
	ot->idname= "PAINT_OT_overlay_manipulate";
	
	/* api callbacks */
	ot->invoke= paint_overlay_manip_invoke;
	ot->modal= paint_overlay_manip_modal;
	ot->poll= paint_overlay_poll;
	
	/* flags */
	ot->flag= OPTYPE_BLOCKING;

	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "action", action_items, 0, "Action", "");
}
