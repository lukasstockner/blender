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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef SHADBUF_EXT_H
#define SHADBUF_EXT_H

struct APixstr;
struct LampRen;
struct Render;
struct RenderPart;
struct ShadBuf;
struct ShadeInput;

/* Shadow buffer data structure create/free. */

void shadowbuf_create(struct Render *re, struct LampRen *lar, float mat[][4]);
void shadowbuf_free(struct LampRen *lar);

/* Render shadow buffers. */ 

void shadowbufs_make_threaded(struct Render *re);

/* Test shadow factor for a receiving point:
   * returns 1.0 for no shadow, 0.0 for complete shadow.
   * inp is the dot product between lamp vector and normal,
     to hide bias issues at grazing angles */

float shadowbuf_test(struct Render *re, struct ShadBuf *shb,
	float *rco, float *dxco, float *dyco, float inp, float mat_bias);	

/* Determine shadow facetor between two points */

float shadow_halo(struct LampRen *lar, float *p1, float *p2);

/* Utility for Envmap */

void shadowbuf_rotate(struct LampRen *lar, float mat[][4], int restore);

/* Irregular shadow buffer, created/freed per tile */

void irregular_shadowbuf_create(struct Render *re, struct RenderPart *pa,
	struct APixstr *apixbuf);
void irregular_shadowbuf_free(struct Render *re, struct RenderPart *pa);

float irregular_shadowbuf_test(struct Render *re, struct ShadBuf *shb,
	struct ShadeInput *shi);

#endif /* SHADBUF_EXT_H */

