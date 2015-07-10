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

#include "opensubdiv_converter.h"
#include "opensubdiv_intern.h"
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

struct OpenSubdiv_GLMesh *openSubdiv_createOsdGLMeshFromEvaluator(
        //OpenSubdiv_EvaluatorDescr * /*evaluator_descr*/,
        DerivedMesh *dm,
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
	OpenSubdiv::Far::TopologyRefiner *refiner = openSubdiv_topologyRefinerFromDM(dm);

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
