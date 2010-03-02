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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_ENVMAP_H__
#define __RENDER_ENVMAP_H__ 

struct Render;
struct RenderParams;
struct TexResult;

/* Make/Render Environment Map */

void envmaps_make(struct Render *re);

/* Texture Mapping Utility */

void envmap_map(struct Render *re, struct Tex *tex, float *texvec,
	float *dxt, float *dyt, int osatex);

/* Texture Access */

void tex_envmap_init(struct Render *re, struct Tex *tex);
int tex_envmap_sample(struct RenderParams *rpm, struct Tex *tex, float *texvec,
	float *dxt, float *dyt, int osatex, struct TexResult *texres);

#endif /* __RENDER_ENVMAP_H__ */

