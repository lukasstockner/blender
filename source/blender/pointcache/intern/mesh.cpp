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
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_DerivedMesh.h"
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
//	m_points = OPoints(root, m_psys->name, fs);
}

MeshCacheWriter::~MeshCacheWriter()
{
}

void MeshCacheWriter::write_sample()
{
}


MeshCacheReader::MeshCacheReader(Scene *scene, Object *ob, MeshCacheModifierData *mcmd) :
    Reader(scene, &ob->id, mcmd->point_cache),
    m_ob(ob),
    m_mcmd(mcmd)
{
	if (m_archive.valid()) {
		IObject root = m_archive.getTop();
//		m_points = IPoints(root, m_psys->name);
	}
}

MeshCacheReader::~MeshCacheReader()
{
}

PTCReadSampleResult MeshCacheReader::read_sample(float frame)
{
	return PTC_READ_SAMPLE_INVALID;
}

} /* namespace PTC */
