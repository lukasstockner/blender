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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *                 Brecht van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "opensubdiv_capi.h"

#ifdef _MSC_VER
#  include "iso646.h"
#endif

#include <GL/glew.h>

#include <opensubdiv/osd/glMesh.h>
#include <opensubdiv/far/topologyRefinerFactory.h>

/* CPU Backend */
#include <opensubdiv/osd/cpuGLVertexBuffer.h>
#include <opensubdiv/osd/cpuEvaluator.h>

#ifdef OPENSUBDIV_HAS_OPENMP
#  include <opensubdiv/osd/ompEvaluator.h>
#endif  /* OPENSUBDIV_HAS_OPENMP */

#ifdef OPENSUBDIV_HAS_OPENCL
#  include <opensubdiv/osd/clGLVertexBuffer.h>
#  include <opensubdiv/osd/clEvaluator.h>
#  include "opensubdiv_device_context_opencl.h"
#endif  /* OPENSUBDIV_HAS_OPENCL */

#ifdef OPENSUBDIV_HAS_CUDA
#  include <opensubdiv/osd/cudaGLVertexBuffer.h>
#  include <opensubdiv/osd/cudaEvaluator.h>
#  include "opensubdiv_device_context_cuda.h"
#endif  /* OPENSUBDIV_HAS_CUDA */

#ifdef OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK
#  include <opensubdiv/osd/glXFBEvaluator.h>
#  include <opensubdiv/osd/glVertexBuffer.h>
#endif  /* OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK */

#ifdef OPENSUBDIV_HAS_GLSL_COMPUTE
#  include <opensubdiv/osd/glComputeEvaluator.h>
#  include <opensubdiv/osd/glVertexBuffer.h>
#endif  /* OPENSUBDIV_HAS_GLSL_COMPUTE */

#include <opensubdiv/osd/glPatchTable.h>
#include <opensubdiv/far/stencilTable.h>

#include "opensubdiv_partitioned.h"

#include "MEM_guardedalloc.h"

/* **************** Types declaration **************** */

using OpenSubdiv::Osd::GLMeshInterface;
using OpenSubdiv::Osd::Mesh;
using OpenSubdiv::Osd::MeshBitset;
using OpenSubdiv::Far::StencilTable;
using OpenSubdiv::Osd::GLPatchTable;

using OpenSubdiv::Osd::PartitionedMesh;

/* CPU backend */
using OpenSubdiv::Osd::CpuGLVertexBuffer;
using OpenSubdiv::Osd::CpuEvaluator;
typedef PartitionedMesh<CpuGLVertexBuffer,
                        StencilTable,
                        CpuEvaluator,
                        GLPatchTable> OsdCpuMesh;

#ifdef OPENSUBDIV_HAS_OPENMP
using OpenSubdiv::Osd::OmpEvaluator;
typedef PartitionedMesh<CpuGLVertexBuffer,
                        StencilTable,
                        OmpEvaluator,
                        GLPatchTable> OsdOmpMesh;
#endif  /* OPENSUBDIV_HAS_OPENMP */

#ifdef OPENSUBDIV_HAS_OPENCL
using OpenSubdiv::Osd::CLEvaluator;
using OpenSubdiv::Osd::CLGLVertexBuffer;
using OpenSubdiv::Osd::CLStencilTable;
/* TODO(sergey): Use CLDeviceCOntext similar to OSD examples? */
typedef PartitionedMesh<CLGLVertexBuffer,
                        CLStencilTable,
                        CLEvaluator,
                        GLPatchTable,
                        CLDeviceContext> OsdCLMesh;
static CLDeviceContext g_clDeviceContext;
#endif  /* OPENSUBDIV_HAS_OPENCL */

#ifdef OPENSUBDIV_HAS_CUDA
using OpenSubdiv::Osd::CudaEvaluator;
using OpenSubdiv::Osd::CudaGLVertexBuffer;
using OpenSubdiv::Osd::CudaStencilTable;
typedef PartitionedMesh<CudaGLVertexBuffer,
                        CudaStencilTable,
                        CudaEvaluator,
                        GLPatchTable> OsdCudaMesh;
