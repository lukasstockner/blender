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

#include <vector>

#include <opensubdiv/osd/vertex.h>
#include <opensubdiv/osd/mesh.h>
#include <opensubdiv/osd/cpuComputeController.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>
#include <opensubdiv/osd/cpuEvalLimitController.h>
#include <opensubdiv/osd/evalLimitContext.h>

#include "MEM_guardedalloc.h"

#if !defined(WITH_ASSERT_ABORT)
#  define OSD_abort()
#else
#  include <stdlib.h>
#  define OSD_abort() abort()
#endif

#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#endif

/* TODO(sergey): de-duplicate from OpenColorIO C-API. */
#define OBJECT_NEW(type, args ...) new(MEM_mallocN(sizeof(type), __func__)) type
#define OBJECT_DELETE(what, type) if(what) { ((type*)(what))->~type(); MEM_freeN(what); } (void)0

/* Define this when you want to have additional verbosity
 * about what's being passed to OpenSubdiv.
 */
#undef DEBUG_PRINT

/* **************** Types declaration **************** */

typedef OpenSubdiv::OsdVertex OsdVertex;
typedef OpenSubdiv::FarMesh<OsdVertex> OsdFarMesh;
typedef OpenSubdiv::FarMeshFactory<OsdVertex> OsdFarMeshFactory;
typedef OpenSubdiv::HbrCatmarkSubdivision<OsdVertex> OsdHbrCatmarkSubdivision;
typedef OpenSubdiv::HbrFace<OsdVertex> OsdHbrFace;
typedef OpenSubdiv::HbrHalfedge<OsdVertex> OsdHbrHalfEdge;
typedef OpenSubdiv::HbrMesh<OsdVertex> OsdHbrMesh;
typedef OpenSubdiv::HbrVertex<OsdVertex> OsdHbrVertex;
typedef OpenSubdiv::OsdCpuComputeContext OsdCpuComputeContext;
typedef OpenSubdiv::OsdCpuComputeController OsdCpuComputeController;
typedef OpenSubdiv::OsdCpuEvalLimitContext OsdCpuEvalLimitContext;
typedef OpenSubdiv::OsdCpuEvalLimitController OsdCpuEvalLimitController;
typedef OpenSubdiv::OsdCpuVertexBuffer OsdCpuVertexBuffer;
typedef OpenSubdiv::OsdEvalCoords OsdEvalCoords;
typedef OpenSubdiv::OsdVertexBufferDescriptor OsdVertexBufferDescriptor;

typedef struct OpenSubdiv_MeshDescr {
	OpenSubdiv_HbrCatmarkSubdivision *subdivision;
	OpenSubdiv_HbrMesh *hbr_mesh;
	std::vector<float> positions;
    int num_verts, num_ptex_faces;
} OpenSubdiv_MeshDescr;

typedef struct OpenSubdiv_EvaluationDescr {
	/* This are allocated by OpenSubdiv, not guarded alloacated */
	OsdFarMesh *farmesh;
	OsdCpuComputeContext *compute_context;
	OsdCpuEvalLimitContext *evalctx;
	OsdCpuVertexBuffer *vbuf_base;
	OsdCpuVertexBuffer *vbuf_P;
	OsdCpuVertexBuffer *vbuf_dPdu;
	OsdCpuVertexBuffer *vbuf_dPdv;
	float *P;
	float *dPdu;
	float *dPdv;

	/* This are guarded allocated */
	OsdCpuComputeController *compute_controller;
	OsdCpuEvalLimitController *evalctrl;
} OpenSubdiv_EvaluationDescr;

/* **************** HBR Catmark functions **************** */

struct OpenSubdiv_HbrCatmarkSubdivision *openSubdiv_createHbrCatmarkSubdivision(void)
{
	OsdHbrCatmarkSubdivision *catmark_subdivision = OBJECT_NEW(OsdHbrCatmarkSubdivision)();
	return (OpenSubdiv_HbrCatmarkSubdivision *) catmark_subdivision;
}

void openSubdiv_deleteHbrCatmarkSubdivision(struct OpenSubdiv_HbrCatmarkSubdivision *sundivision)
{
	OBJECT_DELETE(sundivision, OsdHbrCatmarkSubdivision);
}

