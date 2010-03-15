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
 * The Original Code is Copyright (C) 2006 Blender Foundation
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_SHADING_H__
#define __RENDER_SHADING_H__

#include "result.h"

struct Isect;
struct HaloRen;
struct LampRen;
struct ObjectInstanceRen obi;
struct PixelRow;
struct RenderLayer;
struct RenderPart;
struct ShadeInput;
struct ShadeResult;
struct StrandPoint;
struct StrandRen;
struct StrandSegment;
struct StrandVert;
struct VlakRen;

/* shadeinput.c */

/* needed to calculate shadow and AO for an entire pixel */
typedef struct ShadeSample {
	int tot;						/* amount of shi in use, can be 1 for not FULL_OSA */
	
	/* could be malloced once */
	ShadeInput shi[RE_MAX_OSA];
	ShadeResult shr[RE_MAX_OSA];
} ShadeSample;

/* Shade Input */

void shade_input_set_triangle_i(struct Render *re, struct ShadeInput *shi,
	struct ObjectInstanceRen *obi, struct VlakRen *vlr, short i1, short i2, short i3);

void shade_input_calc_viewco(struct Render *re, struct ShadeInput *shi,
	float x, float y, float z, float *view, float *dxyview, float *co, float *dxco, float *dyco);
void shade_input_set_viewco(struct Render *re, struct ShadeInput *shi,
	float x, float y, float sx, float sy, float z);

void shade_input_set_uv(struct ShadeInput *shi);
void shade_input_set_normals(struct ShadeInput *shi);
void shade_input_flip_normals(struct ShadeInput *shi);
void shade_input_set_shade_texco(struct Render *re, struct ShadeInput *shi);

void shade_input_set_strand(struct Render *re, struct ShadeInput *shi,
	struct StrandRen *strand, struct StrandPoint *spoint);
void shade_input_set_strand_texco(struct Render *re, struct ShadeInput *shi,
	struct StrandRen *strand, struct StrandVert *svert, struct StrandPoint *spoint);
void shade_input_calc_reflection(struct ShadeInput *shi);

void shade_input_init_material(struct Render *re, struct ShadeInput *shi);

void vlr_set_uv_indices(struct VlakRen *vlr, int *i1, int *i2, int *i3);

/* Shading */

/* also the node shader callback */
void shade_material_loop(struct Render *re, struct ShadeInput *shi, struct ShadeResult *shr);
void shade_volume_loop(struct Render *re, struct ShadeInput *shi, struct ShadeResult *shr);

void shade_input_do_shade(struct Render *re, struct ShadeInput *shi,
	struct ShadeResult *shr);

void shade_sample_initialize(struct Render *re, struct ShadeSample *ssamp, struct RenderPart *pa, struct RenderLayer *rl);
void shade_samples_from_pixel(struct Render *re, struct ShadeSample *ssamp, struct PixelRow *row, int x, int y);
void shade_samples(struct Render *re, struct ShadeSample *ssamp);

/* shadeoutput. */
void shade_surface(struct Render *re, struct ShadeInput *shi, struct ShadeResult *shr, int backside);

void shade_color(struct Render *re, struct ShadeInput *shi, ShadeResult *shr);
void shade_jittered_coords(struct Render *re, struct ShadeInput *shi, int max, float jitco[RE_MAX_OSA][3], int *totjitco);
void shade_strand_surface_co(struct ShadeInput *shi, float co[3], float n[3]);
void shade_surface_direct(struct Render *re, struct ShadeInput *shi, struct ShadeResult *shr);

/* Utilities */
void environment_lighting_apply(struct Render *re, struct ShadeInput *shi, struct ShadeResult *shr);

void ambient_occlusion(struct Render *re, struct ShadeInput *shi);
float fresnel_fac(float *view, float *vn, float fresnel, float fac);
void shade_ray(struct Render *re, struct Isect *is, struct ShadeInput *shi, struct ShadeResult *shr);

/**
 * Render the pixel at (x,y) for object ap. Apply the jitter mask. 
 * Output is given in float collector[4]. The type vector:
 * t[0] - min. distance
 * t[1] - face/halo index
 * t[2] - jitter mask                     
 * t[3] - type ZB_POLY or ZB_HALO
 * t[4] - max. distance
 * mask is pixel coverage in bits
 * @return pointer to the object
 */

int shadeHaloFloat(struct Render *re, struct HaloRen *har, 
					float *col, int zz, 
					float dist, float xn, 
					float yn, short flarec);

#endif /* __RENDER_SHADING_H__ */

