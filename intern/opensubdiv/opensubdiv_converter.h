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

#ifndef __OPENSUBDIV_CONVERTER_H__
#define __OPENSUBDIV_CONVERTER_H__

#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/far/primvarRefiner.h>

#include <cstdio>

extern "C" {
struct DerivedMesh;
}

struct OsdBlenderConverter {
public:
	OsdBlenderConverter(struct DerivedMesh *dm);

	OpenSubdiv::Sdc::SchemeType get_type() const;
	OpenSubdiv::Sdc::Options get_options() const;

	int get_num_faces() const;
	int get_num_edges() const;
	int get_num_verts() const;

	void get_coarse_verts(float *coords) const;

	/* Face relationships. */
	int get_num_face_verts(int face) const;
	void get_face_verts(int face, int *face_verts) const;
	void get_face_edges(int face, int *face_edges) const;

	/* Edge relationships. */
	void get_edge_verts(int edge, int *edge_verts) const;
	int get_num_edge_faces(int edge) const;
	void get_edge_faces(int edge, int *edge_faces) const;

	/* Vertex relationships. */
	int get_num_vert_edges(int vert) const;
	void get_vert_edges(int vert, int *vert_edges) const;
	int get_num_vert_faces(int vert) const;
	void get_vert_faces(int vert, int *vert_faces) const;

private:
	struct DerivedMesh *dm_;
};

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Vtr {

class ConstIndexArrayOwn : public ConstIndexArray
{
public:
	ConstIndexArrayOwn(Index *ptr, size_type size)
	    : ConstIndexArray(ptr, size) { }

	~ConstIndexArrayOwn() {
		delete [] this->_begin;
	}
};

}  /* namespace Vtr */

