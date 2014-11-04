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

#include "PTC_api.h"

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

PointCacheWriter::PointCacheWriter(Scene *scene, Object *ob, PointCacheModifierData *pcmd) :
    Writer(scene, &ob->id, pcmd->point_cache),
    m_ob(ob),
    m_pcmd(pcmd)
{
	uint32_t fs = add_frame_sampling();
	
	OObject root = m_archive.getTop();
	m_mesh = OPolyMesh(root, m_pcmd->modifier.name, fs);
}

PointCacheWriter::~PointCacheWriter()
{
}

void PointCacheWriter::write_sample()
{
	DerivedMesh *output_dm = m_pcmd->output_dm;
	if (!output_dm)
		return;
	
	OPolyMeshSchema &schema = m_mesh.getSchema();
	
	MVert *mv, *mverts = output_dm->getVertArray(output_dm);
	MLoop *ml, *mloops = output_dm->getLoopArray(output_dm);
	MPoly *mp, *mpolys = output_dm->getPolyArray(output_dm);
	int totvert = output_dm->getNumVerts(output_dm);
	int totloop = output_dm->getNumLoops(output_dm);
	int totpoly = output_dm->getNumPolys(output_dm);
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


PointCacheReader::PointCacheReader(Scene *scene, Object *ob, PointCacheModifierData *pcmd) :
    Reader(scene, &ob->id, pcmd->point_cache),
    m_ob(ob),
    m_pcmd(pcmd),
    m_result(NULL)
{
	if (m_archive.valid()) {
		IObject root = m_archive.getTop();
		m_mesh = IPolyMesh(root, m_pcmd->modifier.name);
	}
}

PointCacheReader::~PointCacheReader()
{
}

PTCReadSampleResult PointCacheReader::read_sample(float frame)
{
	/* discard existing result data */
	discard_result();
	
	if (!m_mesh.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	IPolyMeshSchema &schema = m_mesh.getSchema();
//	TimeSamplingPtr ts = schema.getTimeSampling();
	if (!schema.valid() || schema.getPositionsProperty().getNumSamples() == 0)
		return PTC_READ_SAMPLE_INVALID;
	
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
	int loopstart = 0;
	for (i = 0, mp = mpolys; i < totpolys; ++i, ++mp) {
		mp->totloop = counts_data[i];
		mp->loopstart = loopstart;
		
		loopstart += mp->totloop;
	}
	
	CDDM_calc_edges(m_result);
	DM_ensure_normals(m_result);
//	if (!DM_is_valid(m_result))
//		return PTC_READ_SAMPLE_INVALID;
	
	return PTC_READ_SAMPLE_EXACT;
}

DerivedMesh *PointCacheReader::acquire_result()
{
	DerivedMesh *dm = m_result;
	m_result = NULL;
	return dm;
}

void PointCacheReader::discard_result()
{
	if (m_result) {
		m_result->release(m_result);
		m_result = NULL;
	}
}

} /* namespace PTC */


/* ==== C API ==== */

PTCWriter *PTC_writer_point_cache(Scene *scene, Object *ob, PointCacheModifierData *pcmd)
{
	return (PTCWriter *)(new PTC::PointCacheWriter(scene, ob, pcmd));
}

PTCReader *PTC_reader_point_cache(Scene *scene, Object *ob, PointCacheModifierData *pcmd)
{
	return (PTCReader *)(new PTC::PointCacheReader(scene, ob, pcmd));
}

struct DerivedMesh *PTC_reader_point_cache_acquire_result(PTCReader *_reader)
{
	PTC::PointCacheReader *reader = (PTC::PointCacheReader *)_reader;
	return reader->acquire_result();
}

void PTC_reader_point_cache_discard_result(PTCReader *_reader)
{
	PTC::PointCacheReader *reader = (PTC::PointCacheReader *)_reader;
	reader->discard_result();
}
