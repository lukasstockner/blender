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

#include "smoke.h"

extern "C" {
#include "DNA_object_types.h"
#include "DNA_smoke_types.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

SmokeWriter::SmokeWriter(Scene *scene, Object *ob, SmokeDomainSettings *domain) :
    Writer(scene, &ob->id, domain->point_cache[0]),
    m_ob(ob),
    m_domain(domain)
{
	uint32_t fs = add_frame_sampling();
	
	OObject root = m_archive.getTop();
//	m_points = OPoints(root, m_psys->name, fs);
}

SmokeWriter::~SmokeWriter()
{
}

void SmokeWriter::write_sample()
{
}


SmokeReader::SmokeReader(Scene *scene, Object *ob, SmokeDomainSettings *domain) :
    Reader(scene, &ob->id, domain->point_cache[0]),
    m_ob(ob),
    m_domain(domain)
{
	if (m_archive.valid()) {
		IObject root = m_archive.getTop();
//		m_points = IPoints(root, m_psys->name);
	}
}

SmokeReader::~SmokeReader()
{
}

PTCReadSampleResult SmokeReader::read_sample(float frame)
{
	return PTC_READ_SAMPLE_INVALID;
}

} /* namespace PTC */
