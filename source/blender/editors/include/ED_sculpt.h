/**
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Nicholas Bishop
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_SCULPT_H
#define ED_SCULPT_H

struct ARegion;
struct bContext;
struct Object;
struct RegionView3D;
struct wmKeyConfig;
struct wmWindowManager;

/* sculpt.c */
void ED_operatortypes_sculpt(void);
void sculpt_get_redraw_planes(float planes[4][4], struct ARegion *ar,
				   struct RegionView3D *rv3d, struct Object *ob);
void ED_sculpt_force_update(struct bContext *C);
void ED_sculpt_modifiers_changed(struct Object *ob);

/* paint_ops.c */
void ED_operatortypes_paint(void);
void ED_keymap_paint(struct wmKeyConfig *keyconf);

/* paint_undo.c */
#define UNDO_PAINT_IMAGE	0
#define UNDO_PAINT_MESH		1

int ED_undo_paint_step(struct bContext *C, int type, int step, const char *name);
void ED_undo_paint_free(void);
int ED_undo_paint_valid(int type, const char *name);

void ED_draw_paint_overlay(const struct bContext *C, struct ARegion *ar);
void ED_draw_on_surface_cursor(float modelview[16], float projection[16], float col[3], float alpha, float size[3], int viewport[4], float location[3], float inner_radius, float outer_radius, int brush_size);
void ED_draw_fixed_overlay_on_surface(float modelview[16], float projection[16], float size[3], int viewport[4], float location[3], float outer_radius, struct Sculpt *sd, struct Brush *brush, struct ViewContext *vc, float t, float b, float l, float r, float angle);

#endif