/* **************** HBR mesh functions **************** */

struct OpenSubdiv_HbrMesh *openSubdiv_createCatmarkHbrMesh(struct OpenSubdiv_HbrCatmarkSubdivision *subdivision)
{
	OsdHbrMesh *hbr_mesh = OBJECT_NEW(OsdHbrMesh)((OsdHbrCatmarkSubdivision *) subdivision);
	return (OpenSubdiv_HbrMesh *) hbr_mesh;
}

void openSubdiv_deleteHbrMesh(struct OpenSubdiv_HbrMesh *mesh)
{
	OBJECT_DELETE(mesh, OsdHbrMesh);
}

void openSubdiv_createHbrMeshVertex(struct OpenSubdiv_HbrMesh *mesh, int id)
{
	OsdHbrMesh *osd_hbrmesh = (OsdHbrMesh*) mesh;

	OsdVertex v;
	osd_hbrmesh->NewVertex(id, v);
}

void openSubdiv_createHbrMeshTriFace(struct OpenSubdiv_HbrMesh *mesh, int v0, int v1, int v2)
{
	int indices[3] = {v0, v1, v2};
	openSubdiv_createHbrMeshFace(mesh, 3, indices);
}

void openSubdiv_createHbrMeshQuadFace(struct OpenSubdiv_HbrMesh *mesh, int v0, int v1, int v2, int v3)
{
	int indices[4] = {v0, v1, v2, v3};
	openSubdiv_createHbrMeshFace(mesh, 4, indices);
}

static bool newFaceSanityCheck(OsdHbrMesh *mesh, int num_vertices, int *indices)
{
#ifndef NDEBUG
	/* Do some sanity checking. It is a good idea to keep this in your
	 * code for your personal sanity as well.
	 *
	 * Note that this loop is not changing the HbrMesh, it's purely validating
	 * the topology that is about to be created below.
	 */
	for (int j = 0; j < num_vertices; j++) {
		OsdHbrVertex *origin = mesh->GetVertex(indices[j]);
		OsdHbrVertex *destination = mesh->GetVertex(indices[(j + 1) % num_vertices]);
		OsdHbrHalfEdge *opposite = destination->GetEdge(origin);

		if (origin == NULL || destination == NULL) {
			fprintf(stderr, "An edge was specified that connected a nonexistent vertex\n");
			OSD_abort();
			return false;
		}

		if (origin == destination) {
			fprintf(stderr, "An edge was specified that connected a vertex to itself\n");
			OSD_abort();
			return false;
		}

		if (opposite && opposite->GetOpposite()) {
			fprintf(stderr, "A non-manifold edge incident to more than 2 faces was found\n");
			OSD_abort();
			return false;
		}

		if (origin->GetEdge(destination)) {
			fprintf(stderr, "An edge connecting two vertices was specified more than once.\n"
			                 "It's likely that an incident face was flipped\n");
			OSD_abort();
			return false;
		}
    }
#endif

	return true;
}

void openSubdiv_createHbrMeshFace(struct OpenSubdiv_HbrMesh *mesh, int num_vertices, int *indices)
{
	OsdHbrMesh *osd_hbrmesh = (OsdHbrMesh *) mesh;

	if (!newFaceSanityCheck(osd_hbrmesh, num_vertices, indices)) {
		return;
	}

	osd_hbrmesh->NewFace(num_vertices, indices, 0);
}

void openSubdiv_finishHbrMesh(struct OpenSubdiv_HbrMesh *mesh)
{
	OsdHbrMesh *osd_hbrmesh = (OsdHbrMesh *) mesh;

	/* Finish HBR mesh construction. */

	/* Apply some tags to drive the subdivision algorithm. Here we set the
	 * default boundary interpolation mode along with a corner sharpness.
	 */
	osd_hbrmesh->SetInterpolateBoundaryMethod(OsdHbrMesh::k_InterpolateBoundaryEdgeOnly);
	osd_hbrmesh->Finish();
}

/* **************** Mesh descriptor functions **************** */

