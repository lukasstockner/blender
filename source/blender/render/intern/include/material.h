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
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_MATERIAL_H__
#define __RENDER_MATERIAL_H__

struct Render;
struct ShadeInput;
struct ShadeGeometry;
struct ShadeMaterial;

/* Material
 *
 * Begin/end shading for a given location and view vector. Getting
 * color, alpha, bsdf, emit, .. must be done between begin and end. */

void mat_shading_begin(struct Render *re, struct ShadeInput *shi,
	struct ShadeMaterial *smat);
void mat_shading_end(struct Render *re, struct ShadeMaterial *smat);

/* BSDF: combination of brdf and btdf

   note that we include the cos(geom->vn, lv) term in the bsdf. this
   makes it easier to implement bsdf's that use some approximation
   tricks. also note that for physically correct results (energy
   conversvation) the returned values would be in the range 0 to 1/pi,
   and have unit 1/sr */

#define BSDF_DIFFUSE		1
#define BSDF_SPECULAR		2
#define BSDF_REFLECTION		(BSDF_DIFFUSE|BSDF_SPECULAR)
#define BSDF_TRANSMISSION	4

void mat_bsdf_f(float bsdf[3],
	struct ShadeMaterial *mat, struct ShadeGeometry *geom, int thread, float lv[3], int flag);
void mat_bsdf_sample(float lv[3], float pdf[3],
	struct ShadeMaterial *mat, struct ShadeGeometry *geom, int flag, float r[2]);

/* Color: diffuse part of bsdf integrated over hemisphere, result
   will be in the range 0 to 1 for physically correct results */

void mat_color(float color[3], struct ShadeMaterial *mat);

/* Color: transmission part of bsdf integrated over hemisphere, result
   will be in the range 0 to 1 for physically correct results. 
   converted to grayscale currently since rasterizer does not support
   opacity values per RGB channel. */

float mat_alpha(struct ShadeMaterial *mat);

/* Emission: light emitted in direction geom->view
   unit is luminance, lm/(m^2.sr) */

void mat_emit(float emit[3], struct ShadeMaterial *mat, struct ShadeGeometry *geom, int thread);

/* Displacement in camere space coordinates at current location */

void mat_displacement(struct Render *re, struct ShadeInput *shi, float displacement[3]);

#endif /* __RENDER_MATERIAL_H__ */

