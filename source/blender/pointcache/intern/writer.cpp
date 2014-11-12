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

#include "writer.h"
#include "util_path.h"

extern "C" {
#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "DNA_scene_types.h"
}

namespace PTC {

using namespace Abc;

/* make sure the file's directory exists */
static void ensure_directory(const char *filename)
{
	char dir[FILE_MAXDIR];
	BLI_split_dir_part(filename, dir, sizeof(dir));
	BLI_dir_create_recursive(dir);
}

Writer::Writer(Scene *scene, ID *id, PointCache *cache) :
    FrameMapper(scene),
    m_error_handler(0),
    m_scene(scene)
{
	std::string filename = ptc_archive_path(cache, id);
	ensure_directory(filename.c_str());
	PTC_SAFE_CALL_BEGIN
	m_archive = OArchive(AbcCoreHDF5::WriteArchive(), filename, Abc::ErrorHandler::kThrowPolicy);
	PTC_SAFE_CALL_END_HANDLER(m_error_handler)
}

Writer::~Writer()
{
	if (m_error_handler)
		delete m_error_handler;
}

void Writer::set_error_handler(ErrorHandler *handler)
{
	if (m_error_handler)
		delete m_error_handler;
	
	m_error_handler = handler;
}

bool Writer::valid() const
{
	return m_error_handler ? m_error_handler->max_error_level() >= PTC_ERROR_CRITICAL : true;
}

uint32_t Writer::add_frame_sampling()
{
	chrono_t cycle_time = seconds_per_frame();
	chrono_t start_time = 0.0f;
	return m_archive.addTimeSampling(TimeSampling(cycle_time, start_time));
}

} /* namespace PTC */
