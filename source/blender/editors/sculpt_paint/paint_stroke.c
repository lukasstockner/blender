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
#include "DNA_brush_types.h"

#include "RNA_access.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_paint.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLI_math.h"
#include "BLI_rand.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"
#include "sculpt_intern.h" // XXX, for expedience in getting this working, refactor later (or this just shows that this needs unification)


#include <float.h>
#include <math.h>

struct PaintStroke {
	void *mode_data;
	void *smooth_stroke_cursor;
	wmTimer *timer;

	/* Cached values */
	ViewContext vc;
	float project_mat[4][4];
	bglMats mats;
	Brush *brush;
	/* brush location (object space) */
	float location[3], symmetry_location[3];
	/* screen-space brush location */
	float mouse[2], initial_mouse[2], last_mouse_position[2];
	/* not always the same as brush_size() */
	int pixel_radius;
	/* tablet pressure, or 1 if using mouse */
	float pressure;
	/* 3d brush radius */
	float radius, radius_squared, initial_radius;
	/* previous location of updating rake rotation */
	float last_rake[2];
	/* this value is added to the brush's rotation in calculations */
	float rotation;
	/* mouse location used for texturing */
	float tex_mouse[2];
	/* symmetry */
	/* current symmetry pass (0-7) */
	int mirror_symmetry_pass;
	int radial_symmetry_pass;
	float symm_rot_mat[4][4];
	float symm_rot_mat_inv[4][4];
	/* decrease brush strength if symmetry overlaps */
	float feather;

	/* anything special that sculpt or other paint modes need
	   to do should go through these modifiers */
	float modifier_initial_radius_factor;
	int modifier_use_original_texture_coords;
	int modifier_use_original_location;

	/* Set whether any stroke step has yet occurred
	   e.g. in sculpt mode, stroke doesn't start until cursor
	   passes over the mesh */
	int stroke_started;
	/* 1 if this is the first paint dab, 0 otherwise */
	int first_dab;

	/* callbacks */
	StrokeGetLocation get_location;
	StrokeTestStart test_start;
	StrokeUpdateStep update_step;
	StrokeUpdateSymmetry update_symmetry;
	StrokeBrushAction brush_action;
	StrokeDone done;
};

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

#if 0

// grid texture for testing

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

static void load_grid()
{
	static GLuint overlay_texture;

	if (!overlay_texture) {
		//GLfloat largest_supported_anisotropy;

		glGenTextures(1, &overlay_texture);
		glBindTexture(GL_TEXTURE_2D, overlay_texture);
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
	}
}

#endif

extern float get_tex_pixel(Brush* br, float u, float v);

typedef struct Snapshot {
	float size[3];
	float ofs[3];
	float rot;
	int brush_size;
	int winx;
	int winy;
	int brush_map_mode;
	int curve_changed_timestamp;
} Snapshot;

static int same_snap(Snapshot* snap, Brush* brush, ViewContext* vc)
{
	MTex* mtex = &brush->mtex;

	return 
		(mtex->tex &&
		    mtex->ofs[0] == snap->ofs[0] &&
		    mtex->ofs[1] == snap->ofs[1] &&
		    mtex->ofs[2] == snap->ofs[2] &&
		    mtex->size[0] == snap->size[0] &&
		    mtex->size[1] == snap->size[1] &&
		    mtex->size[2] == snap->size[2] &&
		    mtex->rot == snap->rot) &&
		((mtex->brush_map_mode == MTEX_MAP_MODE_FIXED && brush_size(brush) <= snap->brush_size) || (brush_size(brush) == snap->brush_size)) && // make brush smaller shouldn't cause a resample
		mtex->brush_map_mode == snap->brush_map_mode &&
		vc->ar->winx == snap->winx &&
		vc->ar->winy == snap->winy;
}

static void make_snap(Snapshot* snap, Brush* brush, ViewContext* vc)
{
	if (brush->mtex.tex) {
		snap->brush_map_mode = brush->mtex.brush_map_mode;
		copy_v3_v3(snap->ofs, brush->mtex.ofs);
		copy_v3_v3(snap->size, brush->mtex.size);
		snap->rot = brush->mtex.rot;
	}
	else {
		snap->brush_map_mode = -1;
		snap->ofs[0]= snap->ofs[1]= snap->ofs[2]= -1;
		snap->size[0]= snap->size[1]= snap->size[2]= -1;
		snap->rot = -1;
	}

	snap->brush_size = brush_size(brush);
	snap->winx = vc->ar->winx;
	snap->winy = vc->ar->winy;
}