OpenSubdiv_MeshDescr *openSubdiv_createMeshDescr(void)
{
	OpenSubdiv_MeshDescr *mesh_descr =
		(OpenSubdiv_MeshDescr *) MEM_callocN(sizeof(OpenSubdiv_MeshDescr), "opensubd mesh descr");

	mesh_descr->subdivision = openSubdiv_createHbrCatmarkSubdivision();
	mesh_descr->hbr_mesh = openSubdiv_createCatmarkHbrMesh(mesh_descr->subdivision);

	return mesh_descr;
}

void openSubdiv_deleteMeshDescr(OpenSubdiv_MeshDescr *mesh_descr)
{
	openSubdiv_deleteHbrMesh(mesh_descr->hbr_mesh);
	openSubdiv_deleteHbrCatmarkSubdivision(mesh_descr->subdivision);
	MEM_freeN(mesh_descr);
}

void openSubdiv_createMeshDescrVertex(OpenSubdiv_MeshDescr *mesh_descr, const float coord[3])
{
#ifdef DEBUG_PRINT
	printf("Adding vertex id:%d coord:(%f %f %f)\n", mesh_descr->num_verts, coord[0], coord[1], coord[2]);
#endif

	openSubdiv_createHbrMeshVertex(mesh_descr->hbr_mesh, mesh_descr->num_verts++);

    mesh_descr->positions.push_back(coord[0]);
    mesh_descr->positions.push_back(coord[1]);
    mesh_descr->positions.push_back(coord[2]);
}

void openSubdiv_createMeshDescrTriFace(OpenSubdiv_MeshDescr *mesh_descr, int v0, int v1, int v2)
{
	int indices[3] = {v0, v1, v2};
	openSubdiv_createMeshDescrFace(mesh_descr, 3, indices);
}

void openSubdiv_createMeshDescrQuadFace(OpenSubdiv_MeshDescr *mesh_descr, int v0, int v1, int v2, int v3)
{
	int indices[4] = {v0, v1, v2, v3};
	openSubdiv_createMeshDescrFace(mesh_descr, 4, indices);
}

void openSubdiv_createMeshDescrFace(OpenSubdiv_MeshDescr *mesh_descr, int num_vertices, int *indices)
{
	/* TODO(sergey): it shall be posible to de-duplicate code */
	OsdHbrMesh *osd_hbrmesh = (OsdHbrMesh *) mesh_descr->hbr_mesh;

#ifdef DEBUG_PRINT
	printf("Adding face ptex_index:%d vertices:", mesh_descr->num_ptex_faces);
	for (int i = 0; i < num_vertices; i++) {
		printf("%d ", indices[i]);
	}
	printf("\n");
#endif

	if (!newFaceSanityCheck(osd_hbrmesh, num_vertices, indices)) {
		return;
	}

	OsdHbrFace *face = osd_hbrmesh->NewFace(num_vertices, indices, 0);

	face->SetPtexIndex(mesh_descr->num_ptex_faces);

    if (num_vertices == 4) {
        mesh_descr->num_ptex_faces++;
	}
    else {
        mesh_descr->num_ptex_faces += num_vertices;
	}
}

void openSubdiv_finishMeshDescr(OpenSubdiv_MeshDescr *mesh_descr)
{
	openSubdiv_finishHbrMesh(mesh_descr->hbr_mesh);
}

/* **************** Evaluation functions **************** */

