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

#ifndef __RENDER_ENVIRONMENT_H__
#define __RENDER_ENVIRONMENT_H__

struct Lamp;
struct LampRen;
struct Render;
struct SunSky;
struct World;

/* Init/Free */

void environment_init(struct Render *re, struct World *world);
void environment_free(struct Render *re);

/* Sun/Sky */

void environment_sun_init(struct LampRen *lar, struct Lamp *la, float obmat[4][4]);
void environment_sun_free(struct LampRen *lar);

/* Shading */

void environment_shade(struct Render *re, float col[3], float co[3],
	float view[3], float dxyview[2], int thread);
void environment_no_tex_shade(struct Render *re, float col[3], float view[3]);
void environment_shade_pixel(struct Render *re, float col[4],
	float fx, float fy, int thread);

/* Mist */

float environment_mist_factor(struct Render *re, float zcor, float *co);

/* Atmosphere */

void atmosphere_shade_pixel(struct Render *re, struct SunSky *sunsky,
	float *col, float fx, float fy, float distance);

#endif /* __RENDER_ENVIRONMENT_H__ */

