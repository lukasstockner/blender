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
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef OCCLUSION_H
#define OCCLUSION_H

struct ObjectRen;
struct Render;
struct RenderDB;
struct RenderPart;
struct ShadeInput;
struct ShadeSample;

/* Disk Occlusion Create/Free */

void disk_occlusion_create(struct Render *re);
void disk_occlusion_free(struct RenderDB *rdb);

/* Sample */

void disk_occlusion_sample(struct Render *re, struct ShadeInput *shi);
void disk_occlusion_sample_direct(struct Render *re, struct ShadeInput *shi);

/* Part Cache */

void disk_occlusion_cache_create(struct Render *re, struct RenderPart *pa, struct ShadeSample *ssamp);
void disk_occlusion_cache_free(struct Render *re, struct RenderPart *pa);

#endif

