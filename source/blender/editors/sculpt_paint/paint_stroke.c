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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins, Tom Musgrove.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_paint.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLI_math.h"


#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"
#include "sculpt_intern.h" // XXX, for expedience in getting this working, refactor later (or this just shows that this needs unification)

#include "BKE_image.h"

#include <float.h>
#include <math.h>

typedef struct PaintStroke {
	void *mode_data;
	void *smooth_stroke_cursor;
	wmTimer *timer;

	/* Cached values */
	ViewContext vc;
	bglMats mats;
	Brush *brush;

	float last_mouse_position[2];

	/* Set whether any stroke step has yet occured
	   e.g. in sculpt mode, stroke doesn't start until cursor
	   passes over the mesh */
	int stroke_started;

	StrokeGetLocation get_location;
	StrokeTestStart test_start;
	StrokeUpdateStep update_step;
	StrokeDone done;
} PaintStroke;

/*** Cursor ***/
static void paint_draw_smooth_stroke(bContext *C, int x, int y, void *customdata) 
{
	Brush *brush = paint_brush(paint_get_active(CTX_data_scene(C)));
	PaintStroke *stroke = customdata;

	glColor4ubv(paint_get_active(CTX_data_scene(C))->paint_cursor_col);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);

	if(stroke && brush && (brush->flag & BRUSH_SMOOTH_STROKE)) {
		ARegion *ar = CTX_wm_region(C);
		sdrawline(x, y, (int)stroke->last_mouse_position[0] - ar->winrct.xmin,
			  (int)stroke->last_mouse_position[1] - ar->winrct.ymin);
	}

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}



#define GRID_WIDTH   8
#define GRID_LENGTH  8

#define W (0xFFFFFFFF)
#define G (0x00888888)
#define E (0xE1E1E1E1)
#define C (0xC3C3C3C3)
#define O (0xB4B4B4B4)
#define Q (0xA9A9A9A9)

static unsigned grid_texture0[256] =
{
   W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,G,G,G,G,G,G,G,G,G,G,G,G,G,G,W,
   W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,
};

static unsigned grid_texture1[64] =
{
   C,C,C,C,C,C,C,C,
   C,G,G,G,G,G,G,C,
   C,G,G,G,G,G,G,C,
   C,G,G,G,G,G,G,C,
   C,G,G,G,G,G,G,C,
   C,G,G,G,G,G,G,C,
   C,G,G,G,G,G,G,C,
   C,C,C,C,C,C,C,C,
};

static unsigned grid_texture2[16] =
{
   O,O,O,O,
   O,G,G,O,
   O,G,G,O,
   O,O,O,O,
};

static unsigned grid_texture3[4] =
{
   Q,Q,
   Q,Q,
};

static unsigned grid_texture4[1] =
{
   Q,
};

#undef W
#undef G
#undef E
#undef C
#undef O
#undef Q

static void load_grid(Brush* brush)
{
	static int loaded = 0;

	if (!loaded) {
		//GLfloat largest_supported_anisotropy;

		glGenTextures(1, (GLint*)(&(brush->overlay_texture)));
		glBindTexture(GL_TEXTURE_2D, brush->overlay_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, grid_texture0);
		glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB,  8,  8, 0, GL_RGBA, GL_UNSIGNED_BYTE, grid_texture1);
		glTexImage2D(GL_TEXTURE_2D, 2, GL_RGB,  4,  4, 0, GL_RGBA, GL_UNSIGNED_BYTE, grid_texture2);
		glTexImage2D(GL_TEXTURE_2D, 3, GL_RGB,  2,  2, 0, GL_RGBA, GL_UNSIGNED_BYTE, grid_texture3);
		glTexImage2D(GL_TEXTURE_2D, 4, GL_RGB,  1,  1, 0, GL_RGBA, GL_UNSIGNED_BYTE, grid_texture4);
		glEnable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		//glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest_supported_anisotropy);
		//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest_supported_anisotropy);

		loaded = 1;
	}
}

extern float get_tex_pixel(Brush* br, float u, float v);

