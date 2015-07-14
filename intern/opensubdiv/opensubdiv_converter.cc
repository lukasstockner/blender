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

#include <opensubdiv/far/topologyRefinerFactory.h>

#include "opensubdiv_converter_capi.h"
#include "opensubdiv_intern.h"

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
	options.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_ALL);
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

void OsdBlenderConverter::get_coarse_verts(float *coords) const
{
	MVert *mv = dm_->getVertArray(dm_);
	const int num_verts = dm_->getNumVerts(dm_);
	for (int i = 0; i < num_verts; ++i) {
		MVert *mvert = &mv[i];
		coords[i * 3 + 0] = mvert->co[0];
		coords[i * 3 + 1] = mvert->co[1];
		coords[i * 3 + 2] = mvert->co[2];
	}
}

/* Face relationships. */
int OsdBlenderConverter::get_num_face_verts(int face) const
{
	const MPoly *mp = dm_->getPolyArray(dm_);
	const MPoly *mpoly = &mp[face];
	return mpoly->totloop;
}

void OsdBlenderConverter::get_face_verts(int face, int *face_verts) const
{
	const MLoop *ml = dm_->getLoopArray(dm_);
	const MPoly *mp = dm_->getPolyArray(dm_);
	const MPoly *mpoly = &mp[face];
	for(int i = 0; i < mpoly->totloop; ++i) {
		face_verts[i] = ml[mpoly->loopstart + i].v;
	}
}

void OsdBlenderConverter::get_face_edges(int face, int *face_edges) const
{
	const MLoop *ml = dm_->getLoopArray(dm_);
	const MPoly *mp = dm_->getPolyArray(dm_);
	const MPoly *mpoly = &mp[face];
	for(int i = 0; i < mpoly->totloop; ++i) {
		face_edges[i] = ml[mpoly->loopstart + i].e;
	}
}

/* Edge relationships. */
void OsdBlenderConverter::get_edge_verts(int edge, int *edge_verts) const
{
	const MEdge *me = dm_->getEdgeArray(dm_);
	const MEdge *medge = &me[edge];
	edge_verts[0] = medge->v1;
	edge_verts[1] = medge->v2;
}

int OsdBlenderConverter::get_num_edge_faces(int edge) const
{
	const MLoop *ml = dm_->getLoopArray(dm_);
	const MPoly *mp = dm_->getPolyArray(dm_);
	int num = 0;
	for (int poly = 0; poly < dm_->getNumPolys(dm_); ++poly) {
		const MPoly *mpoly = &mp[poly];
		for (int loop = 0; loop < mpoly->totloop; ++loop) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->e == edge) {
				++num;
				break;
			}
		}
	}
	return num;
}

void OsdBlenderConverter::get_edge_faces(int edge, int *edge_faces) const
{
	const MLoop *ml = dm_->getLoopArray(dm_);
	const MPoly *mp = dm_->getPolyArray(dm_);
	int num = 0;
	for (int poly = 0; poly < dm_->getNumPolys(dm_); ++poly) {
		const MPoly *mpoly = &mp[poly];
		for (int loop = 0; loop < mpoly->totloop; ++loop) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->e == edge) {
				edge_faces[num++] = poly;
				break;
			}
		}
	}
}

/* Vertex relationships. */
int OsdBlenderConverter::get_num_vert_edges(int vert) const
{
	const MEdge *me = dm_->getEdgeArray(dm_);
	int num = 0;
	for (int edge = 0; edge < dm_->getNumEdges(dm_); ++edge) {
		const MEdge *medge = &me[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			++num;
		}
	}
	return num;
}

void OsdBlenderConverter::get_vert_edges(int vert, int *vert_edges) const
{
	const MEdge *me = dm_->getEdgeArray(dm_);
	int num = 0;
	for (int edge = 0; edge < dm_->getNumEdges(dm_); ++edge) {
		const MEdge *medge = &me[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			vert_edges[num++] = edge;
		}
	}
}

int OsdBlenderConverter::get_num_vert_faces(int vert) const
{
	const MLoop *ml = dm_->getLoopArray(dm_);
	const MPoly *mp = dm_->getPolyArray(dm_);
	int num = 0;
	for (int poly = 0; poly < dm_->getNumPolys(dm_); ++poly) {
		const MPoly *mpoly = &mp[poly];
		for (int loop = 0; loop < mpoly->totloop; ++loop) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->v == vert) {
				++num;
				break;
			}
		}
	}
	return num;
}

