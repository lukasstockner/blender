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

	/* TODO(sergey): Currently unused, but might be handy in the future. */
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
namespace Far {

/* Hackish approach to ensure proper component orientation.
 *
 * TODO(sergey): Get rid of this, it's far too slow.
 */
namespace {

template <typename T>
inline int findInArray(T array, int value)
{
	return (int)(std::find(array.begin(), array.end(), value) - array.begin());
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
		int num_vert_edges = conv.get_num_vert_edges(vert);
		int *vert_edges = new int[num_vert_edges];
		conv.get_vert_edges(vert, vert_edges);
		/* Vert-Edges */
		IndexArray dst_vert_edges = getBaseVertexEdges(refiner, vert);
		int num_vert_faces = conv.get_num_vert_faces(vert);
		int *vert_faces = new int[num_vert_faces];
		conv.get_vert_faces(vert, vert_faces);
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
