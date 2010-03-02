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

#ifndef TEXTURE_H
#define TEXTURE_H

struct Tex;
struct TexResult;
struct Image;
struct ImBuf;
struct Render;
struct RenderParams;
struct ListBase;

typedef struct TexCoord {
	float co[3];
	float dx[3];
	float dy[3];
	int osatex;
} TexCoord;

/* Textures */

void tex_init(struct Render *re, struct Tex *tex);
void tex_free(struct Render *re, struct Tex *tex);

void tex_list_init(struct Render *re, struct ListBase *lb);
void tex_list_free(struct Render *re, struct ListBase *lb);

/*	0: intensity
	TEX_RGB: color
	TEX_NOR: normal
	TEX_RGB|TEX_NOR: everything */

int tex_sample(struct RenderParams *rpm, struct Tex *tex, TexCoord *texco,
	struct TexResult *texres, short thread, short which_output);

/* Image Texture */

int imagewraposa(struct RenderParams *rpm, struct Tex *tex, struct Image *ima, struct ImBuf *ibuf,
	float *texvec, float *dxt, float *dyt, struct TexResult *texres);
int imagewrap(struct RenderParams *rpm, struct Tex *tex, struct Image *ima, struct ImBuf *ibuf,
	float *texvec, struct TexResult *texres);
void image_sample(struct RenderParams *rpm, struct Image *ima, float fx, float fy,
	float dx, float dy, float *result);

/* Utilities */

void tex_brightness_contrast(struct Tex *tex, struct TexResult *texres);
void tex_brightness_contrast_rgb(struct Tex *tex, struct TexResult *texres);

#endif /* TEXTURE_H */

