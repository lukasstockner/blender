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
 * Contributor(s): Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_VOLUME_PRECACHE_H__
#define __RENDER_VOLUME_PRECACHE_H__

struct ObjectInstanceRen;
struct Render;
struct RenderDB;
 
/* Create/Free */

void volume_precache_create(struct Render *re);
void volume_precache_free(struct RenderDB *rdb);

/* Other */

int point_inside_volume_objectinstance(struct Render *re, struct ObjectInstanceRen *obi, float *co);

typedef struct VolumePrecache {
	int res[3];
	float *data_r;
	float *data_g;
	float *data_b;
} VolumePrecache;

#endif /* __RENDER_VOLUME_PRECACHE_H__ */

