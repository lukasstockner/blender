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

#ifndef __RENDER_CAMERA_H__
#define __RENDER_CAMERA_H__

struct Render;
struct RenderCamera;
struct rcti;
struct rctf;

/* Panorama */

float panorama_pixel_rot(struct Render *re);
void panorama_set_camera_params(struct Render *re, struct rcti *disprect, struct rctf *viewplane);

/* Conversion from Raster */

void camera_raster_to_view(struct RenderCamera *cam, float view[3], float x, float y);
void camera_raster_to_ray(struct RenderCamera *cam, float start[3], float vec[3], float x, float y);
void camera_raster_to_co(struct RenderCamera *cam, float co[3], float x, float y, int z);
void camera_raster_plane_to_co(struct RenderCamera *cam, float co[3], float dxco[3], float dyco[3],
	float view[3], float dxyview[2], float x, float y, float plane[4]);

/* Convert from World to Homomgenous */

void camera_halo_co_to_hoco(struct RenderCamera *cam, float hoco[4], float co[3]);
void camera_matrix_co_to_hoco(float winmat[][4], float hoco[4], float co[3]);
void camera_window_matrix(struct RenderCamera *cam, float winmat[][4]);
void camera_window_rect_bounds(int winx, int winy, struct rcti *rect, float bounds[4]);
void camera_hoco_to_zco(struct RenderCamera *cam, float zco[3], float hoco[4]);

int camera_hoco_test_clip(float hoco[4]);

/* Struct */

#include "DNA_vec_types.h"

typedef struct RenderCamera {
	rctf viewplane;			/* mapped on winx winy */
	float viewdx, viewdy;	/* size of 1 pixel */
	float ycor;				/* viewdy/viewdx */
	float clipcrop;			/* 2 pixel boundary to prevent clip when filter used */

	/* camera */
	int type;				/* persp/ortographic/panorama camera */
	float lens;				/* lens for perspective camera */
	
	/* matrices */
	float viewmat[4][4];	/* world to camera */
	float viewinv[4][4];	/* camera to world */
	float viewnmat[3][3];	/* world to camera for normals */
	float viewninv[3][3];	/* camera to world for normals */
	float winmat[4][4];		/* camera to raster */
	float viewzvec[3];		/* normalized z-axis of viewmat */

	/* total image size */
	int winx, winy;
	
	/* clippping */
	float clipsta;
	float clipend;

	/* panorama */
	float panophi, panosi, panoco, panodxp, panodxv;
} RenderCamera;

#endif /* __RENDER_CAMERA_H__ */

