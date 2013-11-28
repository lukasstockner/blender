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

#include <Alembic/AbcCoreHDF5/ReadWrite.h>
#include <Alembic/Abc/ArchiveInfo.h>

#include "reader.h"

extern "C" {
#include "DNA_scene_types.h"
}

namespace PTC {

using namespace Abc;

Reader::Reader(const std::string &filename, Scene *scene) :
    m_scene(scene)
{
	m_archive = IArchive(AbcCoreHDF5::ReadArchive(), filename, ErrorHandler::kThrowPolicy);
}

Reader::~Reader()
{
}

void Reader::get_frame_range(int &start_frame, int &end_frame)
{
	if (m_scene->r.frs_sec_base == 0.0f) {
		/* Should never happen, just to be safe */
		start_frame = end_frame = 1;
		return;
	}
	
	double start_time, end_time;
	GetArchiveStartAndEndTime(m_archive, start_time, end_time);
	
	double fps = (double)m_scene->r.frs_sec / (double)m_scene->r.frs_sec_base;
	start_frame = start_time * fps;
	end_frame = end_time * fps;
}

//AbcA::TimeSamplingPtr ts = iArchive.getTimeSampling( i )

} /* namespace PTC */
