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

Writer::Writer(const std::string &filename)
{
	ensure_directory(filename.c_str());
	m_archive = OArchive(AbcCoreHDF5::WriteArchive(), filename, ErrorHandler::kThrowPolicy);
}

Writer::~Writer()
{
}

uint32_t Writer::add_frame_sampling(Scene *scene)
{
	if (scene->r.frs_sec == 0.0f) {
		/* Should never happen, just to be safe
		 * Index 0 is the default time sampling with a step of 1.0
		 */
		return 0;
	}
	
	chrono_t cycle_time = (double)scene->r.frs_sec_base / (double)scene->r.frs_sec;
	chrono_t start_time = 0.0f;
	return m_archive.addTimeSampling(TimeSampling(cycle_time, start_time));
}

} /* namespace PTC */
