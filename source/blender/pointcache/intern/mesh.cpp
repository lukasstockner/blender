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
	set_error_handler(new ModifierErrorHandler(&pcmd->modifier));
	
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

static P3fArraySample create_sample_positions(DerivedMesh *dm, std::vector<V3f> &data)
{
	MVert *mv, *mverts = dm->getVertArray(dm);
	int i, totvert = dm->getNumVerts(dm);
	
	data.reserve(totvert);
	for (i = 0, mv = mverts; i < totvert; ++i, ++mv) {
		float *co = mv->co;
		data.push_back(V3f(co[0], co[1], co[2]));
	}
	
	return P3fArraySample(data);
}

static Int32ArraySample create_sample_vertex_indices(DerivedMesh *dm, std::vector<int> &data)
{
	MLoop *ml, *mloops = dm->getLoopArray(dm);
	int i, totloop = dm->getNumLoops(dm);
	
	data.reserve(totloop);
	for (i = 0, ml = mloops; i < totloop; ++i, ++ml) {
		data.push_back(ml->v);
	}
	
	return Int32ArraySample(data);
}

static Int32ArraySample create_sample_loop_counts(DerivedMesh *dm, std::vector<int> &data)
{
	MPoly *mp, *mpolys = dm->getPolyArray(dm);
	int i, totpoly = dm->getNumPolys(dm);
	
	data.reserve(totpoly);
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		data.push_back(mp->totloop);
	}
	
	return Int32ArraySample(data);
}

static OBoolGeomParam::Sample create_sample_poly_smooth(DerivedMesh *dm, std::vector<bool_t> &data)
{
	MPoly *mp, *mpolys = dm->getPolyArray(dm);
	int i, totpoly = dm->getNumPolys(dm);
	
	data.reserve(totpoly);
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		data.push_back((bool)(mp->flag & ME_SMOOTH));
	}
	
	OBoolGeomParam::Sample sample;
	sample.setVals(BoolArraySample(data));
	sample.setScope(kUniformScope);
	return sample;
}

static OInt32ArrayProperty::sample_type create_sample_edge_vertices(DerivedMesh *dm, std::vector<int> &data)
{
	MEdge *me, *medges = dm->getEdgeArray(dm);
	int i, totedge = dm->getNumEdges(dm);
	
	data.reserve(totedge * 2);
	for (i = 0, me = medges; i < totedge; ++i, ++me) {
		data.push_back(me->v1);
		data.push_back(me->v2);
	}
	
	return OInt32ArrayProperty::sample_type(data);
}

void PointCacheWriter::write_sample()
{
	DerivedMesh *output_dm = m_pcmd->output_dm;
	if (!output_dm)
		return;
	
	OPolyMeshSchema &schema = m_mesh.getSchema();
	
	std::vector<V3f> positions_buffer;
	std::vector<int> indices_buffer;
	std::vector<int> counts_buffer;
	std::vector<bool_t> smooth_buffer;
	std::vector<int> edges_buffer;
//	std::vector<V2f> uvs;
//	V2fArraySample()
	
	// TODO decide how to handle vertex/face normals, in caching vs. export ...
//	std::vector<V2f> uvs;
//	OV2fGeomParam::Sample uvs(V2fArraySample(uvs), kFacevaryingScope );
	
	P3fArraySample positions = create_sample_positions(output_dm, positions_buffer);
	Int32ArraySample indices = create_sample_vertex_indices(output_dm, indices_buffer);
	Int32ArraySample counts = create_sample_loop_counts(output_dm, counts_buffer);
	OBoolGeomParam::Sample smooth = create_sample_poly_smooth(output_dm, smooth_buffer);
	OInt32ArrayProperty::sample_type edges = create_sample_edge_vertices(output_dm, edges_buffer);
	
	OPolyMeshSchema::Sample sample = OPolyMeshSchema::Sample(
	            positions,
	            indices,
	            counts,
	            OV2fGeomParam::Sample(), /* XXX define how/which UV map should be considered primary for the alembic schema */
	            ON3fGeomParam::Sample()
	            );
	schema.set(sample);
	
	m_param_smooth.set(smooth);
	
	m_prop_edges.set(edges);
}


PointCacheReader::PointCacheReader(Scene *scene, Object *ob, PointCacheModifierData *pcmd) :
    Reader(scene, &ob->id, pcmd->point_cache),
    m_ob(ob),
    m_pcmd(pcmd),
    m_result(NULL)
{
	set_error_handler(new ModifierErrorHandler(&pcmd->modifier));
	
	if (m_archive.valid()) {
		IObject root = m_archive.getTop();
		if (root.valid() && root.getChild(m_pcmd->modifier.name)) {
			m_mesh = IPolyMesh(root, m_pcmd->modifier.name);
			
			IPolyMeshSchema &schema = m_mesh.getSchema();
			ICompoundProperty geom_props = schema.getArbGeomParams();
			ICompoundProperty user_props = schema.getUserProperties();
			
			m_param_smooth = IBoolGeomParam(geom_props, "smooth", 0);
			m_prop_edges = IInt32ArrayProperty(user_props, "edges", 0);
		}
	}
}