static CudaDeviceContext g_cudaDeviceContext;
#endif  /* OPENSUBDIV_HAS_CUDA */

#ifdef OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK
using OpenSubdiv::Osd::GLXFBEvaluator;
using OpenSubdiv::Osd::GLStencilTableTBO;
using OpenSubdiv::Osd::GLVertexBuffer;
typedef PartitionedMesh<GLVertexBuffer,
                        GLStencilTableTBO,
                        GLXFBEvaluator,
                        GLPatchTable> OsdGLSLTransformFeedbackMesh;
#endif  /* OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK */

#ifdef OPENSUBDIV_HAS_GLSL_COMPUTE
using OpenSubdiv::Osd::GLComputeEvaluator;
using OpenSubdiv::Osd::GLStencilTableSSBO;
using OpenSubdiv::Osd::GLVertexBuffer;
typedef PartitionedMesh<GLVertexBuffer,
                        GLStencilTableSSBO,
                        GLComputeEvaluator,
                        GLPatchTable> OsdGLSLComputeMesh;
#endif

#if 0
static OpenSubdiv::OsdUtilMesh<OsdVertex>::Scheme get_osd_scheme(int scheme)
{
	switch (scheme) {
		case OPENSUBDIV_SCHEME_CATMARK:
			return OpenSubdiv::OsdUtilMesh<OsdVertex>::SCHEME_CATMARK;
		case OPENSUBDIV_SCHEME_BILINEAR:
			return OpenSubdiv::OsdUtilMesh<OsdVertex>::SCHEME_BILINEAR;
		case OPENSUBDIV_SCHEME_LOOP:
			return OpenSubdiv::OsdUtilMesh<OsdVertex>::SCHEME_LOOP;
		default:
			assert(!"Wrong subdivision scheme");
	}
	return OpenSubdiv::OsdUtilMesh<OsdVertex>::SCHEME_BILINEAR;
}
#endif

/* TODO(sergey): Currently we use single coarse face per partition,
 * which allows to have per-face material assignment but which also
 * increases number of glDrawElements() calls.
 *
 * Ideally here we need to partition like this, but do some conjunction
 * at draw time, so adjacent faces with the same material are displayed
 * in a single chunk.
 */
#if 0
static void get_partition_per_face(OsdHbrMesh &hmesh,
                                   std::vector<int> *idsOnPtexFaces)
{
	int numFaces = hmesh.GetNumCoarseFaces();

	/* First, assign partition ID to each coarse face. */
	std::vector<int> idsOnCoarseFaces;
	for (int i = 0; i < numFaces; ++i) {
		int partitionID = i;
		idsOnCoarseFaces.push_back(partitionID);
	}

	/* Create ptex index to coarse face index mapping. */
	OsdHbrFace *lastFace = hmesh.GetFace(numFaces - 1);
	int numPtexFaces = lastFace->GetPtexIndex();
	numPtexFaces += (hmesh.GetSubdivision()->FaceIsExtraordinary(&hmesh,
	                                                             lastFace) ?
	                 lastFace->GetNumVertices() : 1);

	/* TODO(sergey): Duplicated logic to simpleHbr. */
	std::vector<int> ptexIndexToFaceMapping(numPtexFaces);
	int ptexIndex = 0;
	for (int i = 0; i < numFaces; ++i) {
		OsdHbrFace *f = hmesh.GetFace(i);
		ptexIndexToFaceMapping[ptexIndex++] = i;
		int numVerts = f->GetNumVertices();
		if (numVerts != 4 ) {
			for (int j = 0; j < numVerts-1; ++j) {
				ptexIndexToFaceMapping[ptexIndex++] = i;
			}
		}
	}
	assert((int)ptexIndexToFaceMapping.size() == numPtexFaces);

	/* Convert ID array from coarse face index space to ptex index space. */
	for (int i = 0; i < numPtexFaces; ++i) {
		idsOnPtexFaces->push_back(idsOnCoarseFaces[ptexIndexToFaceMapping[i]]);
	}
}
#endif

