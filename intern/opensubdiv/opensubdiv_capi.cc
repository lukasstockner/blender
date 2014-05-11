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

#include <opensubdiv/osd/glMesh.h>
#include <opensubdiv/osd/glDrawRegistry.h>
#include <opensubdiv/osdutil/evaluator_capi.h>
#include <opensubdiv/osdutil/topology.h>
#include <opensubdiv/osdutil/mesh.h>

// CPU Backend
#include <opensubdiv/osd/cpuGLVertexBuffer.h>
#include <opensubdiv/osd/cpuComputeContext.h>
#include <opensubdiv/osd/cpuComputeController.h>

// CUDA backend
#include <opensubdiv/osd/cudaGLVertexBuffer.h>
#include <opensubdiv/osd/cudaComputeContext.h>
#include <opensubdiv/osd/cudaComputeController.h>

#include "cudaInit.h"

#include "MEM_guardedalloc.h"

/* **************** Types declaration **************** */

using OpenSubdiv::OsdCpuComputeController;
using OpenSubdiv::OsdGLDrawContext;
using OpenSubdiv::OsdGLMeshInterface;
using OpenSubdiv::OsdMesh;
using OpenSubdiv::OsdMeshBitset;
using OpenSubdiv::OsdUtilSubdivTopology;
using OpenSubdiv::OsdVertex;

// CPU backend
using OpenSubdiv::OsdCpuGLVertexBuffer;
using OpenSubdiv::OsdCpuComputeController;

// CUDA backend
using OpenSubdiv::OsdCudaComputeController;
using OpenSubdiv::OsdCudaGLVertexBuffer;

typedef OpenSubdiv::HbrMesh<OsdVertex> OsdHbrMesh;
typedef OsdMesh<OsdCudaGLVertexBuffer,
                OsdCudaComputeController,
                OsdGLDrawContext> OsdCPUGLMesh;

/* **************** CPU Compute Controller **************** */

struct OpenSubdiv_CPUComputeController *openSubdiv_createCPUComputeController(void)
{
	return (struct OpenSubdiv_CPUComputeController *) OBJECT_GUARDED_NEW(OsdCpuComputeController);
}

void openSubdiv_deleteCPUComputeController(struct OpenSubdiv_CPUComputeController *controller)
{
	OBJECT_GUARDED_DELETE(controller, OsdCpuComputeController);
}

/* **************** GLSL Compute Controller **************** */
/*
struct OpenSubdiv_GLSLComputeController *openSubdiv_createGLSLComputeController(void)
{
	return (struct OpenSubdiv_GLSLComputeController *) OBJECT_GUARDED_NEW(OsdGLSLComputeController);
}

void openSubdiv_deleteGLSLComputeController(struct OpenSubdiv_GLSLComputeController *controller)
{
	OBJECT_GUARDED_DELETE(controller, OsdGLSLComputeController);
}
*/
/* **************** CUDA Compute Controller **************** */
struct OpenSubdiv_CUDAComputeController *openSubdiv_createCUDAComputeController(void)
{
	static bool cudaInitialized = false;
	if (cudaInitialized == false) {
		cudaInitialized = true;
		cudaGLSetGLDevice( cutGetMaxGflopsDeviceId() );
	}

	return (struct OpenSubdiv_CUDAComputeController *) OBJECT_GUARDED_NEW(OsdCudaComputeController);
}

void openSubdiv_deleteCUDAComputeController(struct OpenSubdiv_CUDAComputeController *controller)
{
	OBJECT_GUARDED_DELETE(controller, OsdCudaComputeController);
}

/* **************** OpenSubdiv GL Mesh **************** */

struct OpenSubdiv_GLMesh *openSubdiv_createOsdGLMeshFromEvaluator(
    OpenSubdiv_EvaluatorDescr *evaluator_descr,
    OpenSubdiv_CUDAComputeController *controller,
    int level)
{
	OsdUtilSubdivTopology *topology;
	OpenSubdiv::OsdUtilMesh<OsdVertex> util_mesh;

	topology = (OsdUtilSubdivTopology *)openSubdiv_getEvaluatorTopologyDescr(
		evaluator_descr);

	if (util_mesh.Initialize(*topology) == false) {
		return NULL;
	}

	OsdHbrMesh *hmesh = util_mesh.GetHbrMesh();

	OsdMeshBitset bits;
	bits.set(OpenSubdiv::MeshAdaptive, 0);
	bits.set(OpenSubdiv::MeshFVarData, 1);

	int num_vertex_elements = 3;
	int num_varying_elements = 0;

	/* Trick to avoid passing multi-argument template to a macro. */
	OsdGLMeshInterface *gl_mesh =
		OBJECT_GUARDED_NEW(OsdCPUGLMesh,
		                   (OsdCudaComputeController *) controller,
		                   hmesh,
		                   num_vertex_elements,
		                   num_varying_elements,
		                   level,
		                   bits);

	return (OpenSubdiv_GLMesh*) gl_mesh;
}

void openSubdiv_deleteOsdGLMesh(struct OpenSubdiv_GLMesh *gl_mesh)
{
	OBJECT_GUARDED_DELETE(gl_mesh, OsdCPUGLMesh);
}

unsigned int openSubdiv_getOsdGLMeshPatchIndexBuffer(struct OpenSubdiv_GLMesh *gl_mesh)
{
	return ((OsdGLMeshInterface *)gl_mesh)->GetDrawContext()->GetPatchIndexBuffer();
}

unsigned int openSubdiv_bindOsdGLMeshVertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh)
{
	return ((OsdGLMeshInterface *)gl_mesh)->BindVertexBuffer();
}

void openSubdiv_osdGLMeshUpdateVertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh,
                                            const float *vertex_data,
                                            int start_vertex,
                                            int num_verts)
{
	((OsdGLMeshInterface *)gl_mesh)->UpdateVertexBuffer(vertex_data,
	                                                    start_vertex,
	                                                    num_verts);
}

void openSubdiv_osdGLMeshRefine(struct OpenSubdiv_GLMesh *gl_mesh)
{
	((OsdGLMeshInterface *)gl_mesh)->Refine();
}

void openSubdiv_osdGLMeshSynchronize(struct OpenSubdiv_GLMesh *gl_mesh)
{
	((OsdGLMeshInterface *)gl_mesh)->Synchronize();
}

void openSubdiv_osdGLMeshBindvertexBuffer(OpenSubdiv_GLMesh *gl_mesh)
{
	((OsdGLMeshInterface *)gl_mesh)->BindVertexBuffer();
}