void OsdBlenderConverter::get_vert_faces(int vert, int *vert_faces) const
{
	const MLoop *ml = dm_->getLoopArray(dm_);
	const MPoly *mp = dm_->getPolyArray(dm_);
	int num = 0;
	for (int poly = 0; poly < dm_->getNumPolys(dm_); ++poly) {
		const MPoly *mpoly = &mp[poly];
		for (int loop = 0; loop < mpoly->totloop; ++loop) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->v == vert) {
				vert_faces[num++] = poly;
				break;
			}
		}
	}
}

OpenSubdiv::Far::TopologyRefiner *openSubdiv_topologyRefinerFromDM(DerivedMesh *dm)
{
	using OpenSubdiv::Far::TopologyRefinerFactory;
	OsdBlenderConverter conv(dm);
	TopologyRefinerFactory<OsdBlenderConverter>::Options
	        topology_options(conv.get_type(), conv.get_options());
#ifdef OPENSUBDIV_VALIDATE_TOPOLOGY
	topology_options.validateFullTopology = true;
#endif
	return TopologyRefinerFactory<OsdBlenderConverter>::Create(conv, topology_options);
}

/* *********************************************************** */

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {
namespace Far {

template <>
inline bool TopologyRefinerFactory<OpenSubdiv_Converter>::resizeComponentTopology(
        TopologyRefiner& refiner,
        const OpenSubdiv_Converter& conv)
{
	/* Faces and face-verts */
	const int num_faces = conv.get_num_faces(&conv);
	setNumBaseFaces(refiner, num_faces);
	for (int face = 0; face < num_faces; ++face) {
		const int num_verts = conv.get_num_face_verts(&conv, face);
		setNumBaseFaceVertices(refiner, face, num_verts);
	}
	/* Edges and edge-faces. */
	const int num_edges = conv.get_num_edges(&conv);
	setNumBaseEdges(refiner, num_edges);
	for (int edge = 0; edge < num_edges; ++edge) {
		const int num_edge_faces = conv.get_num_edge_faces(&conv, edge);
		setNumBaseEdgeFaces(refiner, edge, num_edge_faces);
	}
	/* Vertices and vert-faces and vert-edges/ */
	const int num_verts = conv.get_num_verts(&conv);
	setNumBaseVertices(refiner, num_verts);
	for (int vert = 0; vert < num_verts; ++vert) {
		const int num_vert_edges = conv.get_num_vert_edges(&conv, vert),
		          num_vert_faces = conv.get_num_vert_faces(&conv, vert);
		setNumBaseVertexEdges(refiner, vert, num_vert_edges);
		setNumBaseVertexFaces(refiner, vert, num_vert_faces);
	}
	return true;
}

template <>
inline bool TopologyRefinerFactory<OpenSubdiv_Converter>::assignComponentTopology(
	  TopologyRefiner& refiner,
        const OpenSubdiv_Converter& conv)
{
	using Far::IndexArray;
	/* Face relations. */
	const int num_faces = conv.get_num_faces(&conv);
	for (int face = 0; face < num_faces; ++face) {
		IndexArray dst_face_verts = getBaseFaceVertices(refiner, face);
		conv.get_face_verts(&conv, face, &dst_face_verts[0]);
		IndexArray dst_face_edges = getBaseFaceEdges(refiner, face);
		conv.get_face_edges(&conv, face, &dst_face_edges[0]);
	}
	/* Edge relations. */
	const int num_edges = conv.get_num_edges(&conv);
	for (int edge = 0; edge < num_edges; ++edge) {
		/* Edge-vertices */
		IndexArray dst_edge_verts = getBaseEdgeVertices(refiner, edge);
		conv.get_edge_verts(&conv, edge, &dst_edge_verts[0]);
		/* Edge-faces */
		IndexArray dst_edge_faces = getBaseEdgeFaces(refiner, edge);
		conv.get_edge_faces(&conv, edge, &dst_edge_faces[0]);
	}
	/* Vertex relations */
	const int num_verts = conv.get_num_verts(&conv);
	for (int vert = 0; vert < num_verts; ++vert) {
		/* Vert-Faces */
		IndexArray dst_vert_faces = getBaseVertexFaces(refiner, vert);
		int num_vert_edges = conv.get_num_vert_edges(&conv, vert);
		int *vert_edges = new int[num_vert_edges];
		conv.get_vert_edges(&conv, vert, vert_edges);
		/* Vert-Edges */
		IndexArray dst_vert_edges = getBaseVertexEdges(refiner, vert);
		int num_vert_faces = conv.get_num_vert_faces(&conv, vert);
		int *vert_faces = new int[num_vert_faces];
		conv.get_vert_faces(&conv, vert, vert_faces);
		/* Order vertex edges and faces in a CCW order. */
		Index face_start = INDEX_INVALID;
		Index edge_start = INDEX_INVALID;
		int face_vert_start = 0;
		if (num_vert_edges == num_vert_faces) {
			face_start  = vert_faces[0];
			face_vert_start = findInArray(getBaseFaceVertices(refiner, face_start), vert);
			edge_start = getBaseFaceEdges(refiner, face_start)[face_vert_start];
		} else {
			for (int i = 0; i < num_vert_edges; ++i) {
				IndexArray edge_faces = getBaseEdgeFaces(refiner, vert_edges[i]);
				if (edge_faces.size() == 1) {
					edge_start = vert_edges[i];
					face_start = edge_faces[0];
					face_vert_start = findInArray(getBaseFaceVertices(refiner, face_start), vert);
					if (edge_start == (getBaseFaceEdges(refiner, face_start)[face_vert_start])) {
						break;
					}
				}
			}
		}
		int edge_count_ordered = 1;
		int face_count_ordered = 1;
		dst_vert_faces[0] = face_start;
		dst_vert_edges[0] = edge_start;
		while (edge_count_ordered < num_vert_edges) {
			IndexArray fVerts = getBaseFaceVertices(refiner, face_start);
			IndexArray fEdges = getBaseFaceEdges(refiner, face_start);
			int feStart = face_vert_start;
			int feNext = feStart ? (feStart - 1) : (fVerts.size() - 1);
			Index eNext = fEdges[feNext];
			dst_vert_edges[edge_count_ordered++] = eNext;
			if (face_count_ordered < num_vert_faces) {
				IndexArray edge_faces = getBaseEdgeFaces(refiner, eNext);
				face_start = edge_faces[edge_faces[0] == face_start];
				face_vert_start = findInArray(getBaseFaceEdges(refiner, face_start), eNext);
				dst_vert_faces[face_count_ordered++] = face_start;
			}
			edge_start = eNext;
		}

		delete [] vert_edges;
		delete [] vert_faces;
	}
	populateBaseLocalIndices(refiner);
	return true;
};

template <>
inline bool TopologyRefinerFactory<OpenSubdiv_Converter>::assignComponentTags(
        TopologyRefiner& refiner,
        const OpenSubdiv_Converter& conv)
{
	/* TODO(sergey): Use real sharpness. */
	int num_edges = conv.get_num_edges(&conv);
	for (int edge = 0; edge < num_edges; ++edge) {
		setBaseEdgeSharpness(refiner, edge, 0.0f);
	}
	return true;
}

template <>
inline void TopologyRefinerFactory<OpenSubdiv_Converter>::reportInvalidTopology(
        TopologyError /*errCode*/,
        const char *msg,
        const OpenSubdiv_Converter& /*mesh*/)
{
	printf("OpenSubdiv Error: %s\n", msg);
}

}  /* namespace Far */
}  /* namespace OPENSUBDIV_VERSION */
}  /* namespace OpenSubdiv */


struct OpenSubdiv_TopologyRefinerDescr *openSubdiv_createTopologyRefinerDescr(
        OpenSubdiv_Converter *converter)
{
	using OpenSubdiv::Far::TopologyRefinerFactory;
	OpenSubdiv::Sdc::SchemeType scheme_type = OpenSubdiv::Sdc::SCHEME_CATMARK;
	OpenSubdiv::Sdc::Options options;
	options.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);
	options.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_ALL);

	TopologyRefinerFactory<OpenSubdiv_Converter>::Options
	        topology_options(scheme_type, options);
#ifdef OPENSUBDIV_VALIDATE_TOPOLOGY
	topology_options.validateFullTopology = true;
#endif
	return (struct OpenSubdiv_TopologyRefinerDescr*)
	        TopologyRefinerFactory<OpenSubdiv_Converter>::Create(
	                *converter,
	                topology_options);
}
