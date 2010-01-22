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
 * color, alpha, bxdf, emit, .. must be done between begin and end. */

void mat_shading_begin(struct Render *re, struct ShadeInput *shi,
	struct ShadeMaterial *smat);
void mat_shading_end(struct Render *re, struct ShadeMaterial *smat);

/* Color and Alpha access */

void mat_color(float color[3], struct ShadeMaterial *mat);
float mat_alpha(struct ShadeMaterial *mat);

/* BXDF Access */

#define BXDF_DIFFUSE		1
#define BXDF_SPECULAR		2
#define BXDF_TRANSMISSION	4
#define BXDF_REFLECTION		(BXDF_DIFFUSE|BXDF_SPECULAR)

void mat_bxdf_f(float bxdf[3],
	struct ShadeMaterial *mat, struct ShadeGeometry *geom, int thread, float lv[3], int flag);
void mat_bxdf_sample(float lv[3], float pdf[3],
	struct ShadeMaterial *mat, struct ShadeGeometry *geom, int flag, float r[2]);

/* Emission */

void mat_emit(float emit[3], struct ShadeMaterial *mat, struct ShadeGeometry *geom, int thread);

/* Displacement */

void mat_displacement(struct Render *re, struct ShadeInput *shi, float displacement[3]);

#endif /* __RENDER_MATERIAL_H__ */

