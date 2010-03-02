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

#ifndef __RENDER_SAMPLER_H__
#define __RENDER_SAMPLER_H__

struct Render;

typedef struct QMCSampler QMCSampler;

#define SAMP_TYPE_HALTON		1
#define SAMP_TYPE_HAMMERSLEY	2

/* Global Init/Free  */

void samplers_init(struct Render *re);
void samplers_free(struct Render *re);

/* Acquire/Release per Thread */

struct QMCSampler *sampler_acquire(struct Render *re, int thread, int type, int tot);
void sampler_release(struct Render *re, struct QMCSampler *qsa);

/* 2D Sampling */

void sampler_get_float_2d(float s[2], struct QMCSampler *qsa, int num);
void sampler_get_double_2d(double s[2], struct QMCSampler *qsa, int num);

/* Projection Utities */

void sample_project_hemi(float vec[3], float s[2]);
void sample_project_hemi_cosine_weighted(float vec[3], float s[2]);
void sample_project_disc(float vec[3], float radius, float s[2]);
void sample_project_rect(float vec[3], float sizex, float sizey, float s[2]);
void sample_project_phong(float vec[3], float blur, float s[2]);

#endif /* __RENDER_SAMPLER_H__ */

