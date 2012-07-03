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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __PAINT_2D_H__

struct ImBuf;
struct Scene;
struct Brush;

typedef struct BrushPainterCache {
	short enabled;

	int size;           /* size override, if 0 uses 2*BKE_brush_size_get(brush) */
	short flt;          /* need float imbuf? */
	short texonly;      /* no alpha, color or fallof, only texture in imbuf */

	int lastsize;
	float lastalpha;
	float lastjitter;
	float lastrotation;

	struct ImBuf *ibuf;
	struct ImBuf *texibuf;
	struct ImBuf *maskibuf;
} BrushPainterCache;

struct BrushPainter {
	struct Scene *scene;
	struct Brush *brush;

	float lastmousepos[2];  /* mouse position of last paint call */

	float accumdistance;    /* accumulated distance of brush since last paint op */
	float lastpaintpos[2];  /* position of last paint op */
	float startpaintpos[2]; /* position of first paint */

	double accumtime;       /* accumulated time since last paint op (airbrush) */
	double lasttime;        /* time of last update */

	float lastpressure;

	short firsttouch;       /* first paint op */

	float startsize;
	float startalpha;
	float startjitter;
	float startspacing;

	BrushPainterCache cache;
};

#endif
