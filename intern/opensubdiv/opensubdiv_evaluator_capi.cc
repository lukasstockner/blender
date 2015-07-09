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

#include "opensubdiv_capi.h"

#include <cstdio>
#include <vector>

#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTable.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/osd/cpuEvaluator.h>
#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>
#include <opensubdiv/osd/mesh.h>
#include <opensubdiv/osd/types.h>

#include "opensubdiv_converter.h"

using OpenSubdiv::Osd::BufferDescriptor;
using OpenSubdiv::Osd::PatchCoord;
using OpenSubdiv::Far::PatchTable;
using OpenSubdiv::Far::PatchTableFactory;
using OpenSubdiv::Far::StencilTable;
using OpenSubdiv::Far::StencilTableFactory;
using OpenSubdiv::Far::TopologyRefiner;
using OpenSubdiv::Far::TopologyRefinerFactory;

namespace {

class PatchCoordBuffer : public std::vector<PatchCoord> {
public:
	static PatchCoordBuffer *Create(int size)
	{
		PatchCoordBuffer *buffer = new PatchCoordBuffer();
		buffer->resize(size);
		return buffer;
	}
	PatchCoord *BindCpuBuffer() {
		return (PatchCoord*)&(*this)[0];
	}
	int GetNumVertices() {
		return size();
	}
	void UpdateData(const PatchCoord *patch_coords,
	                int num_patch_coords)
	{
		memcpy(&(*this)[0],
		       (void*)patch_coords,
		       num_patch_coords * sizeof(PatchCoord));
	}
};

template<typename SRC_VERTEX_BUFFER,
         typename EVAL_VERTEX_BUFFER,
         typename STENCIL_TABLE,
         typename PATCH_TABLE,
         typename EVALUATOR,
         typename DEVICE_CONTEXT = void>
class EvalOutput {
public:
	typedef OpenSubdiv::Osd::EvaluatorCacheT<EVALUATOR> EvaluatorCache;

	EvalOutput(StencilTable const *vertex_stencils,
	           StencilTable const *varying_stencils,
	           int num_coarse_verts,
	           int num_total_verts,
	           int num_particles,
	           PatchTable const *patch_table,
	           EvaluatorCache *evaluator_cache = NULL,
	           DEVICE_CONTEXT *device_context = NULL)
	    : src_desc_(        /*offset*/ 0, /*length*/ 3, /*stride*/ 3),
	      src_varying_desc_(/*offset*/ 0, /*length*/ 3, /*stride*/ 3),
	      vertex_desc_(     /*offset*/ 0, /*legnth*/ 3, /*stride*/ 6),
	      varying_desc_(    /*offset*/ 3, /*legnth*/ 3, /*stride*/ 6),
	      du_desc_(         /*offset*/ 0, /*legnth*/ 3, /*stride*/ 6),
	      dv_desc_(         /*offset*/ 3, /*legnth*/ 3, /*stride*/ 6),
	      num_coarse_verts_(num_coarse_verts),
	      evaluator_cache_ (evaluator_cache),
	      device_context_(device_context)
	{
		using OpenSubdiv::Osd::convertToCompatibleStencilTable;
		src_data_ = SRC_VERTEX_BUFFER::Create(3, num_total_verts, device_context_);
		src_varying_data_ = SRC_VERTEX_BUFFER::Create(3, num_total_verts, device_context_);
		vertex_data_ = EVAL_VERTEX_BUFFER::Create(6, num_particles, device_context_);
		derivatives_ = EVAL_VERTEX_BUFFER::Create(6, num_particles, device_context_);
		patch_table_ = PATCH_TABLE::Create(patch_table, device_context_);
		patch_coords_ = NULL;
		vertex_stencils_ = convertToCompatibleStencilTable<STENCIL_TABLE>(vertex_stencils,
		                                                                  device_context_);
		varying_stencils_ = convertToCompatibleStencilTable<STENCIL_TABLE>(varying_stencils,
		                                                                   device_context_);
	}

	~EvalOutput()
	{
		delete src_data_;
		delete src_varying_data_;
		delete vertex_data_;
		delete derivatives_;
		delete patch_table_;
		delete patch_coords_;
		delete vertex_stencils_;
		delete varying_stencils_;
	}

	float *BindCpuVertexData() const
	{
		return vertex_data_->BindCpuBuffer();
	}

	void UpdateData(const float *src, int start_vertex, int num_vertices)
	{
		src_data_->UpdateData(src, start_vertex, num_vertices, device_context_);
	}

