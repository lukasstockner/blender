/* $Id$ 
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_SUBSURF_H
#define BKE_SUBSURF_H

struct DMGridAdjacency;
struct DMGridData;
struct DerivedMesh;
struct EditMesh;
struct IndexNode;
struct ListBase;
struct Mesh;
struct MultiresSubsurf;
struct Object;
struct PBVH;
struct SubsurfModifierData;
struct _CCGEdge;
struct _CCGFace;
struct _CCGSubsurf;
struct _CCGVert;

/**************************** External *****************************/

/* Grids */

/* Each grid element can contain zero or more layers of coordinates,
   paint masks, and normals; these numbers are stored in the GridKey

   For now, co and no can have only zero or one layers, only mask is
   really variable.
*/
struct GridKey {
	int co;
	int mask;
	int no;
};

#define GRIDELEM_KEY_INIT(_gridkey, _totco, _totmask, _totno)	\
	(_gridkey->co = _totco, _gridkey->mask = _totmask, _gridkey->no = _totno)

#define GRIDELEM_SIZE(_key) ((3*_key->co + _key->mask + 3*_key->no) * sizeof(float))
#define GRIDELEM_MASK_OFFSET(_key) (_key->mask ? 3*_key->co*sizeof(float) : -1)
#define GRIDELEM_NO_OFFSET(_key) (_key->no ? (3*_key->co + _key->mask) * sizeof(float) : -1)
#define GRIDELEM_INTERP_COUNT(_key) (3*_key->co + _key->mask)

#define GRIDELEM_AT(_grid, _elem, _key) ((struct DMGridData*)(((char*)(_grid)) + (_elem) * GRIDELEM_SIZE(_key)))
#define GRIDELEM_INC(_grid, _inc, _key) ((_grid) = GRIDELEM_AT(_grid, _inc, _key))

#define GRIDELEM_CO(_grid, _key) ((float*)(_grid))
#define GRIDELEM_NO(_grid, _key) ((float*)((char*)(_grid) + GRIDELEM_NO_OFFSET(_key)))
#define GRIDELEM_MASK(_grid, _key) ((float*)((char*)(_grid) + GRIDELEM_MASK_OFFSET(_key)))

#define GRIDELEM_CO_AT(_grid, _elem, _key) GRIDELEM_CO(GRIDELEM_AT(_grid, _elem, _key), _key)
#define GRIDELEM_NO_AT(_grid, _elem, _key) GRIDELEM_NO(GRIDELEM_AT(_grid, _elem, _key), _key)
#define GRIDELEM_MASK_AT(_grid, _elem, _key) GRIDELEM_MASK(GRIDELEM_AT(_grid, _elem, _key), _key)

struct DerivedMesh *subsurf_make_derived_from_derived(
						struct DerivedMesh *dm,
						struct SubsurfModifierData *smd,
						struct GridKey *gridkey,
						int useRenderParams, float (*vertCos)[3],
						int isFinalCalc, int editMode);

void subsurf_calculate_limit_positions(struct Mesh *me, float (*positions_r)[3]);

/**************************** Internal *****************************/

typedef struct CCGDerivedMesh {
	DerivedMesh dm;

	struct _CCGSubSurf *ss;
	int freeSS;
	int drawInteriorEdges, useSubsurfUv;

	struct {int startVert; struct _CCGVert *vert;} *vertMap;
	struct {int startVert; int startEdge; struct _CCGEdge *edge;} *edgeMap;
	struct {int startVert; int startEdge;
			int startFace; struct _CCGFace *face;} *faceMap;

	short *edgeFlags;
	char *faceFlags;

	struct PBVH *pbvh;
	int pbvh_draw;
	struct ListBase *fmap;
	struct IndexNode *fmap_mem;

	struct DMGridData **gridData;
	struct DMGridAdjacency *gridAdjacency;
	int *gridOffset;
	struct _CCGFace **gridFaces;

	struct {
		struct MultiresModifierData *mmd;
		int local_mmd;

		int lvl, totlvl;
		float (*orco)[3];

		struct Object *ob;
		int modified;

		void (*update)(DerivedMesh*);
	} multires;
} CCGDerivedMesh;

#endif

