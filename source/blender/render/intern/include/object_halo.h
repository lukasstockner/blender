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

#ifndef __RENDER_OBJECT_HALO_H__
#define __RENDER_OBJECT_HALO_H__

struct DerivedMesh;
struct Material;
struct ObjectRen;
struct Render;
struct RenderCamera;
struct RenderDB;

/* Conversion */

struct HaloRen *render_object_halo_get(struct ObjectRen *obr, int nr);

struct HaloRen *halo_init(struct Render *re, struct ObjectRen *obr,
	struct Material *ma, float *vec, float *vec1, float *orco,
	float hasize, float vectsize, int seed);
struct HaloRen *halo_init_particle(struct Render *re, struct ObjectRen *obr,
	struct DerivedMesh *dm, struct Material *ma, float *vec, float *vec1,
	float *orco, float *uvco, float hasize, float vectsize, int seed);

/* Sorting and Projection */

void halos_sort(struct RenderDB *rdb, int totsort);
void halos_project(struct RenderDB *rdb,
	struct RenderCamera *cam, float xoffs, int xparts);

/* Flare Post Process */

void halos_render_flare(struct Render *re);

/* Structs */

typedef struct HaloRen {	
    short miny, maxy;
    float alfa, xs, ys, rad, radsq, sin, cos, co[3], no[3];
	float hard, b, g, r;
    int zs, zd;
    int zBufDist;	/* depth in the z-buffer coordinate system */
    char starpoints, type, add, tex;
    char linec, ringc, seed;
	short flarec; /* used to be a char. why ?*/
    float hasize;
    int pixels;
    unsigned int lay;
    struct Material *mat;
} HaloRen;

/* haloren->type: flags */
#define HA_ONLYSKY		1
#define HA_VECT			2
#define HA_XALPHA		4
#define HA_FLARECIRC	8

#endif /* __RENDER_OBJECT_HALO_H__ */