/* TODO(sergey): It might be good to have in evaluator_capi. */
void openSubdiv_evlauatorClearTags(OpenSubdiv_EvaluatorDescr *evaluator_descr)
{
#if 0
	OsdUtilSubdivTopology *topology =
	    (OsdUtilSubdivTopology *)openSubdiv_getEvaluatorTopologyDescr(
	        evaluator_descr);

	topology->tagData.tags.clear();
	topology->tagData.intArgs.clear();
	topology->tagData.floatArgs.clear();
	topology->tagData.stringArgs.clear();
#else
	(void)evaluator_descr;
#endif
}

void openSubdiv_evaluatorSetEdgeSharpness(
        OpenSubdiv_EvaluatorDescr *evaluator_descr,
        int v0, int v1,
        float sharpness)
{
#if 0
	OsdUtilSubdivTopology *topology =
	    (OsdUtilSubdivTopology *)openSubdiv_getEvaluatorTopologyDescr(
	        evaluator_descr);
	int indices[] = {v0, v1};

	topology->tagData.AddCrease(indices, 2, &sharpness, 1);
#else
	(void)evaluator_descr;
	(void)v0;
	(void)v1;
	(void)sharpness;
#endif
}

const float *openSubdiv_evaluatorGetFloatTagArgs(
        OpenSubdiv_EvaluatorDescr *evaluator_descr)
{
#if 0
	OsdUtilSubdivTopology *topology =
	    (OsdUtilSubdivTopology *)openSubdiv_getEvaluatorTopologyDescr(
	        evaluator_descr);
	return &topology->tagData.floatArgs[0];
#else
	(void)evaluator_descr;
	return NULL;
#endif
}

static int g_nverts = 5,
           g_nedges = 8,
           g_nfaces = 5;

// vertex positions
static float g_verts[5][3] = {{ 0.0f,  0.0f,  2.0f},
                              { 0.0f, -2.0f,  0.0f},
                              { 2.0f,  0.0f,  0.0f},
                              { 0.0f,  2.0f,  0.0f},
                              {-2.0f,  0.0f,  0.0f}};

// number of vertices in each face
static int g_facenverts[5] = { 3, 3, 3, 3, 4 };

// index of face vertices
static int g_faceverts[16] = { 0, 1, 2,
                               0, 2, 3,
                               0, 3, 4,
                               0, 4, 1,
                               4, 3, 2, 1 };

// index of edge vertices (2 per edge)
static int g_edgeverts[16] = { 0, 1,
                               1, 2,
                               2, 0,
                               2, 3,
                               3, 0,
                               3, 4,
                               4, 0,
                               4, 1 };


// index of face edges
static int g_faceedges[16] = { 0, 1, 2,
                               2, 3, 4,
                               4, 5, 6,
                               6, 7, 0,
                               5, 3, 1, 7 };

// number of faces adjacent to each edge
static int g_edgenfaces[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };

// index of faces incident to a given edge
static int g_edgefaces[16] = { 0, 3,
                               0, 4,
                               0, 1,
                               1, 4,
                               1, 2,
                               2, 4,
                               2, 3,
                               3, 4 };

// number of faces incident to each vertex
static int g_vertexnfaces[5] = { 4, 3, 3, 3, 3 };

// index of faces incident to each vertex
static int g_vertexfaces[25] = { 0, 1, 2, 3,
                                 0, 3, 4,
                                 0, 4, 1,
                                 1, 4, 2,
                                 2, 4, 3 };


// number of edges incident to each vertex
static int g_vertexnedges[5] = { 4, 3, 3, 3, 3 };

// index of edges incident to each vertex
static int g_vertexedges[25] = { 0, 2, 4, 6,
                                 1, 0, 7,
                                 2, 1, 3,
                                 4, 3, 5,
                                 6, 5, 7 };

