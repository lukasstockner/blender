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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: none of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef STRAND_H
#define STRAND_H 

struct APixstrand;
struct StrandVert;
struct StrandRen;
struct StrandBuffer;
struct ShadeSample;
struct StrandPart;
struct Render;
struct RenderPart;
struct RenderDB;
struct ZSpan;
struct ObjectInstanceRen;
struct SurfaceCache;
struct DerivedMesh;
struct ObjectRen;

typedef struct StrandPoint {
	/* position within segment */
	float t;

	/* camera space */
	float co[3];
	float nor[3];
	float tan[3];
	float strandco;
	float width;

	/* derivatives */
	float dtco[3], dsco[3];
	float dtstrandco;

	/* outer points */
	float co1[3], co2[3];
	float hoco1[4], hoco2[4];
	float zco1[3], zco2[3];
	int clip1, clip2;

	/* screen space */
	float hoco[4];
	float x, y;

	/* simplification */
	float alpha;
} StrandPoint;

typedef struct StrandSegment {
	struct StrandVert *v[4];
	struct StrandRen *strand;
	struct StrandBuffer *buffer;
	struct ObjectInstanceRen *obi;
	float sqadaptcos;

	StrandPoint point1, point2;
	int shaded;
} StrandSegment;

struct StrandShadeCache;
typedef struct StrandShadeCache StrandShadeCache;

void strand_eval_point(StrandSegment *sseg, StrandPoint *spoint);
void render_strand_segment(struct Render *re, float winmat[][4], struct StrandPart *spart, struct ZSpan *zspan, int totzspan, StrandSegment *sseg);
void strand_minmax(struct StrandRen *strand, float *min, float *max);

struct SurfaceCache *cache_strand_surface(struct Render *re, struct ObjectRen *obr, struct DerivedMesh *dm, float mat[][4], int timeoffset);
void free_strand_surface(struct RenderDB *rdb);

struct StrandShadeCache *strand_shade_cache_create(void);
void strand_shade_cache_free(struct StrandShadeCache *cache);
void strand_shade_segment(struct Render *re, struct StrandShadeCache *cache, struct StrandSegment *sseg, struct ShadeSample *ssamp, float t, float s, int addpassflag);
void strand_shade_unref(struct StrandShadeCache *cache, struct StrandVert *svert);

struct StrandRen *render_object_strand_get(struct ObjectRen *obr, int nr);
struct StrandBuffer *render_object_strand_buffer_add(struct ObjectRen *obr, int totvert);

float *render_strand_get_surfnor(struct ObjectRen *obr, struct StrandRen *strand, int verify);
float *render_strand_get_uv(struct ObjectRen *obr, struct StrandRen *strand, int n, char **name, int verify);
struct MCol *render_strand_get_mcol(struct ObjectRen *obr, struct StrandRen *strand, int n, char **name, int verify);
float *render_strand_get_simplify(struct ObjectRen *obr, struct StrandRen *strand, int verify);
int *render_strand_get_face(struct ObjectRen *obr, struct StrandRen *strand, int verify);
float *render_strand_get_winspeed(struct ObjectInstanceRen *obi, struct StrandRen *strand, int verify);

int zbuffer_strands_abuf(struct Render *re, struct RenderPart *pa, struct APixstrand *apixbuf, struct ListBase *apsmbase, unsigned int lay, int negzmask, float winmat[][4], int winx, int winy, int sample, float (*jit)[2], float clipcrop, int shadow, struct StrandShadeCache *cache);

typedef struct StrandTableNode {
	struct StrandRen *strand;
	float *winspeed;
	float *surfnor;
	float *simplify;
	int *face;
	struct MCol *mcol;
	float *uv;
	int totuv, totmcol;
} StrandTableNode;

typedef struct StrandVert {
	float co[3];
	float strandco;
} StrandVert;

typedef struct StrandBound {
	int start, end;
	float boundbox[2][3];
} StrandBound;

typedef struct StrandBuffer {
	struct StrandBuffer *next, *prev;
	struct StrandVert *vert;
	struct StrandBound *bound;
	int totvert, totbound;

	struct ObjectRen *obr;
	struct Material *ma;
	struct SurfaceCache *surface;
	unsigned int lay;
	int overrideuv;
	int flag, maxdepth;
	float adaptcos, minwidth, widthfade;

	float winmat[4][4];
	int winx, winy;
} StrandBuffer;

typedef struct StrandRen {
	StrandVert *vert;
	StrandBuffer *buffer;
	int totvert, flag;
	int clip, index;
	float orco[3];
} StrandRen;

/* strandbuffer->flag */
#define R_STRAND_BSPLINE	1
#define R_STRAND_B_UNITS	2

#endif

