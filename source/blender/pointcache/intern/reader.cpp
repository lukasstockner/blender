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
#include "util_path.h"
#include "util_error_handler.h"

extern "C" {
#include "BLI_fileops.h"

#include "DNA_scene_types.h"
}

namespace PTC {

using namespace Abc;

Reader::Reader(Scene *scene, ID *id, PointCache *cache) :
    FrameMapper(scene),
    m_scene(scene)
{
	std::string filename = ptc_archive_path(cache, id);
	PTC_SAFE_CALL_BEGIN
	m_archive = IArchive(AbcCoreHDF5::ReadArchive(), filename, Abc::ErrorHandler::kThrowPolicy);
	PTC_SAFE_CALL_END
}

Reader::~Reader()
{
}

void Reader::get_frame_range(int &start_frame, int &end_frame)
{
	if (m_archive.valid()) {
		double start_time, end_time;
		GetArchiveStartAndEndTime(m_archive, start_time, end_time);
		start_frame = (int)time_to_frame(start_time);
		end_frame = (int)time_to_frame(end_time);
	}
	else {
		start_frame = end_frame = 1;
	}
}

ISampleSelector Reader::get_frame_sample_selector(float frame)
{
	return ISampleSelector(frame_to_time(frame), ISampleSelector::kFloorIndex);
}

PTCReadSampleResult Reader::test_sample(float frame)
{
	if (m_archive.valid()) {
		double start_time, end_time;
		GetArchiveStartAndEndTime(m_archive, start_time, end_time);
		float start_frame = time_to_frame(start_time);
		float end_frame = time_to_frame(end_time);
		
		if (frame < start_frame)
			return PTC_READ_SAMPLE_EARLY;
		else if (frame > end_frame)
			return PTC_READ_SAMPLE_LATE;
		else {
			/* TODO could also be EXACT, but INTERPOLATED is more general
			 * do we need to support this?
			 * checking individual time samplings is also possible, but more involved.
			 */
			return PTC_READ_SAMPLE_INTERPOLATED;
		}
	}
	else {
		return PTC_READ_SAMPLE_INVALID;
	}
}

} /* namespace PTC */