// Edge crease sharpness
static float g_edgeCreases[8] = { 0.0f,
                                  2.5f,
                                  0.0f,
                                  2.5f,
                                  0.0f,
                                  2.5f,
                                  0.0f,
                                  2.5f };

struct Converter {

public:

	OpenSubdiv::Sdc::SchemeType GetType() const {
        return OpenSubdiv::Sdc::SCHEME_CATMARK;
    }

    OpenSubdiv::Sdc::Options GetOptions() const {
        OpenSubdiv::Sdc::Options options;
        options.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);
        return options;
    }

    int GetNumFaces() const { return g_nfaces; }

    int GetNumEdges() const { return g_nedges; }

    int GetNumVertices() const { return g_nverts; }

    //
    // Face relationships
    //
    int GetNumFaceVerts(int face) const { return g_facenverts[face]; }

    int const * GetFaceVerts(int face) const { return g_faceverts+getCompOffset(g_facenverts, face); }

    int const * GetFaceEdges(int edge) const { return g_faceedges+getCompOffset(g_facenverts, edge); }


    //
    // Edge relationships
    //
    int const * GetEdgeVertices(int edge) const { return g_edgeverts+edge*2; }

    int GetNumEdgeFaces(int edge) const { return g_edgenfaces[edge]; }

    int const * GetEdgeFaces(int edge) const { return g_edgefaces+getCompOffset(g_edgenfaces, edge); }

    //
    // Vertex relationships
    //
    int GetNumVertexEdges(int vert) const { return g_vertexnedges[vert]; }

    int const * GetVertexEdges(int vert) const { return g_vertexedges+getCompOffset(g_vertexnedges, vert); }

    int GetNumVertexFaces(int vert) const { return g_vertexnfaces[vert]; }

    int const * GetVertexFaces(int vert) const { return g_vertexfaces+getCompOffset(g_vertexnfaces, vert); }

