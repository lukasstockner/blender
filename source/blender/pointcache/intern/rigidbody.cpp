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

#include "rigidbody.h"

extern "C" {
#include "DNA_scene_types.h"
#include "DNA_rigidbody_types.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

RigidBodyWriter::RigidBodyWriter(Scene *scene, RigidBodyWorld *rbw) :
    Writer(scene, &scene->id, rbw->pointcache),
    m_rbw(rbw)
{
	uint32_t fs = add_frame_sampling();
	
	OObject root = m_archive.getTop();
//	m_points = OPoints(root, m_psys->name, fs);
}

RigidBodyWriter::~RigidBodyWriter()
{
}

void RigidBodyWriter::write_sample()
{
}


RigidBodyReader::RigidBodyReader(Scene *scene, RigidBodyWorld *rbw) :
    Reader(scene, &scene->id, rbw->pointcache),
    m_rbw(rbw)
{
	if (m_archive.valid()) {
		IObject root = m_archive.getTop();
//		m_points = IPoints(root, m_psys->name);
	}
}

RigidBodyReader::~RigidBodyReader()
{
}

PTCReadSampleResult RigidBodyReader::read_sample(float frame)
{
	return PTC_READ_SAMPLE_INVALID;
}

} /* namespace PTC */
