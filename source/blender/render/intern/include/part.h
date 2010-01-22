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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_PART_H__
#define __RENDER_PART_H__

#include "DNA_listBase.h"

#include "result.h"

struct Render;
struct RenderPart;
struct RenderResult;
struct rctf;

/* Create/Free */

void parts_create(struct Render *re);
void parts_free(struct Render *re);

/* Find/Next */

int parts_find_next_slice(struct Render *re, int *slice, int *minx,
	struct rctf *viewplane);

struct RenderPart *parts_find_next(struct Render *re, int minx);

/* Struct */

typedef struct RenderPart {
	struct RenderPart *next, *prev;

	struct Render *re;
	
	struct RenderResult *result;			/* result of part rendering */
	ListBase fullresult;					/* optional full sample buffers */
	struct RenderLayer *rlpp[RE_MAX_OSA];	/* full sample buffers for current layer */
	int totsample;							/* total number of full sample buffers */
	
	int *recto;						/* object table for objects */
	int *rectp;						/* polygon index table */
	int *rectz;						/* zbuffer */
	int *rectmask;					/* negative zmask */
	void **rectdaps;				/* delta acum buffer for pixel structs */
	int *rectbacko;					/* object table for backside sss */
	int *rectbackp;					/* polygon index table for backside sss */
	int *rectbackz;					/* zbuffer for backside sss */
	void **rectall;					/* buffer for all faces for sss */

	rcti disprect;					/* part coordinates within total picture */
	int rectx, recty;				/* the size */
	short crop, ready;				/* crop is amount of pixels we crop, for filter */
	short sample, nr;				/* sample can be used by zbuffers, nr is partnr */
	short thread;					/* thread id */
	
	char *clipflag;					/* clipflags for part zbuffering */
} RenderPart;

#endif /* __RENDER_PART_H__ */