private:

    int getCompOffset(int const * comps, int comp) const {
        int ofs=0;
        for (int i=0; i<comp; ++i) {
            ofs += comps[i];
        }
        return ofs;
    }

};

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Far {

template <>
bool
TopologyRefinerFactory<Converter>::resizeComponentTopology(
    TopologyRefiner & refiner, Converter const & conv) {

    // Faces and face-verts
    int nfaces = conv.GetNumFaces();
    setNumBaseFaces(refiner, nfaces);
    for (int face=0; face<nfaces; ++face) {

        int nv = conv.GetNumFaceVerts(face);
        setNumBaseFaceVertices(refiner, face, nv);
    }

   // Edges and edge-faces
    int nedges = conv.GetNumEdges();
    setNumBaseEdges(refiner, nedges);
    for (int edge=0; edge<nedges; ++edge) {

        int nf = conv.GetNumEdgeFaces(edge);
        setNumBaseEdgeFaces(refiner, edge, nf);
    }

    // Vertices and vert-faces and vert-edges
    int nverts = conv.GetNumVertices();
    setNumBaseVertices(refiner, nverts);
    for (int vert=0; vert<nverts; ++vert) {

        int ne = conv.GetNumVertexEdges(vert),
            nf = conv.GetNumVertexFaces(vert);
        setNumBaseVertexEdges(refiner, vert, ne);
        setNumBaseVertexFaces(refiner, vert, nf);
    }
    return true;
}

template <>
bool
TopologyRefinerFactory<Converter>::assignComponentTopology(
    TopologyRefiner & refiner, Converter const & conv) {

    typedef Far::IndexArray      IndexArray;

    { // Face relations:
        int nfaces = conv.GetNumFaces();
        for (int face=0; face<nfaces; ++face) {

            IndexArray dstFaceVerts = getBaseFaceVertices(refiner, face);
            IndexArray dstFaceEdges = getBaseFaceEdges(refiner, face);

            int const * faceverts = conv.GetFaceVerts(face);
            int const * faceedges = conv.GetFaceEdges(face);

            for (int vert=0; vert<conv.GetNumFaceVerts(face); ++vert) {
                dstFaceVerts[vert] = faceverts[vert];
                dstFaceEdges[vert] = faceedges[vert];
            }
        }
    }

    { // Edge relations
      //
      // Note: if your representation is unable to provide edge relationships
      //       (ex: half-edges), you can comment out this section and Far will
      //       automatically generate the missing information.
      //
        int nedges = conv.GetNumEdges();
        for (int edge=0; edge<nedges; ++edge) {

            //  Edge-vertices:
            IndexArray dstEdgeVerts = getBaseEdgeVertices(refiner, edge);
            dstEdgeVerts[0] = conv.GetEdgeVertices(edge)[0];
            dstEdgeVerts[1] = conv.GetEdgeVertices(edge)[1];

            //  Edge-faces
            IndexArray dstEdgeFaces = getBaseEdgeFaces(refiner, edge);
            for (int face=0; face<conv.GetNumEdgeFaces(face); ++face) {
                dstEdgeFaces[face] = conv.GetEdgeFaces(edge)[face];
            }
        }
    }

    { // Vertex relations
        int nverts = conv.GetNumVertices();
        for (int vert=0; vert<nverts; ++vert) {

            //  Vert-Faces:
            IndexArray vertFaces = getBaseVertexFaces(refiner, vert);
            //LocalIndexArray vertInFaceIndices = getBaseVertexFaceLocalIndices(refiner, vert);
            for (int face=0; face<conv.GetNumVertexFaces(vert); ++face) {
                vertFaces[face] = conv.GetVertexFaces(vert)[face];
            }

            //  Vert-Edges:
            IndexArray vertEdges = getBaseVertexEdges(refiner, vert);
            //LocalIndexArray vertInEdgeIndices = getBaseVertexEdgeLocalIndices(refiner, vert);
            for (int edge=0; edge<conv.GetNumVertexEdges(vert); ++edge) {
                vertEdges[edge] = conv.GetVertexEdges(vert)[edge];
            }
        }
    }

    populateBaseLocalIndices(refiner);

    return true;
};

template <>
bool
TopologyRefinerFactory<Converter>::assignComponentTags(
    TopologyRefiner & refiner, Converter const & conv) {

    // arbitrarily sharpen the 4 bottom edges of the pyramid to 2.5f
    for (int edge=0; edge<conv.GetNumEdges(); ++edge) {
        setBaseEdgeSharpness(refiner, edge, g_edgeCreases[edge]);
    }
    return true;
}

} // namespace Far

} // namespace OPENSUBDIV_VERSION
} // namespace OpenSubdiv

struct OpenSubdiv_GLMesh *openSubdiv_createOsdGLMeshFromEvaluator(
        OpenSubdiv_EvaluatorDescr * /*evaluator_descr*/,
        int evaluator_type,
        int level,
        int /*scheme*/,
        int /*subdivide_uvs*/)
{
	MeshBitset bits;
	/* TODO(sergey): Adaptive subdivisions are not currently
	 * possible because of the lack of tessellation shader.
	 */
	bits.set(OpenSubdiv::Osd::MeshAdaptive, 0);
    bits.set(OpenSubdiv::Osd::MeshUseSingleCreasePatch, 0);
    bits.set(OpenSubdiv::Osd::MeshInterleaveVarying, 0);
    bits.set(OpenSubdiv::Osd::MeshFVarData, 1);
    bits.set(OpenSubdiv::Osd::MeshEndCapBSplineBasis, 1);
    // bits.set(Osd::MeshEndCapGregoryBasis, 1);
    // bits.set(Osd::MeshEndCapLegacyGregory, 1);

	const int num_vertex_elements = 6;
	const int num_varying_elements = 0;

	GLMeshInterface *mesh = NULL;

    Converter conv;
    OpenSubdiv::Far::TopologyRefiner * refiner =
        OpenSubdiv::Far::TopologyRefinerFactory<Converter>::Create(conv,
                OpenSubdiv::Far::TopologyRefinerFactory<Converter>::Options(conv.GetType(), conv.GetOptions()));

	mesh = new OsdCpuMesh(refiner,
	                      num_vertex_elements,
	                      num_varying_elements,
	                      level,
	                      bits);

	if (mesh == NULL) {
		return NULL;
	}

	OpenSubdiv_GLMesh *gl_mesh =
		(OpenSubdiv_GLMesh *) OBJECT_GUARDED_NEW(OpenSubdiv_GLMesh);
	gl_mesh->evaluator_type = evaluator_type;
	gl_mesh->descriptor = (OpenSubdiv_GLMeshDescr *) mesh;
	gl_mesh->level = level;

	return gl_mesh;
}