static void load_tex(Brush* brush, ViewContext* vc)
{
	float* buffer;
	float* p;

	int width, height;
	float x, y;
	int i, j;
	float xlim, ylim;

	if (brush->overlay_texture) glDeleteTextures(1, (GLint*)(&brush->overlay_texture));

	width = height = 256;

	p = buffer = MEM_mallocN(sizeof(float)*width*height, "load_tex");

	xlim = brush->size / (float)vc->ar->winx  *  width;
	ylim = brush->size / (float)vc->ar->winy  *  height;

	for (j = 0, y = 0; j < height; j++, y = j/ylim) {
		for (i = 0, x = 0; i < width; i++, x = i/xlim) {

			// largely duplicated from tex_strength

			const float rotation = -brush->mtex.rot;
			float diameter = brush->size;

			x = (float)i/width;
			y = (float)j/height;

			x -= 0.5f;
			y -= 0.5f;
			
			x *= vc->ar->winx / diameter;
			y *= vc->ar->winy / diameter;

			/* it is probably worth optimizing for those cases where 
			   the texture is not rotated by skipping the calls to
			   atan2, sqrtf, sin, and cos. */
			if (rotation > 0.001 || rotation < -0.001) {
				const float angle    = atan2(y, x) + rotation;
				const float flen     = sqrtf(x*x + y*y);

				x = flen * cos(angle);
				y = flen * sin(angle);
			}

			x *= brush->mtex.size[0];
			y *= brush->mtex.size[1];

			x += brush->mtex.ofs[0];
			y += brush->mtex.ofs[1];

			*p = get_tex_pixel(brush, x, y);

			p++;
		}
	}

	glGenTextures(1, (GLint*)(&(brush->overlay_texture)));

	glBindTexture(GL_TEXTURE_2D, brush->overlay_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, width, height, 0, GL_LUMINANCE, GL_FLOAT, buffer);

	glEnable(GL_TEXTURE_2D);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	MEM_freeN(buffer);
}

/* Convert a point in model coordinates to 2D screen coordinates. */
// XXX duplicated from sculpt.c, deal with this later.
static void projectf(bglMats *mats, const float v[3], float p[2])
{
	double ux, uy, uz;

	gluProject(v[0],v[1],v[2], mats->modelview, mats->projection,
		   (GLint *)mats->viewport, &ux, &uy, &uz);
	p[0]= ux;
	p[1]= uy;
}

static int project_brush_radius(RegionView3D* rv3d, float radius, float location[3], bglMats* mats)
{
	float view[3], nonortho[3], ortho[3], offset[3], p1[2], p2[2];

	viewvector(rv3d, location, view);

	// create a vector that is not orthogonal to view

	if (fabsf(view[0]) < 0.1) {
		nonortho[0] = view[0] + 1;
		nonortho[1] = view[1];
		nonortho[2] = view[2];
	}
	else if (fabsf(view[1]) < 0.1) {
		nonortho[0] = view[0];
		nonortho[1] = view[1] + 1;
		nonortho[2] = view[2];
	}
	else {
		nonortho[0] = view[0];
		nonortho[1] = view[1];
		nonortho[2] = view[2] + 1;
	}

	// get a vector in the plane of the view
	cross_v3_v3v3(ortho, nonortho, view);
	normalize_v3(ortho);

	// make a point on the surface of the brush tagent to the view
	mul_v3_fl(ortho, radius);
	add_v3_v3v3(offset, location, ortho);

	// project the center of the brush, and the tagent point to the view onto the screen
	projectf(mats, location, p1);
	projectf(mats, offset, p2);

	// the distance between these points is the size of the projected brush in pixels
	return len_v2v2(p1, p2);
}

static void sculpt_set_brush_radius(bContext* C, Brush *brush, int value)
{
	
	brush->size = value;
	//printf("resize radius \n");
	//U.sculpt_paint_pixel_radius = value;

	//PointerRNA brushptr;
	//PropertyRNA *size;

	///* brush.size = value */

	//RNA_id_pointer_create(&brush->id, &brushptr);

	//size= RNA_struct_find_property(&brushptr, "size");
	//RNA_property_int_set(&brushptr, size, value);

	//WM_event_add_notifier(C, NC_BRUSH|NA_EDITED, brush);
}

