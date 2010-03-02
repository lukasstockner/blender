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
 * Contributor(s): Raul Fernandez Hernandez (Farsthary), Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef VOXELDATA_H
#define VOXELDATA_H 

struct Render;
struct TexResult;

/* Voxeldata Texture */

void tex_voxeldata_init(struct Render *re, struct Tex *tex);
void tex_voxeldata_free(struct Render *re, struct Tex *tex);

int tex_voxeldata_sample(struct Tex *tex, float *texvec, struct TexResult *texres);

#endif /* VOXELDATA_H */