static int load_tex(Sculpt *sd, Brush* br, ViewContext* vc)
{
	static GLuint overlay_texture = 0;
	static int init = 0;
	static int tex_changed_timestamp = -1;
	static int curve_changed_timestamp = -1;
	static Snapshot snap;
	static int old_size = -1;

	GLubyte* buffer = 0;

	int size;
	int j;
	int refresh;

#ifndef _OPENMP
	(void)sd; /* quied unused warning */
#endif
	
	if (br->mtex.brush_map_mode == MTEX_MAP_MODE_TILED && !br->mtex.tex) return 0;
	
	refresh = 
		!overlay_texture ||
		(br->mtex.tex && 
		    (!br->mtex.tex->preview ||
		      br->mtex.tex->preview->changed_timestamp[0] != tex_changed_timestamp)) ||
		!br->curve ||
		br->curve->changed_timestamp != curve_changed_timestamp ||
		!same_snap(&snap, br, vc);

	if (refresh) {
		if (br->mtex.tex && br->mtex.tex->preview)
			tex_changed_timestamp = br->mtex.tex->preview->changed_timestamp[0];

		if (br->curve)
			curve_changed_timestamp = br->curve->changed_timestamp;

		make_snap(&snap, br, vc);

		if (br->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED) {
			int s = brush_size(br);
			int r = 1;

			for (s >>= 1; s > 0; s >>= 1)
				r++;

			size = (1<<r);

			if (size < 256)
				size = 256;

			if (size < old_size)
				size = old_size;
		}
		else
			size = 512;

		if (old_size != size) {
			if (overlay_texture) {
				glDeleteTextures(1, &overlay_texture);
				overlay_texture = 0;
			}

			init = 0;

			old_size = size;
		}

		buffer = MEM_mallocN(sizeof(GLubyte)*size*size, "load_tex");

		#pragma omp parallel for schedule(static) if (sd->paint.flags & PAINT_USE_OPENMP)
		for (j= 0; j < size; j++) {
			int i;
			float y;
			float len;

			for (i= 0; i < size; i++) {

				// largely duplicated from tex_strength

				const float rotation = -br->mtex.rot;
				float radius = brush_size(br);
				int index = j*size + i;
				float x;
				float avg;

				x = (float)i/size;
				y = (float)j/size;

				x -= 0.5f;
				y -= 0.5f;

				if (br->mtex.brush_map_mode == MTEX_MAP_MODE_TILED) {
					x *= vc->ar->winx / radius;
					y *= vc->ar->winy / radius;
				}
				else {
					x *= 2;
					y *= 2;
				}

				len = sqrtf(x*x + y*y);

				if ((br->mtex.brush_map_mode == MTEX_MAP_MODE_TILED) || len <= 1) {
					/* it is probably worth optimizing for those cases where 
					   the texture is not rotated by skipping the calls to
					   atan2, sqrtf, sin, and cos. */
					if (br->mtex.tex && (rotation > 0.001 || rotation < -0.001)) {
						const float angle    = atan2(y, x) + rotation;

						x = len * cos(angle);
						y = len * sin(angle);
					}

					x *= br->mtex.size[0];
					y *= br->mtex.size[1];

					x += br->mtex.ofs[0];
					y += br->mtex.ofs[1];

					avg = br->mtex.tex ? get_tex_pixel(br, x, y) : 1;

					avg += br->texture_sample_bias;

					if (br->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED)
						avg *= brush_curve_strength(br, len, 1); /* Falloff curve */

					buffer[index] = 255 - (GLubyte)(255*avg);
				}
				else {
					buffer[index] = 0;
				}
			}
		}

		if (!overlay_texture)
			glGenTextures(1, &overlay_texture);
	}
	else {
		size= old_size;
	}

	glBindTexture(GL_TEXTURE_2D, overlay_texture);

	if (refresh) {
		if (!init) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, size, size, 0, GL_ALPHA, GL_UNSIGNED_BYTE, buffer);
			init = 1;
		}
		else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_ALPHA, GL_UNSIGNED_BYTE, buffer);
		}

		if (buffer)
			MEM_freeN(buffer);
	}

	glEnable(GL_TEXTURE_2D);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (br->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}

	return 1;
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

int project_brush_radius(RegionView3D* rv3d, float radius, float location[3], bglMats* mats)
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

#if 0
int sculpt_get_brush_geometry(bContext* C, int x, int y, int* pixel_radius, float location[3], float modelview[16], float projection[16], int viewport[4])
{
	Object *ob = CTX_data_active_object(C);
	struct PaintStroke *stroke;
	float window[2];
	int hit;

	stroke = paint_stroke_new(C, NULL, NULL, NULL, NULL);

	window[0] = x + stroke->vc.ar->winrct.xmin;
	window[1] = y + stroke->vc.ar->winrct.ymin;

	memcpy(modelview, stroke->vc.rv3d->viewmat, sizeof(float[16]));
	memcpy(projection, stroke->vc.rv3d->winmat, sizeof(float[16]));
	memcpy(viewport, stroke->mats.viewport, sizeof(int[4]));

	if (ob && ob->paint && ob->paint->sculpt && ob->paint->pbvh && sculpt_stroke_get_location(C, stroke, location, window)) {
		*pixel_radius = project_brush_radius(stroke->vc.rv3d, brush_unprojected_radius(stroke->brush), location, &stroke->mats);

		if (*pixel_radius == 0)
			*pixel_radius = brush_size(stroke->brush);

		mul_m4_v3(ob->obmat, location);

		hit = 1;
	}
	else {
		Sculpt* sd    = CTX_data_tool_settings(C)->sculpt;
		Brush*  brush = paint_brush(&sd->paint);

		*pixel_radius = brush_size(brush);
		hit = 0;
	}

	paint_stroke_free(stroke);

	return hit;
}
#endif