static void sculpt_set_brush_unprojected_radius(bContext* C, Brush *brush, float value)
{
	
	brush->unprojected_radius = value;
	//PointerRNA brushptr;
	//PropertyRNA *unprojected_radius;

	///* brush.unprojected_radius = value */

	//RNA_id_pointer_create(&brush->id, &brushptr);

	//unprojected_radius= RNA_struct_find_property(&brushptr, "unprojected_radius");
	//RNA_property_float_set(&brushptr, unprojected_radius, value);

	//WM_event_add_notifier(C, NC_BRUSH|NA_EDITED, brush);
}

static int sculpt_get_brush_geometry(bContext* C, int x, int y, int* pixel_radius, float location[3], float modelview[16], float projection[16], int viewport[4])
{
	struct PaintStroke *stroke;
	float window[2];
	int hit;

	stroke = paint_stroke_new(C, NULL, NULL, NULL, NULL);

	window[0] = x + stroke->vc.ar->winrct.xmin;
	window[1] = y + stroke->vc.ar->winrct.ymin;

	if (stroke->vc.obact->sculpt && stroke->vc.obact->sculpt->pbvh && sculpt_stroke_get_location(C, stroke, location, window)) {
		*pixel_radius = project_brush_radius(stroke->vc.rv3d, stroke->brush->unprojected_radius, location, &stroke->mats);

		if (*pixel_radius == 0) {
			*pixel_radius = stroke->brush->size;
		}

		mul_m4_v3(stroke->vc.obact->sculpt->ob->obmat, location);

		memcpy(modelview, stroke->vc.rv3d->viewmat, sizeof(float[16]));
		memcpy(projection, stroke->vc.rv3d->winmat, sizeof(float[16]));
		memcpy(viewport, stroke->mats.viewport, sizeof(int[4]));
		hit = 1;
	}
	else {
		Sculpt* sd    = CTX_data_tool_settings(C)->sculpt;
		Brush*  brush = paint_brush(&sd->paint);

		*pixel_radius = brush->size;
		hit = 0;
	}

	paint_stroke_free(stroke);

	return hit;
}

// XXX duplicated from sculpt.c
static float unproject_brush_radius(Object *ob, ViewContext *vc, float center[3], float offset)
{
	float delta[3], scale, loc[3];

	mul_v3_m4v3(loc, ob->obmat, center);

	initgrabz(vc->rv3d, loc[0], loc[1], loc[2]);
	window_to_3d_delta(vc->ar, delta, offset, 0);

	scale= fabsf(mat4_to_scale(ob->obmat));
	scale= (scale == 0.0f)? 1.0f: scale;

	return len_v3(delta)/scale;
}

