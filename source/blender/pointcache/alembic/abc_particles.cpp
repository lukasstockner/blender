/*
 * Copyright 2013, Blender Foundation.
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

#include "alembic.h"

#include "abc_particles.h"

extern "C" {
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
}

#include "PTC_api.h"

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcParticlesWriter::AbcParticlesWriter(Scene *scene, Object *ob, ParticleSystem *psys) :
    ParticlesWriter(scene, ob, psys, &m_archive),
    m_archive(scene, &ob->id, psys->pointcache, m_error_handler)
{
	uint32_t fs = m_archive.add_frame_sampling();
	
	OObject root = m_archive.archive.getTop();
	m_points = OPoints(root, m_psys->name, fs);
}

AbcParticlesWriter::~AbcParticlesWriter()
{
}

void AbcParticlesWriter::write_sample()
{
	OPointsSchema &schema = m_points.getSchema();
	
	int totpart = m_psys->totpart;
	ParticleData *pa;
	int i;
	
	/* XXX TODO only needed for the first frame/sample */
	std::vector<Util::uint64_t> ids;
	ids.reserve(totpart);
	for (i = 0, pa = m_psys->particles; i < totpart; ++i, ++pa)
		ids.push_back(i);
	
	std::vector<V3f> positions;
	positions.reserve(totpart);
	for (i = 0, pa = m_psys->particles; i < totpart; ++i, ++pa) {
		float *co = pa->state.co;
		positions.push_back(V3f(co[0], co[1], co[2]));
	}
	
	OPointsSchema::Sample sample = OPointsSchema::Sample(V3fArraySample(positions), UInt64ArraySample(ids));

	schema.set(sample);
}


AbcParticlesReader::AbcParticlesReader(Scene *scene, Object *ob, ParticleSystem *psys) :
    ParticlesReader(scene, ob, psys, &m_archive),
    m_archive(scene, &ob->id, psys->pointcache, m_error_handler)
{
	if (m_archive.archive.valid()) {
		IObject root = m_archive.archive.getTop();
		m_points = IPoints(root, m_psys->name);
	}
	
	/* XXX TODO read first sample for info on particle count and times */
	m_totpoint = 0;
}

AbcParticlesReader::~AbcParticlesReader()
{
}

PTCReadSampleResult AbcParticlesReader::read_sample(float frame)
{
	if (!m_points.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	IPointsSchema &schema = m_points.getSchema();
	TimeSamplingPtr ts = schema.getTimeSampling();
	
	ISampleSelector ss = m_archive.get_frame_sample_selector(frame);
	chrono_t time = ss.getRequestedTime();
	
	std::pair<index_t, chrono_t> sres = ts->getFloorIndex(time, schema.getNumSamples());
	chrono_t stime = sres.second;
	float sframe = m_archive.time_to_frame(stime);
	
	IPointsSchema::Sample sample;
	schema.get(sample, ss);
	
	const V3f *positions = sample.getPositions()->get();
	int totpart = m_psys->totpart, i;
	ParticleData *pa;
	for (i = 0, pa = m_psys->particles; i < sample.getPositions()->size(); ++i, ++pa) {
		pa->state.co[0] = positions[i].x;
		pa->state.co[1] = positions[i].y;
		pa->state.co[2] = positions[i].z;
	}
	
	return PTC_READ_SAMPLE_EXACT;
}

/* ==== API ==== */

Writer *abc_writer_particles(Scene *scene, Object *ob, ParticleSystem *psys)
{
	return new AbcParticlesWriter(scene, ob, psys);
}

Reader *abc_reader_particles(Scene *scene, Object *ob, ParticleSystem *psys)
{
	return new AbcParticlesReader(scene, ob, psys);
}

} /* namespace PTC */
