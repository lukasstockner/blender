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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_CACHE_H__
#define __RENDER_CACHE_H__

#include "object.h"

struct DerivedMesh;
struct ObjectRen;
struct Render;
struct RenderDB;
struct RenderLayer;
struct RenderPart;
struct ShadeInput;
struct ShadeSample;

/* Pixel Cache: per part cache for sharing occlusion and other shading
   results between pixel. */

typedef struct PixelCacheSample {
	float co[3], n[3];
	float ao[3], env[3], indirect[3];
	float intensity, dist2;
	int x, y, filled;
} PixelCacheSample;

typedef struct PixelCache {
	PixelCacheSample *sample;
	int x, y, w, h, step;
} PixelCache;

PixelCache *pixel_cache_create(struct Render *re, struct RenderPart *pa, struct ShadeSample *ssamp);
void pixel_cache_free(PixelCache *cache);

int pixel_cache_sample(struct PixelCache *cache, struct ShadeInput *shi);
void pixel_cache_insert_sample(struct PixelCache *cache, struct ShadeInput *shi);

/* Surface Cache: used for strand to cache occlusion and speed vectors
   on the original surface to be used by the strand. */

typedef struct SurfaceCache {
	struct SurfaceCache *next, *prev;
	ObjectRen obr;
	int (*face)[4];
	float (*co)[3];
	/* for occlusion caching */
	float (*ao)[3];
	float (*env)[3];
	float (*indirect)[3];
	/* for speedvectors */
	float (*prevco)[3], (*nextco)[3];
	int totvert, totface;
} SurfaceCache;

SurfaceCache *surface_cache_create(struct Render *re, struct ObjectRen *obr, struct DerivedMesh *dm, float mat[][4], int timeoffset);
void surface_cache_free(struct RenderDB *rdb);

void surface_cache_sample(SurfaceCache *cache, struct ShadeInput *shi);

/* Irradiance Cache */

struct IrrCache;
typedef struct IrrCache IrrCache;

void irr_cache_create(struct Render *re, struct RenderPart *pa,
	struct RenderLayer *rl, struct ShadeSample *ssamp);
void irr_cache_free(struct Render *re, struct RenderPart *pa);

int irr_cache_lookup(struct Render *re, struct ShadeInput *shi,
	IrrCache *cache, float *ao, float env[3], float indirect[3],
	float P[3], float dPdu[3], float dPdv[3], float N[3], float bumpN[3], int do_sample);

void irr_cache_write(struct Render *re, struct IrrCache *cache);
IrrCache *irr_cache_read(struct Render *re, int thread);
void irr_cache_merge(struct Render *re, IrrCache *cache);

/* Radiosity Cache */

void radio_cache_create(struct Render *re, int thread);
void radio_cache_free(struct RenderDB *rdb, int thread);
int radio_cache_lookup(struct Render *re, struct ShadeInput *shi, float color[3], float raylength);
void radio_cache_add(struct Render *re, struct ShadeInput *shi, float color[3]);

#endif /* __RENDER_CACHE_H__ */

