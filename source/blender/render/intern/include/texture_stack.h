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

#ifndef TEXTURE_STACK_H
#define TEXTURE_STACK_H

struct HaloRen;
struct Image;
struct LampRen;
struct ShadeInput;
struct Tex;
struct TexResult;

/* Texture Stacks */

void do_material_tex(struct Render *re, struct ShadeInput *shi);
void do_lamp_tex(struct Render *re, struct LampRen *la, float *lavec,
	struct ShadeInput *shi, float *colf, int effect);
void do_sky_tex(struct Render *re, float *rco, float *lo, float *dxyview,
	float *hor, float *zen, float *blend, int skyflag, short thread);
void do_halo_tex(struct Render *re, struct HaloRen *har, float xn, float yn,
	float *colf);
void do_volume_tex(struct Render *re, struct ShadeInput *shi, float *xyz,
	int mapto_flag, float *col, float *val);

/* TexFace Sample */

void do_realtime_texture(struct RenderParams *rpm, struct ShadeInput *shi,
	struct Image *ima);

#endif /* TEXTURE_STACK_H */

