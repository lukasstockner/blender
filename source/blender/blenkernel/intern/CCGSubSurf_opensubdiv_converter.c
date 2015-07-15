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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/CCGSubSurf_opensubdiv.c
 *  \ingroup bke
 */

#ifdef WITH_OPENSUBDIV

#include <stdlib.h>

#include "MEM_guardedalloc.h"
#include "BLI_sys_types.h" // for intptr_t support

#include "BLI_utildefines.h" /* for BLI_assert */
#include "BLI_math.h"

#include "CCGSubSurf.h"
#include "CCGSubSurf_intern.h"

#include "BKE_DerivedMesh.h"

#include "opensubdiv_capi.h"
#include "opensubdiv_converter_capi.h"

/* TODO(sergey): Optimize this by using mesh_map, so we don't
 * do full mesh lookup for every geometry primitive.
 */

static OpenSubdiv_SchemeType conv_dm_get_catmark_type(
        const OpenSubdiv_Converter *UNUSED(converter))
{
	return OSD_SCHEME_CATMARK;
}

static OpenSubdiv_SchemeType conv_dm_get_bilinear_type(
        const OpenSubdiv_Converter *UNUSED(converter))
{
	return OSD_SCHEME_BILINEAR;
}

static int conv_dm_get_num_faces(const OpenSubdiv_Converter *converter)
{
	DerivedMesh *dm = converter->user_data;
	return dm->getNumPolys(dm);
}

static int conv_dm_get_num_edges(const OpenSubdiv_Converter *converter)
{
	DerivedMesh *dm = converter->user_data;
	return dm->getNumEdges(dm);
}

static int conv_dm_get_num_verts(const OpenSubdiv_Converter *converter)
{
	DerivedMesh *dm = converter->user_data;
	return dm->getNumVerts(dm);
}

static int conv_dm_get_num_face_verts(const OpenSubdiv_Converter *converter,
                                      int face)
{
	DerivedMesh *dm = converter->user_data;
	const MPoly *mp = dm->getPolyArray(dm);
	const MPoly *mpoly = &mp[face];
	return mpoly->totloop;
}

static void conv_dm_get_face_verts(const OpenSubdiv_Converter *converter,
                                   int face,
                                   int *face_verts)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	const MPoly *mpoly = &mp[face];
	int loop;
	for(loop = 0; loop < mpoly->totloop; loop++) {
		face_verts[loop] = ml[mpoly->loopstart + loop].v;
	}
}

static void conv_dm_get_face_edges(const OpenSubdiv_Converter *converter,
                                   int face,
                                   int *face_edges)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	const MPoly *mpoly = &mp[face];
	int loop;
	for(loop = 0; loop < mpoly->totloop; loop++) {
		face_edges[loop] = ml[mpoly->loopstart + loop].e;
	}
}

static void conv_dm_get_edge_verts(const OpenSubdiv_Converter *converter,
                                   int edge,
                                   int *edge_verts)
{
	DerivedMesh *dm = converter->user_data;
	const MEdge *me = dm->getEdgeArray(dm);
	const MEdge *medge = &me[edge];
	edge_verts[0] = medge->v1;
	edge_verts[1] = medge->v2;
}

static int conv_dm_get_num_edge_faces(const OpenSubdiv_Converter *converter,
                                      int edge)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &mp[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->e == edge) {
				++num;
				break;
			}
		}
	}
	return num;
}

static void conv_dm_get_edge_faces(const OpenSubdiv_Converter *converter,
                                   int edge,
                                   int *edge_faces)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &mp[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->e == edge) {
				edge_faces[num++] = poly;
				break;
			}
		}
	}
}

static int conv_dm_get_num_vert_edges(const OpenSubdiv_Converter *converter,
                                      int vert)
{
	DerivedMesh *dm = converter->user_data;
	const MEdge *me = dm->getEdgeArray(dm);
	int num = 0, edge;
	for (edge = 0; edge < dm->getNumEdges(dm); edge++) {
		const MEdge *medge = &me[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			++num;
		}
	}
	return num;
}

static void conv_dm_get_vert_edges(const OpenSubdiv_Converter *converter,
                                   int vert,
                                   int *vert_edges)
{
	DerivedMesh *dm = converter->user_data;
	const MEdge *me = dm->getEdgeArray(dm);
	int num = 0, edge;
	for (edge = 0; edge < dm->getNumEdges(dm); edge++) {
		const MEdge *medge = &me[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			vert_edges[num++] = edge;
		}
	}
}

static int conv_dm_get_num_vert_faces(const OpenSubdiv_Converter *converter,
                                      int vert)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &mp[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->v == vert) {
				++num;
				break;
			}
		}
	}
	return num;
}

static void conv_dm_get_vert_faces(const OpenSubdiv_Converter *converter,
                                   int vert,
                                   int *vert_faces)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &mp[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->v == vert) {
				vert_faces[num++] = poly;
				break;
			}
		}
	}
}

void ccgSubSurf_converter_setup_from_derivedmesh(
        CCGSubSurf *ss,
        DerivedMesh *dm,
        OpenSubdiv_Converter *converter)
{
	if (ss->meshIFC.simpleSubdiv)
		converter->get_type = conv_dm_get_bilinear_type;
	else
		converter->get_type = conv_dm_get_catmark_type;

	converter->get_num_faces = conv_dm_get_num_faces;
	converter->get_num_edges = conv_dm_get_num_edges;
	converter->get_num_verts = conv_dm_get_num_verts;

	converter->get_num_face_verts = conv_dm_get_num_face_verts;
	converter->get_face_verts = conv_dm_get_face_verts;
	converter->get_face_edges = conv_dm_get_face_edges;

	converter->get_edge_verts = conv_dm_get_edge_verts;
	converter->get_num_edge_faces = conv_dm_get_num_edge_faces;
	converter->get_edge_faces = conv_dm_get_edge_faces;

	converter->get_num_vert_edges = conv_dm_get_num_vert_edges;
	converter->get_vert_edges = conv_dm_get_vert_edges;
	converter->get_num_vert_faces = conv_dm_get_num_vert_faces;
	converter->get_vert_faces = conv_dm_get_vert_faces;

	converter->user_data = dm;
}

/* TODO(sergey): Implement converter from CCGSubSurf. */

#endif  /* WITH_OPENSUBDIV */