// XXX paint cursor now does a lot of the same work that is needed during a sculpt stroke
// problem: all this stuff was not intended to be used at this point, so things feel a
// bit hacked.  I've put lots of stuff in Brush that probably better goes in Paint
// Functions should be refactored so that they can be used between sculpt.c and
// paint_stroke.c clearly and optimally and the lines of communication between the
// two modules should be more clearly defined.
static void paint_draw_cursor(bContext *C, int x, int y, void *customdata)
{
	ViewContext vc;

	view3d_set_viewcontext(C, &vc);

	if (vc.obact->sculpt) {
		Paint *paint = paint_get_active(CTX_data_scene(C));
		Brush *brush = paint_brush(paint);

		int pixel_radius, viewport[4];
		float location[3], modelview[16], projection[16];

		int hit;

		/* keep track of mouse movement angle so rack can start at a sensible angle */
		int dx = brush->last_x - x;
		int dy = brush->last_y - y;

		if (dx*dx + dy*dy > 100) {
			/* only update if distance traveled is more than 10 pixels */
			brush->last_angle = atan2(dx, dy);
			brush->last_x = x;
			brush->last_y = y;
		} /* else, do not update last_x and last_y so that the distance can accumulate */

		if(!(brush->flag & BRUSH_LOCK_SIZE) && !(paint->flags & PAINT_SHOW_BRUSH)) 
			return;

		hit = sculpt_get_brush_geometry(C, x, y, &pixel_radius, location, modelview, projection, viewport);

		if (brush->flag & BRUSH_LOCK_SIZE) sculpt_set_brush_radius(C, brush, pixel_radius);

		if (hit) {
			float unprojected_radius;
			int flip;
			int sign;
			float visual_strength = brush->alpha * brush->alpha;
			
			const float min_alpha = 0.20f;
			const float max_alpha = 0.80f;
			float* col;
			float  alpha;

			// XXX duplicated from brush_strength & paint_stroke_add_step, refactor later
			//wmEvent* event = CTX_wm_window(C)->eventstate;

			// XXX: no way currently to know state of pen flip or invert key modifier without starting a stroke
			flip = 1;

			if ( brush->draw_pressure && brush->flag & BRUSH_ALPHA_PRESSURE)
				visual_strength *= brush->pressure_value;

			// remove effect of strength multiplier
			visual_strength /= brush->strength_multiplier;

			// don't show effect of strength past the soft limit
			if (visual_strength > 1) visual_strength = 1;

			if (brush->draw_anchored) {
				unprojected_radius = unproject_brush_radius(CTX_data_active_object(C), &vc, location, brush->anchored_size);
			}
			else {
				if (brush->flag & BRUSH_ANCHORED)
					unprojected_radius = unproject_brush_radius(CTX_data_active_object(C), &vc, location, 8);
				else
					unprojected_radius = unproject_brush_radius(CTX_data_active_object(C), &vc, location, brush->size);
			}

			if (brush->draw_pressure && brush->flag & BRUSH_SIZE_PRESSURE)
				unprojected_radius *= brush->pressure_value;

			if (!(brush->flag & BRUSH_LOCK_SIZE)) 
				sculpt_set_brush_unprojected_radius(C, brush, unprojected_radius);

			if(!(paint->flags & PAINT_SHOW_BRUSH))
				return;

			sign = flip * ((brush->flag & BRUSH_DIR_IN)? -1 : 1);

			if (sign < 0 && ELEM4(brush->sculpt_tool, SCULPT_TOOL_DRAW, SCULPT_TOOL_INFLATE, SCULPT_TOOL_CLAY, SCULPT_TOOL_PINCH))
				col = brush->sub_col;
			else
				col = brush->add_col;

			alpha = (paint->flags & PAINT_SHOW_BRUSH_ON_SURFACE) ? min_alpha + (visual_strength*(max_alpha-min_alpha)) : 0.50f;

			if (paint->flags & PAINT_SHOW_BRUSH_ON_SURFACE) {
				const float max_thickness= 0.16;
				const float min_thickness= 0.06;
				const float thickness=     1.0 - min_thickness - visual_strength*max_thickness;
				const float inner_radius=  brush->draw_anchored ? unprojected_radius                  : unprojected_radius*thickness;
				const float outer_radius=  brush->draw_anchored ? 1.0f/thickness * unprojected_radius : unprojected_radius;

				GLUquadric* sphere;

				glPushAttrib(
					GL_COLOR_BUFFER_BIT|
					GL_CURRENT_BIT|
					GL_DEPTH_BUFFER_BIT|
					GL_ENABLE_BIT|
					GL_LINE_BIT|
					GL_POLYGON_BIT|
					GL_STENCIL_BUFFER_BIT|
					GL_TRANSFORM_BIT|
					GL_VIEWPORT_BIT|
					GL_TEXTURE_BIT);

				glColor4f(col[0], col[1], col[2], alpha);

				glEnable(GL_BLEND);

				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				glLoadMatrixf(modelview);

				if (brush->draw_anchored)
					glTranslatef(brush->anchored_location[0], brush->anchored_location[1], brush->anchored_location[2]);
				else
					glTranslatef(location[0], location[1], location[2]);

				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glLoadMatrixf(projection);

				glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glDepthMask(GL_FALSE);

				glDisable(GL_CULL_FACE);

				glEnable(GL_DEPTH_TEST);

				glClearStencil(0);
				glClear(GL_STENCIL_BUFFER_BIT);
				glEnable(GL_STENCIL_TEST);

				glStencilFunc(GL_ALWAYS, 3, 0xFF);
				glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);

				sphere = gluNewQuadric();

				gluSphere(sphere, outer_radius, 40, 40);

				glStencilFunc(GL_ALWAYS, 1, 0xFF);
				glStencilOp(GL_KEEP, GL_DECR, GL_KEEP);

				if (brush->size >= 8)
					gluSphere(sphere, inner_radius, 40, 40);

				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

				glStencilFunc(GL_EQUAL, 1, 0xFF);
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

				gluSphere(sphere, outer_radius, 40, 40);

				glStencilFunc(GL_EQUAL, 3, 0xFF);
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

				gluSphere(sphere, outer_radius, 40, 40);

				gluDeleteQuadric(sphere);

				glPopMatrix();

				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();

				glPopAttrib();
			}
			else {
				glPushAttrib(
					GL_COLOR_BUFFER_BIT|
					GL_CURRENT_BIT|
					GL_DEPTH_BUFFER_BIT|
					GL_ENABLE_BIT|
					GL_LINE_BIT|
					GL_POLYGON_BIT|
					GL_STENCIL_BUFFER_BIT|
					GL_TRANSFORM_BIT|
					GL_VIEWPORT_BIT|
					GL_TEXTURE_BIT);

				glColor4f(col[0], col[1], col[2], alpha);

				glEnable(GL_BLEND);

				glEnable(GL_LINE_SMOOTH);

				if (brush->draw_anchored) {
					glTranslatef(brush->anchored_initial_mouse[0] - vc.ar->winrct.xmin, brush->anchored_initial_mouse[1] - vc.ar->winrct.ymin, 0.0f);
					glutil_draw_lined_arc(0.0, M_PI*2.0, brush->anchored_size, 40);
					glTranslatef(-brush->anchored_initial_mouse[0], -brush->anchored_initial_mouse[1], 0.0f);
				}
				else {
					glTranslatef((float)x, (float)y, 0.0f);
					glutil_draw_lined_arc(0.0, M_PI*2.0, brush->size, 40);
					glTranslatef(-(float)x, -(float)y, 0.0f);
				}

				glPopAttrib();
			}

			if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_TILED && brush->flag & BRUSH_TEXTURE_OVERLAY) {
				const float diameter = 2*brush->size;

				glPushAttrib(
					GL_COLOR_BUFFER_BIT|
					GL_CURRENT_BIT|
					GL_DEPTH_BUFFER_BIT|
					GL_ENABLE_BIT|
					GL_LINE_BIT|
					GL_POLYGON_BIT|
					GL_STENCIL_BUFFER_BIT|
					GL_TRANSFORM_BIT|
					GL_VIEWPORT_BIT|
					GL_TEXTURE_BIT);

				glColor4f(col[0], col[1], col[2], alpha);

				glEnable(GL_BLEND);

				load_tex(brush, &vc);

				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, brush->overlay_texture);

				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
				glDepthMask(GL_FALSE);
				glDepthFunc(GL_ALWAYS);

				glMatrixMode(GL_TEXTURE);
				glLoadIdentity();

				glColor4f(1.0f, 1.0f, 1.0f, brush->texture_overlay_alpha / 100.0f);
				glBegin(GL_QUADS);
					glTexCoord2f(0, 0);
					glVertex2f(0, 0);

					glTexCoord2f(1, 0);
					glVertex2f(viewport[2], 0);

					glTexCoord2f(1, 1);
					glVertex2f(viewport[2], viewport[3]);

					glTexCoord2f(0, 1);
					glVertex2f(0, viewport[3]);
				glEnd();

				glLoadIdentity();

				glMatrixMode(GL_MODELVIEW);

				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

				glDisable(GL_TEXTURE_2D);

				glDepthMask(GL_TRUE);
				glDepthFunc(GL_LEQUAL);

				glPopAttrib();
			}
		}
	}
	else {
		Paint *paint = paint_get_active(CTX_data_scene(C));
		Brush *brush = paint_brush(paint);

		if(!(paint->flags & PAINT_SHOW_BRUSH))
			return;

		glColor4ubv(paint_get_active(CTX_data_scene(C))->paint_cursor_col);
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);

		glTranslatef((float)x, (float)y, 0.0f);
		glutil_draw_lined_arc(0.0, M_PI*2.0, brush->size, 40);
		glTranslatef((float)-x, (float)-y, 0.0f);

		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);
	}
}

