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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef DNA_BRUSH_TYPES_H
#define DNA_BRUSH_TYPES_H

#include "DNA_ID.h"
#include "DNA_texture_types.h"

#ifndef MAX_MTEX
#define MAX_MTEX	18
#endif

struct CurveMapping;
struct MTex;
struct Image;

typedef struct BrushClone {
	struct Image *image;		/* image for clone tool */
	float offset[2];			/* offset of clone image from canvas */
	float alpha, pad;			/* transparency for drawing of clone image */
} BrushClone;

typedef struct Brush {
	ID id;

	struct BrushClone clone;
	struct CurveMapping *curve;	/* falloff curve */
	struct MTex mtex;
	
	short blend, pad;				/* blend mode */
	int size;				/* brush diameter */
	int flag, pad3;				/* general purpose flag */	
	float detail;				/* dynamic subdivission detail */
	float smoothness;			/* dynamic subdivission smoothness*/
	float jitter;				/* jitter the position of the brush */
	int spacing;				/* spacing of paint operations */
	int smooth_stroke_radius;		/* turning radius (in pixels) for smooth stroke */
	float smooth_stroke_factor;		/* higher values limit fast changes in the stroke direction */
	float rate;					/* paint operations / second (airbrush) */

	float rgb[3];				/* color */
	float alpha;				/* opacity */
	
	int sculpt_direction;		/* the direction of movement for sculpt vertices */

	float plane_offset; /* offset for plane brushes (clay, flatten, fill, scrape, contrast) */
	float texture_offset;

	char sculpt_tool;			/* active sculpt tool */
	char vertexpaint_tool;		/* active vertex/weight paint tool/blend mode */
	char imagepaint_tool;		/* active image paint tool */
	char stroke_tool;

	int pad2;
} Brush;

/* Brush.flag */
#define BRUSH_BIT(x) (1<<x)
#define BRUSH_AIRBRUSH		BRUSH_BIT(0)
#define BRUSH_TORUS		BRUSH_BIT(1)
#define BRUSH_ALPHA_PRESSURE	BRUSH_BIT(2)
#define BRUSH_SIZE_PRESSURE	BRUSH_BIT(3)
#define BRUSH_JITTER_PRESSURE	BRUSH_BIT(4) /* was BRUSH_RAD_PRESSURE */
#define BRUSH_SPACING_PRESSURE	BRUSH_BIT(5)
#define BRUSH_FIXED_TEX		BRUSH_BIT(6)
#define BRUSH_RAKE		BRUSH_BIT(7)
#define BRUSH_ANCHORED		BRUSH_BIT(8)
#define BRUSH_DIR_IN		BRUSH_BIT(9)
#define BRUSH_SPACE		BRUSH_BIT(10)
#define BRUSH_SMOOTH_STROKE	BRUSH_BIT(11)
#define BRUSH_PERSISTENT	BRUSH_BIT(12)
#define BRUSH_ACCUMULATE	BRUSH_BIT(13)
#define BRUSH_LOCK_ALPHA	BRUSH_BIT(14)
#define BRUSH_ORIGINAL_NORMAL	BRUSH_BIT(15)
#define BRUSH_OFFSET_PRESSURE	BRUSH_BIT(16)
#define BRUSH_SUBDIV		BRUSH_BIT(17)
#define BRUSH_SPACE_ATTEN	BRUSH_BIT(18)
#define BRUSH_ADAPTIVE_SPACE	BRUSH_BIT(19)


/* Brush stroke_tool */
//#define STROKE_TOOL_DOTS	1
//#define STROKE_TOOL_SPACE	2
//#define STROKE_TOOL_FREEHAND	3
//#define STROKE_TOOL_SMOOTH	4
//#define STROKE_TOOL_AIRBRUSH	5
//#define STROKE_TOOL_ANCHORED	6

/* Brush.sculpt_tool */
#define SCULPT_TOOL_DRAW      1
#define SCULPT_TOOL_SMOOTH    2
#define SCULPT_TOOL_PINCH     3
#define SCULPT_TOOL_INFLATE   4
#define SCULPT_TOOL_GRAB      5
#define SCULPT_TOOL_LAYER     6
#define SCULPT_TOOL_FLATTEN   7
#define SCULPT_TOOL_CLAY      8
#define SCULPT_TOOL_FILL      9
#define SCULPT_TOOL_SCRAPE   10
#define SCULPT_TOOL_CONTRAST 11

/* ImagePaintSettings.tool */
#define PAINT_TOOL_DRAW		0
#define PAINT_TOOL_SOFTEN	1
#define PAINT_TOOL_SMEAR	2
#define PAINT_TOOL_CLONE	3

/* direction that the brush displaces along */
enum {
	SCULPT_DISP_DIR_AREA,
	SCULPT_DISP_DIR_VIEW,	
	SCULPT_DISP_DIR_X,
	SCULPT_DISP_DIR_Y,
	SCULPT_DISP_DIR_Z,
};

#define MAX_BRUSH_PIXEL_RADIUS 200

#endif