namespace Far {

/* Hackish approach to ensure proper component orientation.
 *
 * TODO(sergey): Get rid of this, it's far too slow.
 */
namespace {

using OpenSubdiv::Vtr::ConstIndexArrayOwn;

inline int findInArray(ConstIndexArrayOwn array, Index value)
{
	return (int)(std::find(array.begin(), array.end(), value) - array.begin());
}

inline ConstIndexArrayOwn getVertexEdges(const OsdBlenderConverter& conv, Index vIndex)
{
	int num_vert_edges = conv.get_num_vert_edges(vIndex);
	int *vert_edges = new int[num_vert_edges];
	conv.get_vert_edges(vIndex, vert_edges);
	return ConstIndexArrayOwn(vert_edges, num_vert_edges);
}

inline ConstIndexArrayOwn getVertexFaces(const OsdBlenderConverter& conv, Index vIndex)
{
	int num_vert_faces = conv.get_num_vert_faces(vIndex);
	int *vert_faces = new int[num_vert_faces];
	conv.get_vert_faces(vIndex, vert_faces);
	return ConstIndexArrayOwn(vert_faces, num_vert_faces);
}

inline ConstIndexArrayOwn getFaceVertices(const OsdBlenderConverter& conv, Index fIndex)
{
	int num_face_verts = conv.get_num_face_verts(fIndex);
	int *face_verts = new int[num_face_verts];
	conv.get_face_verts(fIndex, face_verts);
	return ConstIndexArrayOwn(face_verts, num_face_verts);
}

inline ConstIndexArrayOwn getFaceEdges(const OsdBlenderConverter& conv, Index fIndex)
{
	int num_face_edges = conv.get_num_face_verts(fIndex);
	int *face_edges = new int[num_face_edges];
	conv.get_face_edges(fIndex, face_edges);
	return ConstIndexArrayOwn(face_edges, num_face_edges);
}

inline ConstIndexArrayOwn getEdgeFaces(const OsdBlenderConverter& conv, Index eIndex)
{
	int num_edge_faces = conv.get_num_edge_faces(eIndex);
	int *edge_faces = new int[num_edge_faces];
	conv.get_edge_faces(eIndex, edge_faces);
	return ConstIndexArrayOwn(edge_faces, num_edge_faces);
}

void orderVertexFacesAndEdges(const OsdBlenderConverter& conv,
                              Index vIndex,
                              Index *vFacesOrdered,
                              Index *vEdgesOrdered)
{
	ConstIndexArrayOwn vEdges = getVertexEdges(conv, vIndex);
	ConstIndexArrayOwn vFaces = getVertexFaces(conv, vIndex);
	int fCount = vFaces.size();
	int eCount = vEdges.size();
	Index fStart = INDEX_INVALID;
	Index eStart = INDEX_INVALID;
	int fvStart = 0;
	if (eCount == fCount) {
		fStart  = vFaces[0];
		fvStart = findInArray(getFaceVertices(conv, fStart), vIndex);
		eStart = getFaceEdges(conv, fStart)[fvStart];
	} else {
		for (int i = 0; i < eCount; ++i) {
			ConstIndexArrayOwn eFaces = getEdgeFaces(conv, vEdges[i]);
			if (eFaces.size() == 1) {
				eStart = vEdges[i];
				fStart = eFaces[0];
				fvStart = findInArray(getFaceVertices(conv, fStart), vIndex);
				if (eStart == (getFaceEdges(conv, fStart)[fvStart])) {
					break;
				}
			}
		}
	}
	int eCountOrdered = 1;
	int fCountOrdered = 1;
	vFacesOrdered[0] = fStart;
	vEdgesOrdered[0] = eStart;
	while (eCountOrdered < eCount) {
		ConstIndexArrayOwn fVerts = getFaceVertices(conv, fStart);
		ConstIndexArrayOwn fEdges = getFaceEdges(conv, fStart);
		int feStart = fvStart;
		int feNext = feStart ? (feStart - 1) : (fVerts.size() - 1);
		Index eNext = fEdges[feNext];
		vEdgesOrdered[eCountOrdered++] = eNext;
		if (fCountOrdered < fCount) {
			ConstIndexArrayOwn eFaces = getEdgeFaces(conv, eNext);
			fStart = eFaces[eFaces[0] == fStart];
			fvStart = findInArray(getFaceEdges(conv, fStart), eNext);
			vFacesOrdered[fCountOrdered++] = fStart;
		}
		eStart = eNext;
	}
	assert(eCountOrdered == eCount);
	assert(fCountOrdered == fCount);
}

}  /* namespace */

template <>
inline bool TopologyRefinerFactory<OsdBlenderConverter>::resizeComponentTopology(
        TopologyRefiner& refiner,
        const OsdBlenderConverter& conv)
{
	/* Faces and face-verts */
	const int num_faces = conv.get_num_faces();
	setNumBaseFaces(refiner, num_faces);
	for (int face = 0; face < num_faces; ++face) {
		const int num_verts = conv.get_num_face_verts(face);
		setNumBaseFaceVertices(refiner, face, num_verts);
	}
	/* Edges and edge-faces. */
	const int num_edges = conv.get_num_edges();
	setNumBaseEdges(refiner, num_edges);
	for (int edge = 0; edge < num_edges; ++edge) {
		const int num_edge_faces = conv.get_num_edge_faces(edge);
		setNumBaseEdgeFaces(refiner, edge, num_edge_faces);
	}
	/* Vertices and vert-faces and vert-edges/ */
	const int num_verts = conv.get_num_verts();
	setNumBaseVertices(refiner, num_verts);
	for (int vert = 0; vert < num_verts; ++vert) {
		const int num_vert_edges = conv.get_num_vert_edges(vert),
		          num_vert_faces = conv.get_num_vert_faces(vert);
		setNumBaseVertexEdges(refiner, vert, num_vert_edges);
		setNumBaseVertexFaces(refiner, vert, num_vert_faces);
	}
	return true;
}

template <>
inline bool TopologyRefinerFactory<OsdBlenderConverter>::assignComponentTopology(
        TopologyRefiner& refiner,
        const OsdBlenderConverter& conv)
{

	using Far::IndexArray;
	/* Face relations. */
	const int num_faces = conv.get_num_faces();
	for (int face = 0; face < num_faces; ++face) {
		IndexArray dst_face_verts = getBaseFaceVertices(refiner, face);
		conv.get_face_verts(face, &dst_face_verts[0]);
		IndexArray dst_face_edges = getBaseFaceEdges(refiner, face);
		conv.get_face_edges(face, &dst_face_edges[0]);
	}
	/* Edge relations. */
	const int num_edges = conv.get_num_edges();
	for (int edge = 0; edge < num_edges; ++edge) {
		/* Edge-vertices */
		IndexArray dst_edge_verts = getBaseEdgeVertices(refiner, edge);
		conv.get_edge_verts(edge, &dst_edge_verts[0]);
		/* Edge-faces */
		IndexArray dst_edge_faces = getBaseEdgeFaces(refiner, edge);
		conv.get_edge_faces(edge, &dst_edge_faces[0]);
	}
	/* Vertex relations */
	const int num_verts = conv.get_num_verts();
	for (int vert = 0; vert < num_verts; ++vert) {
		/* Vert-Faces */
		IndexArray dst_vert_faces = getBaseVertexFaces(refiner, vert);
		// conv.get_vert_faces(vert, &dst_vert_faces[0]);
		/* Vert-Edges */
		IndexArray dst_vert_edges = getBaseVertexEdges(refiner, vert);
		// conv.get_vert_edges(vert, &dst_vert_edges[0]);
		orderVertexFacesAndEdges(conv, vert, &dst_vert_faces[0], &dst_vert_edges[0]);
	}
	populateBaseLocalIndices(refiner);
	return true;
};

template <>
inline bool TopologyRefinerFactory<OsdBlenderConverter>::assignComponentTags(
        TopologyRefiner& refiner,
        const OsdBlenderConverter& conv)
{
	/* TODO(sergey): Use real sharpness. */
	int num_edges = conv.get_num_edges();
	for (int edge = 0; edge < num_edges; ++edge) {
		setBaseEdgeSharpness(refiner, edge, 0.0f);
	}
	return true;
}

template <>
inline void TopologyRefinerFactory<OsdBlenderConverter>::reportInvalidTopology(
        TopologyError /*errCode*/,
        const char *msg,
        const OsdBlenderConverter& /*mesh*/)
{
	printf("OpenSubdiv Error: %s\n", msg);
}

}  /* namespace Far */
}  /* namespace OPENSUBDIV_VERSION */
}  /* namespace OpenSubdiv */

OpenSubdiv::Far::TopologyRefiner *openSubdiv_topologyRefinerFromDM(DerivedMesh *dm);

#endif  /* __OPENSUBDIV_CONVERTER_H__ */
