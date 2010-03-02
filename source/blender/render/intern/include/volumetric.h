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
 * Contributor(s): Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_VOLUME_H__
#define __RENDER_VOLUME_H__

struct Material;
struct ObjectInstanceRen;
struct ObjectRen;
struct Render;
struct ShadeInput;
struct ShadeResult;

float vol_get_density(Render *re, ShadeInput *shi, float *co);
void vol_get_scattering(Render *re, ShadeInput *shi, float *scatter_col, float *co_);

void shade_volume_outside(Render *re, ShadeInput *shi, ShadeResult *shr);
void shade_volume_inside(Render *re, ShadeInput *shi, ShadeResult *shr);
void shade_volume_shadow(Render *re, ShadeInput *shi, ShadeResult *shr, struct Isect *last_is);

#define STEPSIZE_VIEW	0
#define STEPSIZE_SHADE	1

#define VOL_IS_BACKFACE			1
#define VOL_IS_SAMEMATERIAL		2

#define VOL_BOUNDS_DEPTH	0
#define VOL_BOUNDS_SS		1

#define VOL_SHADE_OUTSIDE	0
#define VOL_SHADE_INSIDE	1

typedef struct VolumeOb {
	struct VolumeOb *next, *prev;
	struct Material *ma;
	struct ObjectRen *obr;
} VolumeOb;

typedef struct MatInside {
	struct MatInside *next, *prev;
	struct Material *ma;
	struct ObjectInstanceRen *obi;
} MatInside;

#endif /* __RENDER_VOLUME_H__ */