void openSubdiv_deleteOsdGLMesh(struct OpenSubdiv_GLMesh *gl_mesh)
{
	switch (gl_mesh->evaluator_type) {
#define CHECK_EVALUATOR_TYPE(type, class) \
		case OPENSUBDIV_EVALUATOR_ ## type: \
			delete (class *) gl_mesh->descriptor; \
			break;

		CHECK_EVALUATOR_TYPE(CPU, OsdCpuMesh)

#ifdef OPENSUBDIV_HAS_OPENMP
		CHECK_EVALUATOR_TYPE(OPENMP, OsdOmpMesh)
#endif

#ifdef OPENSUBDIV_HAS_OPENCL
		CHECK_EVALUATOR_TYPE(OPENCL, OsdCLMesh)
#endif

#ifdef OPENSUBDIV_HAS_CUDA
		CHECK_EVALUATOR_TYPE(CUDA, OsdCudaMesh)
#endif

#ifdef OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK
		CHECK_EVALUATOR_TYPE(GLSL_TRANSFORM_FEEDBACK,
		                     OsdGLSLTransformFeedbackMesh)
#endif

#ifdef OPENSUBDIV_HAS_GLSL_COMPUTE
		CHECK_EVALUATOR_TYPE(GLSL_COMPUTE, OsdGLSLComputeMesh)
#endif

#undef CHECK_EVALUATOR_TYPE
	}

	OBJECT_GUARDED_DELETE(gl_mesh, OpenSubdiv_GLMesh);
}

unsigned int openSubdiv_getOsdGLMeshPatchIndexBuffer(struct OpenSubdiv_GLMesh *gl_mesh)
{
	return ((GLMeshInterface *)gl_mesh->descriptor)->GetPatchTable()->GetPatchIndexBuffer();
}

unsigned int openSubdiv_getOsdGLMeshVertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh)
{
	return ((GLMeshInterface *)gl_mesh->descriptor)->BindVertexBuffer();
}

void openSubdiv_osdGLMeshUpdateVertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh,
                                            const float *vertex_data,
                                            int start_vertex,
                                            int num_verts)
{
	((GLMeshInterface *)gl_mesh->descriptor)->UpdateVertexBuffer(vertex_data,
	                                                             start_vertex,
	                                                             num_verts);
}

void openSubdiv_osdGLMeshRefine(struct OpenSubdiv_GLMesh *gl_mesh)
{
	((GLMeshInterface *)gl_mesh->descriptor)->Refine();
}

void openSubdiv_osdGLMeshSynchronize(struct OpenSubdiv_GLMesh *gl_mesh)
{
	((GLMeshInterface *)gl_mesh->descriptor)->Synchronize();
}

void openSubdiv_osdGLMeshBindVertexBuffer(OpenSubdiv_GLMesh *gl_mesh)
{
	((GLMeshInterface *)gl_mesh->descriptor)->BindVertexBuffer();
}

int openSubdiv_supportGPUDisplay(void)
{
	return GL_EXT_geometry_shader4 &&
	       GL_ARB_gpu_shader5 &&
	       glProgramParameteriEXT;
}