PointCacheReader::~PointCacheReader()
{
}

static void apply_sample_positions(DerivedMesh *dm, P3fArraySamplePtr sample)
{
	MVert *mv, *mverts = dm->getVertArray(dm);
	int i, totvert = dm->getNumVerts(dm);
	
	const V3f *data = sample->get();
	for (i = 0, mv = mverts; i < totvert; ++i, ++mv) {
		const V3f &co = data[i];
		copy_v3_v3(mv->co, co.getValue());
	}
}

static void apply_sample_vertex_indices(DerivedMesh *dm, Int32ArraySamplePtr sample)
{
	MLoop *ml, *mloops = dm->getLoopArray(dm);
	int i, totloop = dm->getNumLoops(dm);
	
	BLI_assert(sample->size() == totloop);
	
	const int32_t *data = sample->get();
	for (i = 0, ml = mloops; i < totloop; ++i, ++ml) {
		ml->v = data[i];
	}
}

static void apply_sample_loop_counts(DerivedMesh *dm, Int32ArraySamplePtr sample)
{
	MPoly *mp, *mpolys = dm->getPolyArray(dm);
	int i, totpoly = dm->getNumPolys(dm);
	
	BLI_assert(sample->size() == totpoly);
	
	const int32_t *data = sample->get();
	int loopstart = 0;
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		mp->totloop = data[i];
		mp->loopstart = loopstart;
		
		loopstart += mp->totloop;
	}
}

static void apply_sample_poly_smooth(DerivedMesh *dm, BoolArraySamplePtr sample)
{
	MPoly *mp, *mpolys = dm->getPolyArray(dm);
	int i, totpoly = dm->getNumPolys(dm);
	
	BLI_assert(sample->size() == totpoly);
	
	const bool_t *data = sample->get();
	bool changed = false;
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		if (data[i]) {
			mp->flag |= ME_SMOOTH;
			changed = true;
		}
	}
	if (changed)
		dm->dirty = (DMDirtyFlag)((int)dm->dirty | DM_DIRTY_NORMALS);
}

static void apply_sample_edge_vertices(DerivedMesh *dm, Int32ArraySamplePtr sample)
{
	MEdge *me, *medges = dm->getEdgeArray(dm);
	int i, totedge = dm->getNumEdges(dm);
	
	BLI_assert(sample->size() == totedge * 2);
	
	const int32_t *data = sample->get();
	for (i = 0, me = medges; i < totedge; ++i, ++me) {
		me->v1 = data[(i << 1)];
		me->v2 = data[(i << 1) + 1];
	}
}

PTCReadSampleResult PointCacheReader::read_sample(float frame)
{
	/* discard existing result data */
	discard_result();
	
	if (!m_mesh.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	IPolyMeshSchema &schema = m_mesh.getSchema();
	if (!schema.valid() || schema.getPositionsProperty().getNumSamples() == 0)
		return PTC_READ_SAMPLE_INVALID;
	
	ISampleSelector ss = get_frame_sample_selector(frame);
	
	IPolyMeshSchema::Sample sample;
	schema.get(sample, ss);
	
	P3fArraySamplePtr positions = sample.getPositions();
	Int32ArraySamplePtr indices = sample.getFaceIndices();
	Int32ArraySamplePtr counts = sample.getFaceCounts();
	
	BoolArraySamplePtr smooth;
	if (m_param_smooth) {
		IBoolGeomParam::Sample sample_smooth;
		m_param_smooth.getExpanded(sample_smooth, ss);
		smooth = sample_smooth.getVals();
	}
	
	Int32ArraySamplePtr edges;
	if (m_prop_edges) {
		m_prop_edges.get(edges, ss);
		BLI_assert(edges->size() % 2 == 0); /* 2 vertex indices per edge */
	}
	
	int totverts = positions->size();
	int totloops = indices->size();
	int totpolys = counts->size();
	int totedges = edges->size() >> 1;
	m_result = CDDM_new(totverts, totedges, 0, totloops, totpolys);
	
	apply_sample_positions(m_result, positions);
	apply_sample_vertex_indices(m_result, indices);
	apply_sample_loop_counts(m_result, counts);
	apply_sample_edge_vertices(m_result, edges);
	if (smooth)
		apply_sample_poly_smooth(m_result, smooth);
	
	DM_ensure_normals(m_result);
	
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
