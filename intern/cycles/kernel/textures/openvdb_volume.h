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

#ifndef __OPENVDB_VOLUME_H__
#define __OPENVDB_VOLUME_H__

#include <openvdb/openvdb.h>
#include <OSL/oslexec.h>

CCL_NAMESPACE_BEGIN

using namespace OIIO;

typedef enum eOpenVDBGridType
{
	OPENVDB_GRID_TYPE_NOT_SET,
	OPENVDB_GRID_TYPE_FLOAT,
	OPENVDB_GRID_TYPE_INT32,
	OPENVDB_GRID_TYPE_INT64,
	OPENVDB_GRID_TYPE_VEC3F
} eOpenVDBGridType;

typedef struct OpenVDBVolume { //increasingly, it seems OpenVDBVolume should actually be OpenVDBVolumeCollection; checking with Brecht.
	//TODO: wip: handles for file, sampling mechanism and pointer to grid(s): each file might contain more than 1 grid;
	openvdb::io::File file;
	openvdb::GridPtrVecPtr grids;

	
} OpenVDBVolume;

class OpenVDBVolumeAccessor {
public:
private:
	openvdb::GridBase::Ptr grid;
	eOpenVDBGridType grid_type; // Is this really necessary? Check OpenVDB's generic programming guidelines.
	
};

class OpenVDBUtil
{
public:
	static void initialize_library();
	static bool open_file(OIIO::ustring filename, OpenVDBVolume &vdb_volume);
	

    static bool is_vdb_volume_file(OIIO::ustring filename);

	static ustring u_openvdb_file_extension;
private:
    static bool vdb_file_check_extension(ustring filename);
    static bool vdb_file_check_valid_header(ustring filename);
	int i;
};



CCL_NAMESPACE_END

#endif /* __OPENVDB_VOLUME_H__ */