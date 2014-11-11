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
	
	OPolyMeshSchema &schema = m_mesh.getSchema();
	OCompoundProperty geom_props = schema.getArbGeomParams();
	OCompoundProperty user_props = schema.getUserProperties();
	
	m_param_smooth = OBoolGeomParam(geom_props, "smooth", false, kUniformScope, 1, 0);
	
	m_prop_edges = OInt32ArrayProperty(user_props, "edges", 0);
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
	MEdge *me, *medges = output_dm->getEdgeArray(output_dm);
	int totvert = output_dm->getNumVerts(output_dm);
	int totloop = output_dm->getNumLoops(output_dm);
	int totpoly = output_dm->getNumPolys(output_dm);
	int totedge = output_dm->getNumEdges(output_dm);
	int i;
	
	std::vector<V3f> positions;
	positions.reserve(totvert);
	std::vector<int> indices;
	indices.reserve(totloop);
	std::vector<int> counts;
	counts.reserve(totpoly);
	std::vector<bool_t> smooth;
	smooth.reserve(totpoly);
	std::vector<int> edges;
	edges.reserve(totedge * 2);
	
	// TODO decide how to handle vertex/face normals, in caching vs. export ...
//	std::vector<V2f> uvs;
//	OV2fGeomParam::Sample uvs(V2fArraySample(uvs), kFacevaryingScope );
	
	for (i = 0, mv = mverts; i < totvert; ++i, ++mv) {
		float *co = mv->co;
		positions.push_back(V3f(co[0], co[1], co[2]));
	}
	for (i = 0, ml = mloops; i < totloop; ++i, ++ml) {
		indices.push_back(ml->v);
	}
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		counts.push_back(mp->totloop);
		smooth.push_back((bool)(mp->flag & ME_SMOOTH));
	}
	for (i = 0, me = medges; i < totedge; ++i, ++me) {
		edges.push_back(me->v1);
		edges.push_back(me->v2);
	}
	
	OPolyMeshSchema::Sample sample = OPolyMeshSchema::Sample(
	            P3fArraySample(positions),
	            Int32ArraySample(indices),
	            Int32ArraySample(counts)
	            );
	schema.set(sample);
	
	OBoolGeomParam::Sample sample_smooth;
	sample_smooth.setVals(BoolArraySample(smooth));
	sample_smooth.setScope(kUniformScope);
	m_param_smooth.set(sample_smooth);
	
	OInt32ArrayProperty::sample_type sample_edges(edges);
	m_prop_edges.set(sample_edges);
}


PointCacheReader::PointCacheReader(Scene *scene, Object *ob, PointCacheModifierData *pcmd) :
    Reader(scene, &ob->id, pcmd->point_cache),
    m_ob(ob),
    m_pcmd(pcmd),
    m_result(NULL)
{
	if (m_archive.valid()) {
		IObject root = m_archive.getTop();
		if (root.valid() && root.getChild(m_pcmd->modifier.name))
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
	ICompoundProperty geom_props = schema.getArbGeomParams();
	ICompoundProperty user_props = schema.getUserProperties();
	if (!schema.valid() || schema.getPositionsProperty().getNumSamples() == 0)
		return PTC_READ_SAMPLE_INVALID;
	
	ISampleSelector ss = get_frame_sample_selector(frame);
	
	IPolyMeshSchema::Sample sample;
	schema.get(sample, ss);
	
	P3fArraySamplePtr positions = sample.getPositions();
	Int32ArraySamplePtr indices = sample.getFaceIndices();
	Int32ArraySamplePtr counts = sample.getFaceCounts();
	int totverts = positions->size();
	int totloops = indices->size();
	int totpolys = counts->size();
	
	IBoolGeomParam param_smooth(geom_props, "smooth", 0);
	BoolArraySamplePtr smooth;
	if (param_smooth) {
		IBoolGeomParam::Sample sample_smooth;
		param_smooth.getExpanded(sample_smooth, ss);
		smooth = sample_smooth.getVals();
	}
	
	IInt32ArrayProperty prop_edges(user_props, "edges", 0);
	Int32ArraySamplePtr edges;
	if (prop_edges) {
		prop_edges.get(edges, ss);
	}
	BLI_assert(edges->size() % 2 == 0); /* 2 vertex indices per edge */
	int totedges = edges->size() >> 1;
	
	m_result = CDDM_new(totverts, totedges, 0, totloops, totpolys);
	MVert *mv, *mverts = m_result->getVertArray(m_result);
	MLoop *ml, *mloops = m_result->getLoopArray(m_result);
	MPoly *mp, *mpolys = m_result->getPolyArray(m_result);
	MEdge *me, *medges = m_result->getEdgeArray(m_result);
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
	if (smooth) {
		const bool_t *smooth_data = smooth->get();
		bool changed = false;
		for (i = 0, mp = mpolys; i < totpolys; ++i, ++mp) {
			if (smooth_data[i]) {
				mp->flag |= ME_SMOOTH;
				changed = true;
			}
		}
		if (changed)
			m_result->dirty = (DMDirtyFlag)((int)m_result->dirty | DM_DIRTY_NORMALS);
	}
	
	const int32_t *edges_data = edges->get();
	for (i = 0, me = medges; i < totedges; ++i, ++me) {
		me->v1 = edges_data[(i << 1)];
		me->v2 = edges_data[(i << 1) + 1];
	}
	
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

ePointCacheModifierMode PTC_mod_point_cache_get_mode(PointCacheModifierData *pcmd)
{
	/* can't have simultaneous read and write */
	if (pcmd->writer) {
		BLI_assert(!pcmd->reader);
		return MOD_POINTCACHE_MODE_WRITE;
	}
	else if (pcmd->reader) {
		BLI_assert(!pcmd->writer);
		return MOD_POINTCACHE_MODE_READ;
	}
	else
		return MOD_POINTCACHE_MODE_NONE;
}

ePointCacheModifierMode PTC_mod_point_cache_set_mode(Scene *scene, Object *ob, PointCacheModifierData *pcmd, ePointCacheModifierMode mode)
{
	switch (mode) {
		case MOD_POINTCACHE_MODE_READ:
			if (pcmd->writer) {
				PTC_writer_free(pcmd->writer);
				pcmd->writer = NULL;
			}
			if (!pcmd->reader) {
				pcmd->reader = PTC_reader_point_cache(scene, ob, pcmd);
			}
			return pcmd->reader ? MOD_POINTCACHE_MODE_READ : MOD_POINTCACHE_MODE_NONE;
		
		case MOD_POINTCACHE_MODE_WRITE:
			if (pcmd->reader) {
				PTC_reader_free(pcmd->reader);
				pcmd->reader = NULL;
			}
			if (!pcmd->writer) {
				pcmd->writer = PTC_writer_point_cache(scene, ob, pcmd);
			}
			return pcmd->writer ? MOD_POINTCACHE_MODE_WRITE : MOD_POINTCACHE_MODE_NONE;
		
		default:
			if (pcmd->writer) {
				PTC_writer_free(pcmd->writer);
				pcmd->writer = NULL;
			}
			if (pcmd->reader) {
				PTC_reader_free(pcmd->reader);
				pcmd->reader = NULL;
			}
			return MOD_POINTCACHE_MODE_NONE;
	}
}