// XXX duplicated from sculpt.c
float unproject_brush_radius(Object *ob, ViewContext *vc, float center[3], float offset)
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
static void paint_draw_cursor(bContext *C, int x, int y, void *unused)
{
	ViewContext vc;

	(void)unused;

	view3d_set_viewcontext(C, &vc);
	
// XXX
#if 0
	if (vc.obact->paint && vc.obact->paint->sculpt) {
		Paint *paint = paint_get_active(CTX_data_scene(C));
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
		Brush *brush = paint_brush(paint);

		int pixel_radius = 0, viewport[4];
		float location[3]; // XXX: modelview[16], projection[16];

		int hit;

		int flip;
		int sign;

		float* col;
		float  alpha;

		const float root_alpha = brush_alpha(brush);
		float visual_strength = root_alpha*root_alpha;

		const float min_alpha = 0.20f;
		const float max_alpha = 0.80f;

		{
			const float u = 0.5f;
			const float v = 1 - u;
			const float r = 20;

			const float dx = sd->last_x - x;
			const float dy = sd->last_y - y;

			if (dx*dx + dy*dy >= r*r) {
				sd->last_angle = atan2(dx, dy);

				sd->last_x = u*sd->last_x + v*x;
				sd->last_y = u*sd->last_y + v*y;
			}
		}

		if(!brush_use_locked_size(brush) && !(paint->flags & PAINT_SHOW_BRUSH)) 
			return;

		hit = 0;

		if (brush_use_locked_size(brush))
			brush_set_size(brush, pixel_radius);

		// XXX: no way currently to know state of pen flip or invert key modifier without starting a stroke
		flip = 1;

		sign = flip * ((brush->flag & BRUSH_DIR_IN)? -1 : 1);

		if (sign < 0 && ELEM4(brush->sculpt_tool, SCULPT_TOOL_DRAW, SCULPT_TOOL_INFLATE, SCULPT_TOOL_CLAY, SCULPT_TOOL_PINCH))
			col = brush->sub_col;
		else
			col = brush->add_col;

		alpha = (paint->flags & PAINT_SHOW_BRUSH_ON_SURFACE) ? min_alpha + (visual_strength*(max_alpha-min_alpha)) : 0.50f;

		if (ELEM(brush->mtex.brush_map_mode, MTEX_MAP_MODE_FIXED, MTEX_MAP_MODE_TILED) && brush->flag & BRUSH_TEXTURE_OVERLAY) {
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

			if (load_tex(sd, brush, &vc)) {
				glEnable(GL_BLEND);

				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glDepthMask(GL_FALSE);
				glDepthFunc(GL_ALWAYS);

				glMatrixMode(GL_TEXTURE);
				glPushMatrix();
				glLoadIdentity();
				
				if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED) {
					glTranslatef(0.5f, 0.5f, 0);

					if (brush->flag & BRUSH_RAKE) {
						glRotatef(sd->last_angle*(float)(180.0/M_PI), 0, 0, 1);
					}
					else {
						glRotatef(sd->special_rotation*(float)(180.0/M_PI), 0, 0, 1);
					}

					glTranslatef(-0.5f, -0.5f, 0);

					if (sd->draw_pressure && brush_use_size_pressure(brush)) {
						glTranslatef(0.5f, 0.5f, 0);
						glScalef(1.0f/sd->pressure_value, 1.0f/sd->pressure_value, 1);
						glTranslatef(-0.5f, -0.5f, 0);
					}
				}

				glColor4f(
					U.sculpt_paint_overlay_col[0],
					U.sculpt_paint_overlay_col[1],
					U.sculpt_paint_overlay_col[2],
					brush->texture_overlay_alpha / 100.0f);

				glBegin(GL_QUADS);
				if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED) {
					if (sd->draw_anchored) {
						glTexCoord2f(0, 0);
						glVertex2f(sd->anchored_initial_mouse[0]-sd->anchored_size - vc.ar->winrct.xmin, sd->anchored_initial_mouse[1]-sd->anchored_size - vc.ar->winrct.ymin);

						glTexCoord2f(1, 0);
						glVertex2f(sd->anchored_initial_mouse[0]+sd->anchored_size - vc.ar->winrct.xmin, sd->anchored_initial_mouse[1]-sd->anchored_size - vc.ar->winrct.ymin);

						glTexCoord2f(1, 1);
						glVertex2f(sd->anchored_initial_mouse[0]+sd->anchored_size - vc.ar->winrct.xmin, sd->anchored_initial_mouse[1]+sd->anchored_size - vc.ar->winrct.ymin);

						glTexCoord2f(0, 1);
						glVertex2f(sd->anchored_initial_mouse[0]-sd->anchored_size - vc.ar->winrct.xmin, sd->anchored_initial_mouse[1]+sd->anchored_size - vc.ar->winrct.ymin);
					}
					else {
						const int radius= brush_size(brush);

						glTexCoord2f(0, 0);
						glVertex2f((float)x-radius, (float)y-radius);

						glTexCoord2f(1, 0);
						glVertex2f((float)x+radius, (float)y-radius);

						glTexCoord2f(1, 1);
						glVertex2f((float)x+radius, (float)y+radius);

						glTexCoord2f(0, 1);
						glVertex2f((float)x-radius, (float)y+radius);
					}
				}
				else {
					glTexCoord2f(0, 0);
					glVertex2f(0, 0);

					glTexCoord2f(1, 0);
					glVertex2f(viewport[2], 0);

					glTexCoord2f(1, 1);
					glVertex2f(viewport[2], viewport[3]);

					glTexCoord2f(0, 1);
					glVertex2f(0, viewport[3]);
				}
				glEnd();

				glPopMatrix();
			}

			glPopAttrib();
		}

		if (hit) {
			float unprojected_radius;

			// XXX duplicated from brush_strength & paint_stroke_add_step, refactor later
			//wmEvent* event = CTX_wm_window(C)->eventstate;

			if (sd->draw_pressure && brush_use_alpha_pressure(brush))
				visual_strength *= sd->pressure_value;

			// don't show effect of strength past the soft limit
			if (visual_strength > 1) visual_strength = 1;

			if (sd->draw_anchored) {
				unprojected_radius = unproject_brush_radius(CTX_data_active_object(C), &vc, location, sd->anchored_size);
			}
			else {
				if (brush->flag & BRUSH_ANCHORED)
					unprojected_radius = unproject_brush_radius(CTX_data_active_object(C), &vc, location, 8);
				else
					unprojected_radius = unproject_brush_radius(CTX_data_active_object(C), &vc, location, brush_size(brush));
			}

			if (sd->draw_pressure && brush_use_size_pressure(brush))
				unprojected_radius *= sd->pressure_value;

			if (!brush_use_locked_size(brush))
				brush_set_unprojected_radius(brush, unprojected_radius);

			if(!(paint->flags & PAINT_SHOW_BRUSH))
				return;

		}

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

		if (sd->draw_anchored) {
			glTranslatef(sd->anchored_initial_mouse[0] - vc.ar->winrct.xmin, sd->anchored_initial_mouse[1] - vc.ar->winrct.ymin, 0.0f);
			glutil_draw_lined_arc(0.0, M_PI*2.0, sd->anchored_size, 40);
			glTranslatef(-sd->anchored_initial_mouse[0] + vc.ar->winrct.xmin, -sd->anchored_initial_mouse[1] + vc.ar->winrct.xmin, 0.0f);
		}
		else {
			glTranslatef((float)x, (float)y, 0.0f);
			glutil_draw_lined_arc(0.0, M_PI*2.0, brush_size(brush), 40);
			glTranslatef(-(float)x, -(float)y, 0.0f);
		}

		glPopAttrib();
	}
	//else
