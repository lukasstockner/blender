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

#ifndef __RENDER_RESULT_H__
#define __RENDER_RESULT_H__

#include "DNA_vec_types.h"

struct Render;
struct RenderLayer;
struct RenderPart;
struct RenderResult;
struct ShadeResult;
struct ShadeSample;

#define RE_MAX_OSA 16
#define RR_USEMEM	0

/* Shade Results */

void shade_result_init(struct ShadeResult *shr, int tot);
int shade_result_accumulate(struct ShadeResult *shr,
	struct ShadeSample *ssamp, int tot, int passflag);
void shade_result_interpolate(struct ShadeResult *shr,
	struct ShadeResult *shr1, struct ShadeResult *shr2, float t, int passflag);

void shade_result_to_part(struct Render *re, struct RenderPart *pa,
	struct RenderLayer *rl, int offs, struct ShadeResult *shr);

/* Render Layer */

struct bNodeTree;
struct ListBase;

#define PASS_VECTOR_MAX	10000.0f
#define PASS_Z_MAX		10e10f

/* RenderResult Create/Free */

struct RenderResult *render_result_create(struct Render *re, rcti *partrct, int crop, int savebuffers);
struct RenderResult *render_result_full_sample_create(struct Render *re);

void render_result_free(ListBase *lb, struct RenderResult *rr);

/* Layers and Passes */

struct RenderLayer *render_get_active_layer(struct Render *re, struct RenderResult *rr);
void renderresult_add_names(struct RenderResult *rr);

void push_render_result(struct Render *re);
void pop_render_result(struct Render *re);

/* Merging */

void render_result_merge_part(struct Render *re, struct RenderResult *result);
void render_result_border_merge(struct Render *re);

/* Full Sample */

void render_result_exr_write(struct Render *re);
void render_result_exr_read(struct Render *re);

void do_merge_fullsample(struct Render *re, struct bNodeTree *ntree, struct ListBase *list);

/* Exr Read/Write */

void render_unique_exr_name(struct Render *re, char *str, int sample);

int render_result_read_from_file(char *filename, struct RenderResult *rr);
void render_result_read(struct Render *re, int sample);

/* Convenience Function */

int get_sample_layers(struct Render *re, struct RenderPart *pa,
	struct RenderLayer *rl, struct RenderLayer **rlpp);

/* XXX This shouldn't be here */

void tag_scenes_for_render(struct Render *re);

#endif /* __RENDER_RESULT_H__ */

