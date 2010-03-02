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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Full recode: 2004-2006 Blender Foundation
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef __RENDER_ZBUF_H__
#define __RENDER_ZBUF_H__

struct APixstr;
struct APixstrand;
struct LampRen;
struct ListBase;
struct RenderLayer;
struct RenderPart;
struct StrandShadeCache;
struct VlakRen;
struct ZSpan;

/* Render Part Rasterization */

void zbuffer_solid(struct Render *re, struct RenderPart *pa, struct RenderLayer *rl,
	struct ListBase *psmlist,
	void (*edgefunc)(struct Render *re, struct RenderPart *pa, float *rectf, int *rectz),
	float *edgerect);

int zbuffer_alpha(struct Render *re, struct RenderPart *pa, struct RenderLayer *rl);

/* Shadow Rasterization */

void zbuffer_shadow(struct Render *re, float winmat[][4], struct LampRen *lar,
	int *rectz, int size, float jitx, float jity);
void zbuffer_abuf_shadow(struct Render *re, struct LampRen *lar, float winmat[][4],
	struct APixstr *APixbuf, struct APixstrand *apixbuf, struct ListBase *apsmbase,
	int size, int samples, float (*jit)[2]);

/* SSS Rasterization */

void zbuffer_sss(struct Render *re, struct RenderPart *pa, unsigned int lay,
	void *handle, void (*func)(void*, int, int, int, int, int));

/* Pixel Struct Utilities */

void free_pixel_structs(struct ListBase *lb);
void free_alpha_pixel_structs(struct ListBase *lb);

/* Z-Span rasterization methods for filling triangles. */

typedef struct ZSpan {
	/* Rasterization Data */
	int rectx, recty;						/* range for clipping */
	
	int miny1, maxy1, miny2, maxy2;			/* actual filled in range */
	float *minp1, *maxp1, *minp2, *maxp2;	/* vertex pointers detect min/max range in */
	float *span1, *span2;
	
	float zmulx, zmuly, zofsx, zofsy;		/* transform from hoco to zbuf co */

	/* Per Rasterization Type Data */
	int *rectz, *arectz;					/* zbuffers, arectz is for transparant */
	int *rectz1;							/* seconday z buffer for shadowbuffer (2nd closest z) */
	int *rectp;								/* polygon index buffer */
	int *recto;								/* object buffer */
	int *rectmask;							/* negative zmask buffer */
	struct APixstr *apixbuf, *curpstr;		/* apixbuf for transparent */
	struct APixstrand *curpstrand;			/* same for strands */
	struct ListBase *apsmbase;
	
	int polygon_offset;						/* offset in Z */
	float shad_alpha;						/* copy from material, used by irregular shadbuf */
	int mask, apsmcounter;					/* in use by apixbuf */
	int apstrandmcounter;

	float clipcrop;							/* for shadow */

	void *sss_handle;						/* used by sss */
	void (*sss_func)(void *, int, int, int, int, int);
	
	void *isb_re;							/* used by ISB */
	void (*zbuffunc)(struct ZSpan *, int, int, float *, float *, float *, float *);
	void (*zbuflinefunc)(struct ZSpan *, int, int, float *, float *);
	
} ZSpan;

/* Allocate/Free */

void zbuf_alloc_span(struct ZSpan *zspan, int rectx, int recty, float clipcrop);
void zbuf_free_span(struct ZSpan *zspan);

/* Initialize */

void zbuf_init_span(struct ZSpan *zspan);
void zbuf_add_to_span(struct ZSpan *zspan, float *v1, float *v2);

/* Polygon Clipping */

void zbufclip(struct ZSpan *zspan, int obi, int zvlnr,
	float *f1, float *f2, float *f3, int c1, int c2, int c3);
void zbufclipwire(struct ZSpan *zspan, int obi, int zvlnr,
	int ec, float *ho1, float *ho2, float *ho3, float *ho4, int c1, int c2, int c3, int c4);
void zbufclip4(struct ZSpan *zspan, int obi, int zvlnr,
	float *f1, float *f2, float *f3, float *f4, int c1, int c2, int c3, int c4);

/* Scan Conversion with Callback */

void zspan_scanconvert(struct ZSpan *zpan, void *handle,
	float *v1, float *v2, float *v3,
	void (*func)(void *, int, int, float, float));

#endif /* __RENDER_ZBUF_H__ */

