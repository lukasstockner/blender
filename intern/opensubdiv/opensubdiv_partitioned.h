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
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENSUBDIV_PATITIONED_H__
#define __OPENSUBDIV_PATITIONED_H__

#include <opensubdiv/osd/glMesh.h>
#include <opensubdiv/osdutil/mesh.h>
#include <opensubdiv/osd/cpuComputeController.h>
#include <opensubdiv/osd/glVertexBuffer.h>
#include <opensubdiv/osdutil/patchPartitioner.h>

namespace OpenSubdiv {

template <class DRAW_CONTEXT>
class PartitionedMeshInterface : public OsdMeshInterface<DRAW_CONTEXT> {
    typedef DRAW_CONTEXT DrawContext;
	typedef typename DrawContext::PatchArrayVector PatchArrayVector;

public:
	virtual int GetFVarCount() const = 0;
	virtual int GetNumPartitions() const = 0;
	virtual PatchArrayVector const &GetPatchArrays(int partition) const = 0;
};

typedef PartitionedMeshInterface<OsdGLDrawContext> PartitionedGLMeshInterface;

template <class VERTEX_BUFFER, class COMPUTE_CONTROLLER, class DRAW_CONTEXT>
class PartitionedMesh : public PartitionedMeshInterface<DRAW_CONTEXT>
{
public:
	typedef OsdMesh<VERTEX_BUFFER,
	                COMPUTE_CONTROLLER,
	                DRAW_CONTEXT> Inherited;
	typedef typename Inherited::VertexBuffer VertexBuffer;
	typedef typename Inherited::ComputeController ComputeController;
	typedef typename Inherited::ComputeContext ComputeContext;
	typedef typename Inherited::DrawContext DrawContext;
	typedef typename Inherited::VertexBufferBinding VertexBufferBinding;
	typedef typename DrawContext::PatchArrayVector PatchArrayVector;

	PartitionedMesh(ComputeController *computeController,
	                HbrMesh<OsdVertex> *hmesh,
	                int numVertexElements,
	                int numVaryingElements,
	                int level,
	                OsdMeshBitset bits,
	                std::vector<int> const &partitionPerFace) :
		inherited_impl_(computeController,
		                hmesh,
		                numVertexElements,
		                numVaryingElements,
		                level,
		                bits)
	{
		const FarMesh<OsdVertex> *farMesh = this->GetFarMesh();
		OsdUtilPatchPartitioner partitioner(farMesh->GetPatchTables(),
		                                    partitionPerFace);

		int maxMaterial = partitioner.GetNumPartitions();
		int maxValence = farMesh->GetPatchTables()->GetMaxValence();

		this->_partitionedOsdPatchArrays.resize(maxMaterial);
		for (int i = 0; i < maxMaterial; ++i) {
			OsdDrawContext::ConvertPatchArrays(
				partitioner.GetPatchArrays(i),
				this->_partitionedOsdPatchArrays[i],
				maxValence, numVertexElements);
		}
	}

	virtual int GetNumVertices() const {
		return inherited_impl_.GetNumVertices();
	}

	virtual void UpdateVertexBuffer(float const *vertexData,
	                                int startVertex,
	                                int numVerts) {
		inherited_impl_.UpdateVertexBuffer(vertexData,
		                                   startVertex,
		                                   numVerts);
	}

	virtual void UpdateVaryingBuffer(float const *varyingData,
	                                 int startVertex,
	                                 int numVerts) {
		inherited_impl_.UpdateVaryingBuffer(varyingData,
		                                    startVertex,
		                                    numVerts);
	}

	virtual void Refine() {
		inherited_impl_.Refine();
	}

	virtual void Refine(OsdVertexBufferDescriptor const *vertexDesc,
	                    OsdVertexBufferDescriptor const *varyingDesc,
	                    bool interleaved) {
		inherited_impl_.Refine(vertexDesc, varyingDesc, interleaved);
    }

	virtual void Synchronize() {
		inherited_impl_.Synchronize();
	}

	virtual DrawContext *GetDrawContext() {
		return inherited_impl_.GetDrawContext();
	}

	virtual VertexBufferBinding BindVertexBuffer() {
		return inherited_impl_.BindVertexBuffer();
	}

	virtual VertexBufferBinding BindVaryingBuffer() {
		return inherited_impl_.BindVaryingBuffer();
	}

	virtual FarMesh<OsdVertex> const *GetFarMesh() const {
		return inherited_impl_.GetFarMesh();
	}

	virtual int GetNumPartitions() const {
		return (int)_partitionedOsdPatchArrays.size();
	}

	virtual PatchArrayVector const &GetPatchArrays(int partition) const {
		return _partitionedOsdPatchArrays[partition];
	}