	void UpdateVaryingData(const float *src, int start_vertex, int num_vertices)
	{
		src_varying_data_->UpdateData(src,
		                              start_vertex,
		                              num_vertices,
		                              device_context_);
	}

	void Refine()
	{
		BufferDescriptor dst_desc = src_desc_;
		dst_desc.offset += num_coarse_verts_ * src_desc_.stride;

		EVALUATOR const *eval_instance =
		        OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
		                                                 src_desc_,
		                                                 dst_desc,
		                                                 device_context_);

		EVALUATOR::EvalStencils(src_data_, src_desc_,
		                        src_data_, dst_desc,
		                        vertex_stencils_,
		                        eval_instance,
		                        device_context_);

		dst_desc = src_varying_desc_;
		dst_desc.offset += num_coarse_verts_ * src_varying_desc_.stride;
		eval_instance =
		        OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
		                                                 src_varying_desc_,
		                                                 dst_desc,
		                                                 device_context_);

		EVALUATOR::EvalStencils(src_varying_data_, src_varying_desc_,
		                        src_varying_data_, dst_desc,
		                        varying_stencils_,
		                        eval_instance,
		                        device_context_);
	}

	void EvalPatches()
	{
		EVALUATOR const *eval_instance =
		        OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
		                                                 src_desc_,
		                                                 vertex_desc_,
		                                                 device_context_);

		EVALUATOR::EvalPatches(src_data_, src_desc_,
		                       vertex_data_, vertex_desc_,
		                       patch_coords_->GetNumVertices(),
		                       patch_coords_,
		                       patch_table_, eval_instance, device_context_);
	}

	void EvalPatchesWithDerivatives()
	{
		EVALUATOR const *eval_instance =
		        OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
		                                                 src_desc_,
		                                                 vertex_desc_,
		                                                 du_desc_,
		                                                 dv_desc_,
		                                                 device_context_);
		EVALUATOR::EvalPatches(src_data_, src_desc_,
		                       vertex_data_, vertex_desc_,
		                       derivatives_, du_desc_,
		                       derivatives_, dv_desc_,
		                       patch_coords_->GetNumVertices(),
		                       patch_coords_,
		                       patch_table_, eval_instance, device_context_);
	}

	void EvalPatchesVarying()
	{
		EVALUATOR const *eval_instance =
		        OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
		                                                 src_varying_desc_,
		                                                 varying_desc_,
		                                                 device_context_);

		EVALUATOR::EvalPatches(src_varying_data_, src_varying_desc_,
		                       /* Varying data is interleved in vertexData. */
		                       vertex_data_, varying_desc_,
		                       patch_coords_->GetNumVertices(),
		                       patch_coords_,
		                       patch_table_, eval_instance, device_context_);
	}

	void UpdatePatchCoords(std::vector<PatchCoord> const &patchCoords)
	{
		int new_size = (int)patchCoords.size();
		if (patch_coords_ != NULL && patch_coords_->GetNumVertices() != new_size) {
			delete patch_coords_;
			patch_coords_ = NULL;
		}
		if (patch_coords_ == NULL) {
			patch_coords_ = PatchCoordBuffer::Create(new_size);
		}
		patch_coords_->UpdateData(&patchCoords[0], new_size);
	}
private:
	SRC_VERTEX_BUFFER *src_data_;
	SRC_VERTEX_BUFFER *src_varying_data_;
	EVAL_VERTEX_BUFFER *vertex_data_;
	EVAL_VERTEX_BUFFER *derivatives_;
	EVAL_VERTEX_BUFFER *varying_data_;
	PatchCoordBuffer *patch_coords_;
	PATCH_TABLE *patch_table_;
	BufferDescriptor src_desc_;
	BufferDescriptor src_varying_desc_;
	BufferDescriptor vertex_desc_;
	BufferDescriptor varying_desc_;
	BufferDescriptor du_desc_;
	BufferDescriptor dv_desc_;
	int num_coarse_verts_;

	STENCIL_TABLE const *vertex_stencils_;
	STENCIL_TABLE const *varying_stencils_;

	EvaluatorCache *evaluator_cache_;
	DEVICE_CONTEXT *device_context_;
};

}  /* namespace */

typedef EvalOutput<OpenSubdiv::Osd::CpuVertexBuffer,
                   OpenSubdiv::Osd::CpuVertexBuffer,
                   OpenSubdiv::Far::StencilTable,
                   OpenSubdiv::Osd::CpuPatchTable,
                   OpenSubdiv::Osd::CpuEvaluator> CpuEvalOutput;

