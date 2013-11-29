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

#ifndef PTC_READER_H
#define PTC_READER_H

#include <string>

#include <Alembic/Abc/IArchive.h>

#include "util/util_frame_mapper.h"

struct Scene;

namespace PTC {

using namespace Alembic;

class Reader : public FrameMapper {
public:
	Reader(const std::string &filename, Scene *scene);
	virtual ~Reader();
	
	void get_frame_range(int &start_frame, int &end_frame);
	
	virtual void read_sample() = 0;
	
protected:
	Abc::IArchive m_archive;
	
	Scene *m_scene;
};

} /* namespace PTC */

#endif  /* PTC_READER_H */