/* Put the location of the next stroke dot into the stroke RNA and apply it to the mesh */
static void paint_brush_stroke_add_step(bContext *C, wmOperator *op, wmEvent *event, float mouse[2])
{
	PointerRNA itemptr;

	float location[3];

	float pressure;
	int   pen_flip;

	PaintStroke *stroke = op->customdata;

	/* XXX: can remove the if statement once all modes have this */
	if(stroke->get_location)
		stroke->get_location(C, stroke, location, mouse);
	else
		zero_v3(location);

	/* Tablet */
	if(event->custom == EVT_DATA_TABLET) {
		wmTabletData *wmtab= event->customdata;

		pressure = (wmtab->Active != EVT_TABLET_NONE) ? pressure= wmtab->Pressure : 1;
		pen_flip = (wmtab->Active == EVT_TABLET_ERASER);
	}
	else {
		pressure = 1;
		pen_flip = 0;
	}

	/* Add to stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	RNA_float_set_array(&itemptr, "location",     location);
	RNA_float_set_array(&itemptr, "mouse",        mouse);
	RNA_boolean_set    (&itemptr, "pen_flip",     pen_flip);
	RNA_float_set      (&itemptr, "pressure", pressure);

	stroke->last_mouse_position[0] = mouse[0];
	stroke->last_mouse_position[1] = mouse[1];

	stroke->update_step(C, stroke, &itemptr);
}

/* Returns zero if no sculpt changes should be made, non-zero otherwise */
static int paint_smooth_stroke(PaintStroke *stroke, float output[2], wmEvent *event)
{
	output[0] = event->x;
	output[1] = event->y;

	if(stroke->brush->flag & BRUSH_SMOOTH_STROKE && stroke->brush->sculpt_tool != SCULPT_TOOL_GRAB) {
		float u = stroke->brush->smooth_stroke_factor, v = 1.0 - u;
		float dx = stroke->last_mouse_position[0] - event->x, dy = stroke->last_mouse_position[1] - event->y;

		/* If the mouse is moving within the radius of the last move,
		   don't update the mouse position. This allows sharp turns. */
		if(dx*dx + dy*dy < stroke->brush->smooth_stroke_radius * stroke->brush->smooth_stroke_radius)
			return 0;

		output[0] = event->x * v + stroke->last_mouse_position[0] * u;
		output[1] = event->y * v + stroke->last_mouse_position[1] * u;
	}

	return 1;
}

/* Returns zero if the stroke dots should not be spaced, non-zero otherwise */
static int paint_space_stroke_enabled(Brush *br)
{
	return (br->flag & BRUSH_SPACE) &&
	       !(br->flag & BRUSH_ANCHORED) &&
	       !ELEM5(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_SNAKE_HOOK, SCULPT_TOOL_SMOOTH);
}

/* For brushes with stroke spacing enabled, moves mouse in steps
   towards the final mouse location. */
static int paint_space_stroke(bContext *C, wmOperator *op, wmEvent *event, const float final_mouse[2])
{
	PaintStroke *stroke = op->customdata;
	int cnt = 0;

	if(paint_space_stroke_enabled(stroke->brush)) {
		float mouse[2];
		float vec[2];
		float length, scale;

		copy_v2_v2(mouse, stroke->last_mouse_position);
		sub_v2_v2v2(vec, final_mouse, mouse);

		length = len_v2(vec);

		if(length > FLT_EPSILON) {
			int steps;
			int i;
			float pressure = 1;

			// XXX duplicate code
			if(event->custom == EVT_DATA_TABLET) {
				wmTabletData *wmtab= event->customdata;
				if(wmtab->Active != EVT_TABLET_NONE)
					pressure = stroke->brush->flag & BRUSH_SIZE_PRESSURE ? wmtab->Pressure : 1;
			}

			scale = (stroke->brush->size*pressure*stroke->brush->spacing/50.0f) / length;
			mul_v2_fl(vec, scale);

			steps = (int)(1.0f / scale);

			for(i = 0; i < steps; ++i, ++cnt) {
				add_v2_v2(mouse, vec);
				paint_brush_stroke_add_step(C, op, event, mouse);
			}
		}
	}

	return cnt;
}

/**** Public API ****/

PaintStroke *paint_stroke_new(bContext *C,
				  StrokeGetLocation get_location,
				  StrokeTestStart test_start,
				  StrokeUpdateStep update_step,
				  StrokeDone done)
{
	PaintStroke *stroke = MEM_callocN(sizeof(PaintStroke), "PaintStroke");

	stroke->brush = paint_brush(paint_get_active(CTX_data_scene(C)));
	view3d_set_viewcontext(C, &stroke->vc);
	view3d_get_transformation(stroke->vc.ar, stroke->vc.rv3d, stroke->vc.obact, &stroke->mats);

	stroke->get_location = get_location;
	stroke->test_start = test_start;
	stroke->update_step = update_step;
	stroke->done = done;

	return stroke;
}

void paint_stroke_free(PaintStroke *stroke)
{
	MEM_freeN(stroke);
}

int paint_stroke_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	PaintStroke *stroke = op->customdata;
	float mouse[2];
	int first= 0;

	if(!stroke->stroke_started) {
		stroke->last_mouse_position[0] = event->x;
		stroke->last_mouse_position[1] = event->y;
		stroke->stroke_started = stroke->test_start(C, op, event);

		if(stroke->stroke_started) {
			stroke->smooth_stroke_cursor =
				WM_paint_cursor_activate(CTX_wm_manager(C), paint_poll, paint_draw_smooth_stroke, stroke);

			if(stroke->brush->flag & BRUSH_AIRBRUSH)
				stroke->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, stroke->brush->rate);
		}

		first= 1;
		//ED_region_tag_redraw(ar);
	}

	/* TODO: fix hardcoded events here */
	if(event->type == LEFTMOUSE && event->val == KM_RELEASE) {
		/* exit stroke, free data */
		if(stroke->smooth_stroke_cursor)
			WM_paint_cursor_end(CTX_wm_manager(C), stroke->smooth_stroke_cursor);

		if(stroke->timer)
			WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), stroke->timer);

		stroke->done(C, stroke);
		MEM_freeN(stroke);
		return OPERATOR_FINISHED;
	}
	else if(first || event->type == MOUSEMOVE || (event->type == TIMER && (event->customdata == stroke->timer))) {
		if(stroke->stroke_started) {
			if(paint_smooth_stroke(stroke, mouse, event)) {
				if(paint_space_stroke_enabled(stroke->brush)) {
					if(!paint_space_stroke(C, op, event, mouse)) {
						//ED_region_tag_redraw(ar);
					}
				}
				else
					paint_brush_stroke_add_step(C, op, event, mouse);
			}
			else
				;//ED_region_tag_redraw(ar);
		}
	}

	/* we want the stroke to have the first daub at the start location instead of waiting till we have moved the space distance */
	if(first &&
	   stroke->stroke_started &&
	   paint_space_stroke_enabled(stroke->brush) &&
	   !(stroke->brush->flag & BRUSH_ANCHORED) &&
	   !(stroke->brush->flag & BRUSH_SMOOTH_STROKE))
	{
		paint_brush_stroke_add_step(C, op, event, mouse);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

int paint_stroke_exec(bContext *C, wmOperator *op)
{
	PaintStroke *stroke = op->customdata;

	RNA_BEGIN(op->ptr, itemptr, "stroke") {
		stroke->update_step(C, stroke, &itemptr);
	}
	RNA_END;

	MEM_freeN(stroke);
	op->customdata = NULL;

	return OPERATOR_FINISHED;
}

ViewContext *paint_stroke_view_context(PaintStroke *stroke)
{
	return &stroke->vc;
}

void *paint_stroke_mode_data(struct PaintStroke *stroke)
{
	return stroke->mode_data;
}

void paint_stroke_set_mode_data(PaintStroke *stroke, void *mode_data)
{
	stroke->mode_data = mode_data;
}

int paint_poll(bContext *C)
{
	Paint *p = paint_get_active(CTX_data_scene(C));
	Object *ob = CTX_data_active_object(C);

	return p && ob && paint_brush(p) &&
		CTX_wm_area(C)->spacetype == SPACE_VIEW3D &&
		CTX_wm_region(C)->regiontype == RGN_TYPE_WINDOW;
}

void paint_cursor_start(bContext *C, int (*poll)(bContext *C))
{
	Paint *p = paint_get_active(CTX_data_scene(C));

	if(p && !p->paint_cursor)
		p->paint_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), poll, paint_draw_cursor, NULL);
}

