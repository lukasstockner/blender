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
#include "DNA_texture_types.h" /* for MTex */

//#ifndef MAX_MTEX // XXX Not used?
//#define MAX_MTEX	18
//#endif

struct CurveMapping;
struct MTex;
struct Image;

typedef struct BrushClone {
	struct Image *image;		/* image for clone tool */
	float offset[2];		/* offset of clone image from canvas */
	float alpha, pad;		/* transparency for drawing of clone image */
} BrushClone;

typedef struct Brush {
	ID id;

	struct BrushClone clone;
	struct CurveMapping *curve;	/* falloff curve */
	struct MTex mtex;

	struct ImBuf *icon_imbuf;
	PreviewImage *preview;
	char icon_filepath[240];

	float normal_weight;

	short blend;		/* blend mode */
	short ob_mode;		/* & with ob->mode to see if the brush is compatible, use for display only. */
	int size;			/* brush diameter */
	int flag;			/* general purpose flag */	
	float jitter;			/* jitter the position of the brush */
	int spacing;			/* spacing of paint operations */
	int smooth_stroke_radius;	/* turning radius (in pixels) for smooth stroke */
	float smooth_stroke_factor;	/* higher values limit fast changes in the stroke direction */
	float rate;			/* paint operations / second (airbrush) */

	float rgb[3];			/* color */
	float alpha;			/* opacity */

	int sculpt_plane;		/* the direction of movement for sculpt vertices */

	float plane_offset;		/* offset for plane brushes (clay, flatten, fill, scrape) */

	char sculpt_tool;		/* active sculpt tool */
	char vertexpaint_tool;		/* active vertex/weight paint tool/blend mode */
	char imagepaint_tool;		/* active image paint tool */
	char pad3;

	float autosmooth_factor;

	float crease_pinch_factor;

	float plane_trim;

	float texture_sample_bias;
	int   texture_overlay_alpha;

	float unprojected_radius;

	float add_col[3];
	float sub_col[3];
} Brush;

/* Brush.flag */
#define BRUSH_AIRBRUSH		(1<<0)
#define BRUSH_TORUS		(1<<1)
#define BRUSH_ALPHA_PRESSURE	(1<<2)
#define BRUSH_SIZE_PRESSURE	(1<<3)
#define BRUSH_JITTER_PRESSURE	(1<<4) /* was BRUSH_RAD_PRESSURE */
#define BRUSH_SPACING_PRESSURE	(1<<5)
#define BRUSH_FIXED_TEX		(1<<6)
#define BRUSH_RAKE		(1<<7)
#define BRUSH_ANCHORED		(1<<8)
#define BRUSH_DIR_IN		(1<<9)
#define BRUSH_SPACE		(1<<10)
#define BRUSH_SMOOTH_STROKE	(1<<11)
#define BRUSH_PERSISTENT	(1<<12)
#define BRUSH_ACCUMULATE	(1<<13)
#define BRUSH_LOCK_ALPHA	(1<<14)
#define BRUSH_ORIGINAL_NORMAL	(1<<15)
#define BRUSH_OFFSET_PRESSURE	(1<<16)
#define BRUSH_SPACE_ATTEN	(1<<18)
#define BRUSH_ADAPTIVE_SPACE	(1<<19)
#define BRUSH_LOCK_SIZE		(1<<20)
#define BRUSH_TEXTURE_OVERLAY	(1<<21)
#define BRUSH_EDGE_TO_EDGE	(1<<22)
#define BRUSH_RESTORE_MESH	(1<<23)
#define BRUSH_INVERSE_SMOOTH_PRESSURE (1<<24)
#define BRUSH_RANDOM_ROTATION (1<<25)
#define BRUSH_PLANE_TRIM (1<<26)
#define BRUSH_FRONTFACE (1<<27)
#define BRUSH_CUSTOM_ICON (1<<28)
#define BRUSH_MASK (1<<29)

/* Brush.sculpt_tool */
typedef enum {
	SCULPT_TOOL_DRAW = 1,
	SCULPT_TOOL_SMOOTH,
	SCULPT_TOOL_PINCH,
	SCULPT_TOOL_INFLATE,
	SCULPT_TOOL_GRAB,
	SCULPT_TOOL_LAYER,
	SCULPT_TOOL_FLATTEN,
	SCULPT_TOOL_CLAY,
	SCULPT_TOOL_FILL,
	SCULPT_TOOL_SCRAPE,
	SCULPT_TOOL_NUDGE,
	SCULPT_TOOL_THUMB,
	SCULPT_TOOL_SNAKE_HOOK,
	SCULPT_TOOL_ROTATE,
	SCULPT_TOOL_WAX,        // XXX: reuse this slot later
	SCULPT_TOOL_CREASE,
	SCULPT_TOOL_BLOB,
	SCULPT_TOOL_CLAY_TUBES
} SculptTool;

/* Brush.vertexpaint_tool */
#define VERTEX_PAINT_BLUR 99
/* The other vpaint tools are in IMB_BlendMode */

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