void openSubdiv_evaluateLimit(//OpenSubdiv_EvaluatorDescr *evaluator_descr,
                              DerivedMesh *dm,
                              int osd_face_index,
                              float face_u, float face_v,
                              float P[3],
                              float dPdu[3],
                              float dPdv[3])
{
	OsdBlenderConverter conv(dm);
	TopologyRefiner * refiner =
	        TopologyRefinerFactory<OsdBlenderConverter>::Create(
	                conv,
	                TopologyRefinerFactory<OsdBlenderConverter>::Options(conv.get_type(),
	                                                                     conv.get_options()));

	OpenSubdiv::Far::StencilTable const * vertex_stencils = NULL;
	OpenSubdiv::Far::StencilTable const * varying_stencils = NULL;
	OpenSubdiv::Far::PatchTable const * g_patchTable;
	int nverts=0;
	{
		// Apply feature adaptive refinement to the mesh so that we can use the
		// limit evaluation API features.
		OpenSubdiv::Far::TopologyRefiner::UniformOptions options(1);
		refiner->RefineUniform(options);

		// Generate stencil table to update the bi-cubic patches control
		// vertices after they have been re-posed (both for vertex & varying
		// interpolation)
		StencilTableFactory::Options soptions;
		soptions.generateOffsets = true;
		soptions.generateIntermediateLevels = true;

		vertex_stencils = StencilTableFactory::Create(*refiner, soptions);

		soptions.interpolationMode = StencilTableFactory::INTERPOLATE_VARYING;
		varying_stencils = StencilTableFactory::Create(*refiner, soptions);

		// Generate bi-cubic patch table for the limit surface
		PatchTableFactory::Options poptions;
		poptions.SetEndCapType(PatchTableFactory::Options::ENDCAP_BSPLINE_BASIS);

		PatchTable const * patchTable = PatchTableFactory::Create(*refiner, poptions);

		// append local points stencils
		if (StencilTable const *local_point_stencil_table =
		     patchTable->GetLocalPointStencilTable())
		{
			StencilTable const *table =
				StencilTableFactory::AppendLocalPointStencilTable(*refiner,
				                                                  vertex_stencils,
				                                                  local_point_stencil_table);
			delete vertex_stencils;
			vertex_stencils = table;
		}
		if (StencilTable const *local_point_varying_stencil_table =
		     patchTable->GetLocalPointVaryingStencilTable())
		{
			StencilTable const *table =
				StencilTableFactory::AppendLocalPointStencilTable(*refiner,
				                                                  varying_stencils,
				                                                  local_point_varying_stencil_table);
			delete varying_stencils;
			varying_stencils = table;
		}

		// total number of vertices = coarse verts + refined verts + gregory basis verts
		nverts = vertex_stencils->GetNumControlVertices() +
			vertex_stencils->GetNumStencils();

		g_patchTable = patchTable;
	}

	const int nCoarseVertices = refiner->GetLevel(0).GetNumVertices();
	const int g_nParticles = 1;

	CpuEvalOutput *g_evalOutput = new CpuEvalOutput(vertex_stencils,
	                                                varying_stencils,
	                                                nCoarseVertices,
	                                                nverts,
	                                                g_nParticles,
	                                                g_patchTable);

	float g_positions[3*1024];
	conv.get_coarse_verts(g_positions);
	g_evalOutput->UpdateData(&g_positions[0], 0, nCoarseVertices);
	g_evalOutput->Refine();
	OpenSubdiv::Far::PatchMap patchMap(*g_patchTable);
	std::vector<PatchCoord> patchCoords;
	PatchTable::PatchHandle const *handle =
		patchMap.FindPatch(osd_face_index, face_u, face_v);
	PatchCoord patchCoord(*handle, face_u, face_v);
	patchCoords.push_back(patchCoord);
	g_evalOutput->UpdatePatchCoords(patchCoords);
	g_evalOutput->EvalPatches();

	float *refined_verts = g_evalOutput->BindCpuVertexData();

	P[0] = refined_verts[0];
	P[1] = refined_verts[1];
	P[2] = refined_verts[2];

	if (dPdu) {
		dPdu[0] = 1.0f;
		dPdu[1] = 0.0f;
		dPdu[2] = 0.0f;
	}
	if (dPdv) {
		dPdv[0] = 0.0f;
		dPdv[1] = 1.0f;
		dPdv[2] = 0.0f;
	}
	delete varying_stencils;
	delete g_patchTable;
	delete vertex_stencils;
	delete refiner;
	delete g_evalOutput;
}
