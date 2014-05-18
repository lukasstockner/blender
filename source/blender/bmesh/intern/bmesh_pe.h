/*
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
* Contributor(s): Grigory Revzin.
*
* ***** END GPL LICENSE BLOCK *****
*/

#ifndef __BMESH_PE_H__
#define __BMESH_PE_H__

typedef struct BMesh BMesh;

/* both of these calculate distances to closest selected vertices - after any kind of transform defined by the matrix */

/* the distance is calculated along the surface from the selected vertices */
void BM_prop_dist_calc_connected(BMesh *bm, float loc_to_world_mtx[3][3], float *dists);

/* the distance is calculated either by a bounding sphere or a cicle 
 * in the projection plane from the closest selected vertex */
void BM_prop_dist_calc(BMesh *bm, float loc_to_world_mtx[3][3], float proj_plane_n[3], float dists[]);

#endif
