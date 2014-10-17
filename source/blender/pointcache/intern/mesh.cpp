/*
 * Copyright 2014, Blender Foundation.
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
 */

#include "mesh.h"

extern "C" {
#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

MeshCacheWriter::MeshCacheWriter(Scene *scene, Object *ob, MeshCacheModifierData *mcmd) :
    Writer(scene, &ob->id, mcmd->point_cache),
    m_ob(ob),
    m_mcmd(mcmd)
{
	uint32_t fs = add_frame_sampling();
	
	OObject root = m_archive.getTop();
	m_mesh = OPolyMesh(root, m_mcmd->modifier.name, fs);
}

MeshCacheWriter::~MeshCacheWriter()
{
}

void MeshCacheWriter::write_sample()
{
	DerivedMesh *source_dm = m_ob->derivedFinal;
	if (!source_dm)
		return;
	
	OPolyMeshSchema &schema = m_mesh.getSchema();
	
	MVert *mv, *mverts = source_dm->getVertArray(source_dm);
	MLoop *ml, *mloops = source_dm->getLoopArray(source_dm);
	MPoly *mp, *mpolys = source_dm->getPolyArray(source_dm);
	int totvert = source_dm->getNumVerts(source_dm);
	int totloop = source_dm->getNumLoops(source_dm);
	int totpoly = source_dm->getNumPolys(source_dm);
	int i;
	
	std::vector<V3f> positions;
	positions.reserve(totvert);
	std::vector<int> indices;
	indices.reserve(totloop);
	std::vector<int> counts;
	counts.reserve(totpoly);
	
	for (i = 0, mv = mverts; i < totvert; ++i, ++mv) {
		float *co = mv->co;
		positions.push_back(V3f(co[0], co[1], co[2]));
	}
	for (i = 0, ml = mloops; i < totloop; ++i, ++ml) {
		indices.push_back(ml->v);
	}
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		counts.push_back(mp->totloop);
	}
	
	OPolyMeshSchema::Sample sample = OPolyMeshSchema::Sample(
	            P3fArraySample(positions),
	            Int32ArraySample(indices),
	            Int32ArraySample(counts)
	            );

	schema.set(sample);
}


MeshCacheReader::MeshCacheReader(Scene *scene, Object *ob, MeshCacheModifierData *mcmd) :
    Reader(scene, &ob->id, mcmd->point_cache),
    m_ob(ob),
    m_mcmd(mcmd),
    m_result(NULL)
{
	if (m_archive.valid()) {
		IObject root = m_archive.getTop();
		m_mesh = IPolyMesh(root, m_mcmd->modifier.name);
	}
}

MeshCacheReader::~MeshCacheReader()
{
}

PTCReadSampleResult MeshCacheReader::read_sample(float frame)
{
	/* discard existing result data */
	discard_result();
	
	if (!m_mesh.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	IPolyMeshSchema &schema = m_mesh.getSchema();
//	TimeSamplingPtr ts = schema.getTimeSampling();
	
	ISampleSelector ss = get_frame_sample_selector(frame);
//	chrono_t time = ss.getRequestedTime();
	
//	std::pair<index_t, chrono_t> sres = ts->getFloorIndex(time, schema.getNumSamples());
//	chrono_t stime = sres.second;
//	float sframe = time_to_frame(stime);
	
	IPolyMeshSchema::Sample sample;
	schema.get(sample, ss);
	
	P3fArraySamplePtr positions = sample.getPositions();
	Int32ArraySamplePtr indices = sample.getFaceIndices();
	Int32ArraySamplePtr counts = sample.getFaceCounts();
	int totverts = positions->size();
	int totloops = indices->size();
	int totpolys = counts->size();
	
	m_result = CDDM_new(totverts, 0, 0, totloops, totpolys);
	MVert *mv, *mverts = m_result->getVertArray(m_result);
	MLoop *ml, *mloops = m_result->getLoopArray(m_result);
	MPoly *mp, *mpolys = m_result->getPolyArray(m_result);
	int i;
	
	const V3f *positions_data = positions->get();
	for (i = 0, mv = mverts; i < totverts; ++i, ++mv) {
		const V3f &co = positions_data[i];
		copy_v3_v3(mv->co, co.getValue());
	}
	const int32_t *indices_data = indices->get();
	for (i = 0, ml = mloops; i < totloops; ++i, ++ml) {
		ml->v = indices_data[i];
	}
	const int32_t *counts_data = counts->get();
	for (i = 0, mp = mpolys; i < totpolys; ++i, ++mp) {
		mp->totloop = counts_data[i];
	}
	
	return PTC_READ_SAMPLE_EXACT;
}

DerivedMesh *MeshCacheReader::acquire_result()
{
	DerivedMesh *dm = m_result;
	m_result = NULL;
	return dm;
}

void MeshCacheReader::discard_result()
{
	if (m_result) {
		m_result->release(m_result);
		m_result = NULL;
	}
}

} /* namespace PTC */