	virtual int GetFVarCount() const {
		const FarMesh<OsdVertex> *farMesh = this->GetFarMesh();
		/* TODO(sergey): We assume all the patches have the same
		 * number of face-varying variables.
		 *
		 * TODO(sergey): Check for empty patch tables here.
		 */
		return farMesh->GetPatchTables()[0].GetFVarData().GetFVarWidth();
	}

private:
	Inherited inherited_impl_;
	std::vector<PatchArrayVector> _partitionedOsdPatchArrays;
};

#ifdef OPENSUBDIV_HAS_OPENCL

template <class VERTEX_BUFFER>
class PartitionedMesh<VERTEX_BUFFER,
                      OsdCLComputeController,
                      OsdGLDrawContext> :
        public PartitionedMeshInterface<OsdGLDrawContext>
{
public:
	typedef OsdMesh<VERTEX_BUFFER,
	                OsdCLComputeController,
	                OsdGLDrawContext> Inherited;
	typedef typename Inherited::VertexBuffer VertexBuffer;
	typedef typename Inherited::ComputeController ComputeController;
	typedef typename Inherited::ComputeContext ComputeContext;
	typedef typename Inherited::DrawContext DrawContext;
	typedef typename Inherited::VertexBufferBinding VertexBufferBinding;
	typedef typename DrawContext::PatchArrayVector PatchArrayVector;

	PartitionedMesh(OsdCLComputeController *computeController,
	                HbrMesh<OsdVertex> *hmesh,
	                int numVertexElements,
	                int numVaryingElements,
	                int level,
	                OsdMeshBitset bits,
	                cl_context clContext,
	                cl_command_queue clQueue,
	                std::vector<int> const &partitionPerFace) :
		inherited_impl_(computeController,
		                hmesh,
		                numVertexElements,
		                numVaryingElements,
		                level,
		                bits,
		                clContext,
		                clQueue)
	{
		const FarMesh<OsdVertex> *farMesh = this->GetFarMesh();
		OsdUtilPatchPartitioner partitioner(farMesh->GetPatchTables(),
		                                    partitionPerFace);

		int maxMaterial = partitioner.GetNumPartitions();
		int maxValence = farMesh->GetPatchTables()->GetMaxValence();

		this->_partitionedOsdPatchArrays.resize(maxMaterial);
		for (int i = 0; i < maxMaterial; ++i) {
			OsdDrawContext::ConvertPatchArrays(
				partitioner.GetPatchArrays(i),
				this->_partitionedOsdPatchArrays[i],
				maxValence, numVertexElements);
		}
	}

	virtual int GetNumVertices() const {
		return inherited_impl_.GetNumVertices();
	}

	virtual void UpdateVertexBuffer(float const *vertexData,
	                                int startVertex,
	                                int numVerts) {
		inherited_impl_.UpdateVertexBuffer(vertexData,
		                                   startVertex,
		                                   numVerts);
	}

	virtual void UpdateVaryingBuffer(float const *varyingData,
	                                 int startVertex,
	                                 int numVerts) {
		inherited_impl_.UpdateVaryingBuffer(varyingData,
		                                    startVertex,
		                                    numVerts);
	}

	virtual void Refine() {
		inherited_impl_.Refine();
	}

	virtual void Refine(OsdVertexBufferDescriptor const *vertexDesc,
	                    OsdVertexBufferDescriptor const *varyingDesc,
	                    bool interleaved) {
		inherited_impl_.Refine(vertexDesc, varyingDesc, interleaved);
    }

	virtual void Synchronize() {
		inherited_impl_.Synchronize();
	}

	virtual DrawContext *GetDrawContext() {
		return inherited_impl_.GetDrawContext();
	}

	virtual VertexBufferBinding BindVertexBuffer() {
		return inherited_impl_.BindVertexBuffer();
	}

	virtual VertexBufferBinding BindVaryingBuffer() {
		return inherited_impl_.BindVaryingBuffer();
	}

	virtual FarMesh<OsdVertex> const *GetFarMesh() const {
		return inherited_impl_.GetFarMesh();
	}

	virtual int GetNumPartitions() const {
		return (int)_partitionedOsdPatchArrays.size();
	}

	virtual PatchArrayVector const &GetPatchArrays(int partition) const {
		return _partitionedOsdPatchArrays[partition];
	}

	virtual int GetFVarCount() const {
		const FarMesh<OsdVertex> *farMesh = this->GetFarMesh();
		/* TODO(sergey): We assume all the patches have the same
		 * number of face-varying variables.
		 *
		 * TODO(sergey): Check for empty patch tables here.
		 */
		return farMesh->GetPatchTables()[0].GetFVarData().GetFVarWidth();
	}

private:
	Inherited inherited_impl_;
	std::vector<PatchArrayVector> _partitionedOsdPatchArrays;
};

#endif  // OPENSUBDIV_HAS_OPENCL

}  // namespace OpenSubdiv

#endif  // __OPENSUBDIV_PATITIONED_H__
