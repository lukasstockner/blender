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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "opensubdiv_converter.h"

extern "C" {
#include "BKE_DerivedMesh.h"
#include "DNA_meshdata_types.h"
}

OsdBlenderConverter::OsdBlenderConverter(DerivedMesh *dm)
    : dm_(dm)
{
}

OpenSubdiv::Sdc::SchemeType OsdBlenderConverter::get_type() const
{
	return OpenSubdiv::Sdc::SCHEME_CATMARK;
}

OpenSubdiv::Sdc::Options OsdBlenderConverter::get_options() const
{
	OpenSubdiv::Sdc::Options options;
	options.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);
	return options;
}

int OsdBlenderConverter::get_num_faces() const
{
	return dm_->getNumPolys(dm_);
}

int OsdBlenderConverter::get_num_edges() const
{
	return dm_->getNumEdges(dm_);
}

int OsdBlenderConverter::get_num_verts() const
{
	return dm_->getNumVerts(dm_);
}

/* Face relationships. */
int OsdBlenderConverter::get_num_face_verts(int face) const
{
	MPoly *mp = dm_->getPolyArray(dm_);
	MPoly *mpoly = &mp[face];
	return mpoly->totloop;
}

const int *OsdBlenderConverter::get_face_verts(int face) const
{
	MLoop *ml = dm_->getLoopArray(dm_);
	MPoly *mp = dm_->getPolyArray(dm_);
	MPoly *mpoly = &mp[face];
	static int indices[64];
	for(int i = 0; i < mpoly->totloop; ++i) {
		indices[i] = ml[mpoly->loopstart + i].v;
	}
	return indices;
}

const int *OsdBlenderConverter::get_face_edges(int face) const
{
	MLoop *ml = dm_->getLoopArray(dm_);
	MPoly *mp = dm_->getPolyArray(dm_);
	MPoly *mpoly = &mp[face];
	static int indices[64];
	for(int i = 0; i < mpoly->totloop; ++i) {
		indices[i] = ml[mpoly->loopstart + i].e;
	}
	return indices;
}

/* Edge relationships. */
const int *OsdBlenderConverter::get_edge_verts(int edge) const
{
	MEdge *me = dm_->getEdgeArray(dm_);
	MEdge *medge = &me[edge];
	static int indices[64];
	indices[0] = medge->v1;
	indices[1] = medge->v2;
	return indices;
}

int OsdBlenderConverter::get_num_edge_faces(int edge) const
{
	MLoop *ml = dm_->getLoopArray(dm_);
	MPoly *mp = dm_->getPolyArray(dm_);
	int num = 0;
	for (int poly = 0; poly < dm_->getNumPolys(dm_); ++poly) {
		MPoly *mpoly = &mp[poly];
		for (int loop = 0; loop < mpoly->totloop; ++loop) {
			MLoop *mloop = &ml[loop + mpoly->loopstart];
			if (mloop->e == edge) {
				++num;
				break;
			}
		}
	}
	return num;
}

const int *OsdBlenderConverter::get_edge_faces(int edge) const
{
	static int indices[64];
	MLoop *ml = dm_->getLoopArray(dm_);
	MPoly *mp = dm_->getPolyArray(dm_);
	int num = 0;
	for (int poly = 0; poly < dm_->getNumPolys(dm_); ++poly) {
		MPoly *mpoly = &mp[poly];
		for (int loop = 0; loop < mpoly->totloop; ++loop) {
			MLoop *mloop = &ml[loop + mpoly->loopstart];
			if (mloop->e == edge) {
				indices[num++] = poly;
				break;
			}
		}
	}
	return indices;
}

/* Vertex relationships. */
int OsdBlenderConverter::get_num_vert_edges(int vert) const
{
	MEdge *me = dm_->getEdgeArray(dm_);
	int num = 0;
	for (int edge = 0; edge < dm_->getNumEdges(dm_); ++edge) {
		MEdge *medge = &me[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			++num;
		}
	}
	return num;
}

const int *OsdBlenderConverter::get_vert_edges(int vert) const
{
	static int indices[64];
	MEdge *me = dm_->getEdgeArray(dm_);
	int num = 0;
	for (int edge = 0; edge < dm_->getNumEdges(dm_); ++edge) {
		MEdge *medge = &me[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			indices[num++] = edge;
		}
	}
	return indices;
}

int OsdBlenderConverter::get_num_vert_faces(int vert) const
{
	MLoop *ml = dm_->getLoopArray(dm_);
	MPoly *mp = dm_->getPolyArray(dm_);
	int num = 0;
	for (int poly = 0; poly < dm_->getNumPolys(dm_); ++poly) {
		MPoly *mpoly = &mp[poly];
		for (int loop = 0; loop < mpoly->totloop; ++loop) {
			MLoop *mloop = &ml[loop + mpoly->loopstart];
			if (mloop->v == vert) {
				++num;
				break;
			}
		}
	}
	return num;
}

const int *OsdBlenderConverter::get_vert_faces(int vert) const
{
	static int indices[64];
	MLoop *ml = dm_->getLoopArray(dm_);
	MPoly *mp = dm_->getPolyArray(dm_);
	int num = 0;
	for (int poly = 0; poly < dm_->getNumPolys(dm_); ++poly) {
		MPoly *mpoly = &mp[poly];
		for (int loop = 0; loop < mpoly->totloop; ++loop) {
			MLoop *mloop = &ml[loop + mpoly->loopstart];
			if (mloop->v == vert) {
				indices[num++] = poly;
				break;
			}
		}
	}
	return indices;
}