OpenSubdiv_EvaluationDescr *openSubdiv_createEvaluationDescr(OpenSubdiv_MeshDescr *mesh_descr)
{
	OpenSubdiv_EvaluationDescr *evaluation_descr =
		(OpenSubdiv_EvaluationDescr *) MEM_mallocN(sizeof(OpenSubdiv_EvaluationDescr),
		                                      "opensubd evaluationdescr");

	const int level = 3;
	const bool requirefvar = false;

	/* Convert HRB to FAR mesh. */
	OsdHbrMesh *hbrmesh = (OsdHbrMesh *) mesh_descr->hbr_mesh;

	OsdFarMeshFactory meshFactory(hbrmesh, level, true);
	OsdFarMesh *farmesh = meshFactory.Create(requirefvar);
	int num_hbr_verts = hbrmesh->GetNumVertices();

	/* Refine HBR mesh with vertex coordinates. */
	OsdCpuComputeController *compute_controller = OBJECT_NEW(OsdCpuComputeController)();
	OsdCpuComputeContext *compute_context = OsdCpuComputeContext::Create(farmesh);

	OsdCpuVertexBuffer *vbuf_base = OsdCpuVertexBuffer::Create(3, num_hbr_verts);
	vbuf_base->UpdateData(&mesh_descr->positions[0], 0, mesh_descr->num_verts);

	compute_controller->Refine(compute_context, farmesh->GetKernelBatches(), vbuf_base);
	compute_controller->Synchronize();

	/* Create buffers for evaluation. */
	OsdCpuVertexBuffer *vbuf_P = OsdCpuVertexBuffer::Create(3, 1);
	OsdCpuVertexBuffer *vbuf_dPdu = OsdCpuVertexBuffer::Create(3, 1);
	OsdCpuVertexBuffer *vbuf_dPdv = OsdCpuVertexBuffer::Create(3, 1);

	float *P = vbuf_P->BindCpuBuffer();
	float *dPdu = vbuf_dPdu->BindCpuBuffer();
	float *dPdv = vbuf_dPdv->BindCpuBuffer();

	/* Setup evaluation context. */
	OsdVertexBufferDescriptor in_desc(0, 3, 3), out_desc(0, 3, 3); /* offset, length, stride */

	OsdCpuEvalLimitContext *evalctx = OsdCpuEvalLimitContext::Create(farmesh, false);
	evalctx->GetVertexData().Bind(in_desc, vbuf_base, out_desc, vbuf_P, vbuf_dPdu, vbuf_dPdv);

	/* Store objects in an evaluation context. */
	evaluation_descr->farmesh = farmesh;
	evaluation_descr->compute_context = compute_context;
	evaluation_descr->evalctx = evalctx;
	evaluation_descr->vbuf_base = vbuf_base;
	evaluation_descr->vbuf_P = vbuf_P;
	evaluation_descr->vbuf_dPdu = vbuf_dPdu;
	evaluation_descr->vbuf_dPdv = vbuf_dPdv;
	evaluation_descr->P = P;
	evaluation_descr->dPdu = dPdu;
	evaluation_descr->dPdv = dPdv;
	evaluation_descr->compute_controller = compute_controller;

	evaluation_descr->evalctrl = OBJECT_NEW(OsdCpuEvalLimitController)();

	return evaluation_descr;
}

void openSubdiv_deleteEvaluationDescr(OpenSubdiv_EvaluationDescr *evaluation_descr)
{
	delete evaluation_descr->farmesh;
	delete evaluation_descr->compute_context;
	delete evaluation_descr->vbuf_base;

	delete evaluation_descr->evalctx;
	delete evaluation_descr->vbuf_P;
	delete evaluation_descr->vbuf_dPdu;
	delete evaluation_descr->vbuf_dPdv;

	OBJECT_DELETE(evaluation_descr->compute_controller, OsdCpuComputeController);
	OBJECT_DELETE(evaluation_descr->evalctrl, OsdCpuEvalLimitController);

	MEM_freeN(evaluation_descr);
}

void openSubdiv_evaluateDescr(OpenSubdiv_EvaluationDescr *evaluation_descr,
                              int face_id, float u, float v,
                              float P[3], float dPdu[3], float dPdv[3])
{
	OsdEvalCoords coords;
	coords.u = u;
	coords.v = v;
	coords.face = face_id;

	evaluation_descr->evalctrl->EvalLimitSample<OsdCpuVertexBuffer, OsdCpuVertexBuffer>(coords, evaluation_descr->evalctx, 0);

	memcpy(P, evaluation_descr->P, sizeof(float) * 3);
	if (dPdu) {
		memcpy(dPdu, evaluation_descr->dPdu, sizeof(float) * 3);
	}
	if (dPdv) {
		memcpy(dPdv, evaluation_descr->dPdv, sizeof(float) * 3);
	}
}