#endif

	
	{
		Paint *paint = paint_get_active(CTX_data_scene(C));
		Brush *brush = paint_brush(paint);

		if(!(paint->flags & PAINT_SHOW_BRUSH))
			return;

		glColor4f(brush->add_col[0], brush->add_col[1], brush->add_col[2], 0.5f);
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);

		glTranslatef((float)x, (float)y, 0.0f);
		glutil_draw_lined_arc(0.0, M_PI*2.0, brush_size(brush), 40); // XXX: for now use the brushes size instead of potentially using the unified size because the feature has been enabled for sculpt
		glTranslatef((float)-x, (float)-y, 0.0f);

		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);
	}
}

/**** Symmetry ****/

float calc_overlap(PaintStroke *stroke, float location[3], char symm, char axis, float angle)
{
	float mirror[3];
	float distsq;
	float mat[4][4];
	
	//paint_flip_coord(mirror, cache->traced_location, symm);
	paint_flip_coord(mirror, location, symm);

	unit_m4(mat);
	rotate_m4(mat, axis, angle);

	mul_m4_v3(mat, mirror);

	//distsq = len_squared_v3v3(mirror, cache->traced_location);
	distsq = len_squared_v3v3(mirror, location);

	if (distsq <= 4*stroke->radius_squared)
		return (2*stroke->radius - sqrt(distsq))  /  (2*stroke->radius);
	else
		return 0;
}

static float calc_radial_symmetry_feather(PaintStroke *stroke, Paint *paint,
					  float location[3], char symm, char axis)
{
	int i;
	float overlap;

	overlap = 0;
	for(i = 1; i < paint->radial_symm[axis-'X']; ++i) {
		const float angle = 2*M_PI*i / paint->radial_symm[axis-'X'];
		overlap += calc_overlap(stroke, location, symm, axis, angle);
	}

	return overlap;
}

