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

#include "abc_cloth.h"

extern "C" {
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
}

#include "PTC_api.h"

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcClothWriter::AbcClothWriter(Scene *scene, Object *ob, ClothModifierData *clmd) :
    ClothWriter(scene, ob, clmd, &m_archive),
    m_archive(scene, &ob->id, clmd->point_cache, m_error_handler)
{
}

AbcClothWriter::~AbcClothWriter()
{
}

void AbcClothWriter::write_sample()
{
}


AbcClothReader::AbcClothReader(Scene *scene, Object *ob, ClothModifierData *clmd) :
    ClothReader(scene, ob, clmd, &m_archive),
    m_archive(scene, &ob->id, clmd->point_cache, m_error_handler)
{
	if (m_archive.archive.valid()) {
		IObject root = m_archive.archive.getTop();
//		m_points = IPoints(root, m_psys->name);
	}
}

AbcClothReader::~AbcClothReader()
{
}

PTCReadSampleResult AbcClothReader::read_sample(float frame)
{
	return PTC_READ_SAMPLE_INVALID;
}

/* ==== API ==== */

Writer *abc_writer_cloth(Scene *scene, Object *ob, ClothModifierData *clmd)
{
	return new AbcClothWriter(scene, ob, clmd);
}

Reader *abc_reader_cloth(Scene *scene, Object *ob, ClothModifierData *clmd)
{
	return new AbcClothReader(scene, ob, clmd);
}

} /* namespace PTC */