static float calc_symmetry_feather(PaintStroke *stroke, Paint *paint, float location[3], char symm)
{
	if(paint->flags & PAINT_SYMMETRY_FEATHER) {
		float overlap;
		int i;

		overlap = 0;
		for (i = 0; i <= symm; i++) {
			if(i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {

				overlap += calc_overlap(stroke, location, i, 0, 0);

				overlap += calc_radial_symmetry_feather(stroke, paint, location, i, 'X');
				overlap += calc_radial_symmetry_feather(stroke, paint, location, i, 'Y');
				overlap += calc_radial_symmetry_feather(stroke, paint, location, i, 'Z');
			}
		}

		return 1/overlap;
	}
	else {
		return 1;
	}
}

/* flip data across the axes specified by symm */
static void calc_symm(bContext *C, PaintStroke *stroke, float location[3], char symm,
		      char axis, float angle)
{
	/* radial symmetry */
	unit_m4(stroke->symm_rot_mat);
	rotate_m4(stroke->symm_rot_mat, axis, angle);

	unit_m4(stroke->symm_rot_mat_inv);
	rotate_m4(stroke->symm_rot_mat_inv, axis, -angle);

	/* symmetry_location */
	paint_flip_coord(location, location, symm);
	mul_m4_v3(stroke->symm_rot_mat, location);
	copy_v3_v3(stroke->symmetry_location, location);

	/* callback */
	if(stroke->update_symmetry) {
		stroke->update_symmetry(C, stroke, symm, axis,
					angle,
					stroke->mirror_symmetry_pass,
					stroke->radial_symmetry_pass,
					stroke->symm_rot_mat);
	}
}

static void do_radial_symmetry(bContext *C, PaintStroke *stroke, Paint *paint, char symm, int axis, float feather)
{
	int i;

	for(i = 1; i < paint->radial_symm[axis-'X']; ++i) {
		const float angle = 2*M_PI*i / paint->radial_symm[axis-'X'];
		float location[3];

		stroke->radial_symmetry_pass= i;
		copy_v3_v3(location, stroke->location);
		calc_symm(C, stroke, location, symm, axis, angle);

		stroke->brush_action(C, stroke);
	}
}

static void paint_stroke_update_cache(bContext *C, PaintStroke *stroke, Paint *paint)
{
	ViewContext *vc = &stroke->vc;

	if(stroke->first_dab) {
		copy_v2_v2(stroke->initial_mouse, stroke->mouse);
		copy_v2_v2(stroke->tex_mouse, stroke->initial_mouse);

		if(!brush_use_locked_size(stroke->brush)) {
			stroke->initial_radius =
				paint_calc_object_space_radius(vc,
							       stroke->location,
							       brush_size(stroke->brush));
			brush_set_unprojected_radius(stroke->brush, stroke->initial_radius);
		}
		else
			stroke->initial_radius= brush_unprojected_radius(stroke->brush);

		stroke->initial_radius *= stroke->modifier_initial_radius_factor;
	}

	stroke->pixel_radius = brush_size(stroke->brush);
	stroke->radius = stroke->initial_radius;
	stroke->feather = calc_symmetry_feather(stroke, paint, stroke->location, paint->flags & 7);

	if(brush_use_size_pressure(stroke->brush)) {
		stroke->pixel_radius *= stroke->pressure;
		stroke->radius *= stroke->pressure;
	}

	if(!((stroke->brush->flag & BRUSH_ANCHORED) ||
	     stroke->modifier_use_original_texture_coords)) {
		copy_v2_v2(stroke->tex_mouse, stroke->mouse);

		if((stroke->brush->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED) &&
		   (stroke->brush->flag & BRUSH_RANDOM_ROTATION) &&
		   !(stroke->brush->flag & BRUSH_RAKE)) {
			stroke->rotation = 2*M_PI*BLI_frand();
		}
	}

	if(stroke->brush->flag & BRUSH_ANCHORED) {
		int dx, dy;

		dx = stroke->mouse[0] - stroke->initial_mouse[0];
		dy = stroke->mouse[1] - stroke->initial_mouse[1];

		stroke->pixel_radius = sqrt(dx*dx + dy*dy);

		stroke->rotation = atan2(dx, dy) + M_PI;

		if(stroke->brush->flag & BRUSH_EDGE_TO_EDGE) {
			float d[3];
			float halfway[3];
			float out[3];

			d[0] = dx;
			d[1] = dy;
			d[2] = 0;

			mul_v3_v3fl(halfway, d, 0.5f);
			add_v3_v3(halfway, stroke->initial_mouse);

			if(stroke->get_location(C, stroke, out, halfway)) {
				copy_v2_v2(stroke->tex_mouse, halfway);
				copy_v3_v3(stroke->location, out);
				stroke->pixel_radius  /= 2.0f;
			}
		}

		stroke->radius= paint_calc_object_space_radius(&stroke->vc,
							       stroke->location,
							       stroke->pixel_radius);
	}
	else if(stroke->brush->flag & BRUSH_RAKE) {
		const float u = 0.5f;
		const float r = 20;

		const float dx = stroke->last_rake[0] - stroke->mouse[0];
		const float dy = stroke->last_rake[1] - stroke->mouse[1];

		if(stroke->first_dab) {
			copy_v2_v2(stroke->last_rake, stroke->mouse);
		}
		else if (dx*dx + dy*dy >= r*r) {
			stroke->rotation = atan2(dx, dy);

			interp_v2_v2v2(stroke->last_rake, stroke->mouse, stroke->last_rake, u);
		}
	}

	stroke->radius_squared = stroke->radius*stroke->radius;
}

/* Put the location of the next stroke dot into the stroke RNA and apply it to the mesh */
static void paint_brush_stroke_add_step(bContext *C, wmOperator *op, wmEvent *event, float mouse_in[2])
{
	Paint *paint = paint_get_active(CTX_data_scene(C));
	PaintStroke *stroke = op->customdata;
	PointerRNA itemptr;
	int pen_flip;

	view3d_get_object_project_mat(stroke->vc.rv3d, stroke->vc.obact, stroke->project_mat);

	/* Tablet */
	if(event->custom == EVT_DATA_TABLET) {
		wmTabletData *wmtab= event->customdata;

		stroke->pressure = (wmtab->Active != EVT_TABLET_NONE) ? wmtab->Pressure : 1;
		pen_flip = (wmtab->Active == EVT_TABLET_ERASER);
	}
	else {
		stroke->pressure = 1;
		pen_flip = 0;
	}

	// XXX: temporary check for sculpt mode until things are more unified
	if(stroke->vc.obact->paint && stroke->vc.obact->paint->sculpt) {
		float delta[3];

		brush_jitter_pos(stroke->brush, mouse_in, stroke->mouse);

		// XXX: meh, this is round about because brush_jitter_pos isn't written in the best way to be reused here
		if(stroke->brush->flag & BRUSH_JITTER_PRESSURE) {
			sub_v3_v3v3(delta, stroke->mouse, mouse_in);
			mul_v3_fl(delta, stroke->pressure);
			add_v3_v3v3(stroke->mouse, mouse_in, delta);
		}
	}
	else
		copy_v2_v2(stroke->mouse, mouse_in);

	if(stroke->first_dab ||
	   !((stroke->brush->flag & BRUSH_ANCHORED) ||
	     stroke->modifier_use_original_location)) {

		/* XXX: can remove the following if statement once all modes have this */
		if(stroke->get_location)
			stroke->get_location(C, stroke, stroke->location, stroke->mouse);
		else
			zero_v3(stroke->location);
	}

	/* Add to stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	RNA_float_set_array(&itemptr, "location",     stroke->location);
	RNA_float_set_array(&itemptr, "mouse",        stroke->mouse);
	RNA_boolean_set    (&itemptr, "pen_flip",     pen_flip);
	RNA_float_set(&itemptr, "pressure", stroke->pressure);

	copy_v2_v2(stroke->last_mouse_position, stroke->mouse);

	paint_stroke_update_cache(C, stroke, paint);

	stroke->update_step(C, stroke, &itemptr);

	stroke->first_dab = 0;
}

/* Returns zero if no sculpt changes should be made, non-zero otherwise */
static int paint_smooth_stroke(PaintStroke *stroke, float output[2], wmEvent *event)
{
	output[0] = event->x; 
	output[1] = event->y;

	if((stroke->brush->flag & BRUSH_SMOOTH_STROKE) &&  
	    !ELEM4(stroke->brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_SNAKE_HOOK) &&
	    !(stroke->brush->flag & BRUSH_ANCHORED) &&
	    !(stroke->brush->flag & BRUSH_RESTORE_MESH))
	{
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
	       !ELEM4(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_SNAKE_HOOK);
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
					pressure = brush_use_size_pressure(stroke->brush) ? wmtab->Pressure : 1;
			}

			scale = (brush_size(stroke->brush) * pressure * stroke->brush->spacing/50.0f) / length;
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
			      StrokeUpdateSymmetry update_symmetry,
			      StrokeBrushAction brush_action,
			      StrokeDone done)
{
	PaintStroke *stroke = MEM_callocN(sizeof(PaintStroke), "PaintStroke");

	stroke->brush = paint_brush(paint_get_active(CTX_data_scene(C)));
	view3d_set_viewcontext(C, &stroke->vc);
	view3d_get_transformation(stroke->vc.ar, stroke->vc.rv3d, stroke->vc.obact, &stroke->mats);
	stroke->modifier_initial_radius_factor = 1;

	stroke->get_location = get_location;
	stroke->test_start = test_start;
	stroke->update_step = update_step;
	stroke->update_symmetry = update_symmetry;
	stroke->brush_action = brush_action;
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

	if(!stroke->stroke_started) {
		stroke->last_mouse_position[0] = event->x;
		stroke->last_mouse_position[1] = event->y;
		stroke->stroke_started = stroke->test_start(C, op, event);

		if(stroke->stroke_started) {
			stroke->smooth_stroke_cursor =
				WM_paint_cursor_activate(CTX_wm_manager(C), paint_poll, paint_draw_smooth_stroke, stroke);

			if(stroke->brush->flag & BRUSH_AIRBRUSH)
				stroke->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, stroke->brush->rate);

			stroke->first_dab = 1;
		}

		//ED_region_tag_redraw(ar);
	}

	/* TODO: fix hardcoded events here */
	if(event->type == LEFTMOUSE && event->val == KM_RELEASE) {
		/* exit stroke, free data */
		if(stroke->smooth_stroke_cursor)
			WM_paint_cursor_end(CTX_wm_manager(C), stroke->smooth_stroke_cursor);

		if(stroke->timer)
			WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), stroke->timer);

		if(stroke->stroke_started)
			stroke->done(C, stroke);
		MEM_freeN(stroke);
		return OPERATOR_FINISHED;
	}
	else if(stroke->first_dab || ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE) || (event->type == TIMER && (event->customdata == stroke->timer))) {
		if(stroke->stroke_started) {
			if(paint_smooth_stroke(stroke, mouse, event)) {
				if(paint_space_stroke_enabled(stroke->brush)) {
					if(!paint_space_stroke(C, op, event, mouse)) {
						//ED_region_tag_redraw(ar);
					}
				}
				else {
					paint_brush_stroke_add_step(C, op, event, mouse);
				}
			}
			else {
				;//ED_region_tag_redraw(ar);
			}
		}
	}

	/* we want the stroke to have the first daub at the start location instead of waiting till we have moved the space distance */
	if(stroke->first_dab &&
	   stroke->stroke_started &&
	   paint_space_stroke_enabled(stroke->brush) &&
	   !(stroke->brush->flag & BRUSH_ANCHORED) &&
	   !(stroke->brush->flag & BRUSH_SMOOTH_STROKE))
	{
		paint_brush_stroke_add_step(C, op, event, mouse);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

void paint_stroke_apply_brush(bContext *C, PaintStroke *stroke, Paint *paint)
{
	char symm = paint->flags & 7;
	float location[3];
	int i;

	/* symm is a bitwise combination of XYZ:
	   1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */ 
	for(i = 0; i <= symm; ++i) {		
		if(i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {
			stroke->mirror_symmetry_pass= i;
			stroke->radial_symmetry_pass= 0;

			copy_v3_v3(location, stroke->location);
			calc_symm(C, stroke, location, i, 0, 0);

			stroke->brush_action(C, stroke);

			do_radial_symmetry(C, stroke, paint, i, 'X', stroke->feather);
			do_radial_symmetry(C, stroke, paint, i, 'Y', stroke->feather);
			do_radial_symmetry(C, stroke, paint, i, 'Z', stroke->feather);
		}
	}
}

/* combines mask, curve, and texture strengths */
float paint_stroke_combined_strength(PaintStroke *stroke, float dist, float co[3], float mask)
{
	float mco[3];

	/* if the active area is being applied for symmetry, flip it
	   across the symmetry axis and rotate it back to the orignal
	   position in order to project it. This insures that the 
	   brush texture will be oriented correctly. */
	if(stroke->brush->mtex.tex) {
		paint_stroke_symmetry_unflip(stroke, mco, co);
		co = mco;
	}
	
	return brush_tex_strength(&stroke->vc,
				  stroke->project_mat, stroke->brush, co, mask, dist,
				  stroke->pixel_radius, stroke->radius,
				  stroke->rotation,
				  stroke->tex_mouse);
}

int paint_stroke_exec(bContext *C, wmOperator *op)
{
	Paint *paint = paint_get_active(CTX_data_scene(C));
	PaintStroke *stroke = op->customdata;

	RNA_BEGIN(op->ptr, itemptr, "stroke") {
		RNA_float_get_array(&itemptr, "location", stroke->location);
		RNA_float_get_array(&itemptr, "mouse", stroke->mouse);
		stroke->pressure = RNA_float_get(&itemptr, "pressure");
		paint_stroke_update_cache(C, stroke, paint);

		stroke->update_step(C, stroke, &itemptr);

		stroke->first_dab = 0;
	}
	RNA_END;

	MEM_freeN(stroke);
	op->customdata = NULL;

	return OPERATOR_FINISHED;
}

/**** mode data ****/

void *paint_stroke_mode_data(struct PaintStroke *stroke)
{
	return stroke->mode_data;
}

void paint_stroke_set_mode_data(PaintStroke *stroke, void *mode_data)
{
	stroke->mode_data = mode_data;
}

/**** cache access ***/

ViewContext *paint_stroke_view_context(PaintStroke *stroke)
{
	return &stroke->vc;
}

float paint_stroke_feather(struct PaintStroke *stroke)
{
	return stroke->feather;
}

void paint_stroke_mouse_location(PaintStroke *stroke, float mouse[2])
{
	copy_v2_v2(mouse, stroke->mouse);
}
void paint_stroke_initial_mouse_location(PaintStroke *stroke, float initial_mouse[2])
{
	copy_v2_v2(initial_mouse, stroke->initial_mouse);
}

void paint_stroke_location(PaintStroke *stroke, float location[3])
{
	copy_v3_v3(location, stroke->location);
}

float paint_stroke_pressure(struct PaintStroke *stroke)
{
	return stroke->pressure;
}

void paint_stroke_symmetry_location(struct PaintStroke *stroke, float loc[3])
{
	copy_v3_v3(loc, stroke->symmetry_location);
}

float paint_stroke_radius(struct PaintStroke *stroke)
{
	return stroke->radius;
}

float paint_stroke_radius_squared(struct PaintStroke *stroke)
{
	return stroke->radius_squared;
}

int paint_stroke_first_dab(PaintStroke *stroke)
{
	return stroke->first_dab;
}

void paint_stroke_project(PaintStroke *stroke, float loc[3], float out[2])
{
	view3d_project_float(stroke->vc.ar, loc, out, stroke->project_mat);
}

void paint_stroke_symmetry_unflip(PaintStroke *stroke, float out[3], float vec[3])
{
	paint_flip_coord(out, vec, stroke->mirror_symmetry_pass);
		
	if(stroke->radial_symmetry_pass)
		mul_m4_v3(stroke->symm_rot_mat_inv, out);
}

/**** stroke modifiers ****/
void paint_stroke_set_modifier_use_original_location(PaintStroke *stroke)
{
	stroke->modifier_use_original_location = 1;
}

void paint_stroke_set_modifier_initial_radius_factor(PaintStroke *stroke,
						     float initial_radius_factor)
{
	stroke->modifier_initial_radius_factor = initial_radius_factor;
}

void paint_stroke_set_modifier_use_original_texture_coords(PaintStroke *stroke)
{
	stroke->modifier_use_original_texture_coords = 1;
}

/* returns 1 if the mouse is over the mesh, 0 otherwise */
int paint_stroke_over_mesh(bContext *C, PaintStroke *stroke, int x, int y)
{
	float mouse[2] = {x, y}, co[3];
	return stroke->get_location(C, stroke, co, mouse);
}

/* Do a raycast in the tree to find the 3d brush location
   (This allows us to ignore the GL depth buffer)
   Returns 0 if the ray doesn't hit the mesh, non-zero otherwise
 */
int paint_stroke_get_location(bContext *C, PaintStroke *stroke,
			      BLI_pbvh_HitOccludedCallback hit_cb, void *mode_data,
			      float out[3], float mouse[2], int original)
{
	ViewContext *vc = paint_stroke_view_context(stroke);
	return paint_util_raycast(vc, hit_cb, mode_data, out, mouse, original);
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

/* Optimization for testing if a coord is within the brush area */

void paint_stroke_test_init(PaintStrokeTest *test, PaintStroke *stroke)
{
	test->radius_squared= stroke->radius_squared;
	copy_v3_v3(test->location, stroke->symmetry_location);
}

int paint_stroke_test(PaintStrokeTest *test, float co[3])
{
	float distsq = len_squared_v3v3(co, test->location);

	if(distsq <= test->radius_squared) {
		test->dist = sqrt(distsq);
		return 1;
	}
	else {
		return 0;
	}
}

int paint_stroke_test_sq(PaintStrokeTest *test, float co[3])
{
	float distsq = len_squared_v3v3(co, test->location);

	if(distsq <= test->radius_squared) {
		test->dist = distsq;
		return 1;
	}
	else {
		return 0;
	}
}

int paint_stroke_test_fast(PaintStrokeTest *test, float co[3])
{
	return len_squared_v3v3(co, test->location) <= test->radius_squared;
}

int paint_stroke_test_cube(PaintStrokeTest *test, float co[3], float local[4][4])
{
	const static float side = 0.70710678118654752440084436210485; // sqrt(.5);

	float local_co[3];

	mul_v3_m4v3(local_co, local, co);

	local_co[0] = fabs(local_co[0]);
	local_co[1] = fabs(local_co[1]);
	local_co[2] = fabs(local_co[2]);

	if (local_co[0] <= side && local_co[1] <= side && local_co[2] <= side) {
		test->dist = MAX3(local_co[0], local_co[1], local_co[2]) / side;

		return 1;
	}
	else {
		return 0;
	}
}


#if 0

static int paint_stroke_test_cyl(SculptBrushTest *test, float co[3], float location[3], float an[3])
{
	if (paint_stroke_test_fast(test, co)) {
		float t1[3], t2[3], t3[3], dist;

		sub_v3_v3v3(t1, location, co);
		sub_v3_v3v3(t2, x2, location);

		cross_v3_v3v3(t3, an, t1);

		dist = len_v3(t3)/len_v3(t2);

		test->dist = dist;

		return 1;
	}

	return 0;
}

#endif
