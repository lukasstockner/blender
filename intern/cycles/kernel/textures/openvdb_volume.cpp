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

#include "openvdb_volume.h"

CCL_NAMESPACE_BEGIN

/* static ustrings */
ustring OpenVDBUtil::u_openvdb_file_extension(".vdb");

void OpenVDBUtil::initialize_library()
{ //any additional configuration needed?
	openvdb::initialize();
}

bool OpenVDBUtil::open_file(OIIO::ustring filename, OpenVDBVolume &vdb_volume)
{
	openvdb::io::File file(filename.string());

	file.open();
	openvdb::GridPtrVecPtr grids = file.getGrids();
	
	file.close();

	// Build OpenVDBVolume
	//OpenVDBVolume vdb_volume(file, grids);

	//return vdb_volume;
	return true;
}

bool OpenVDBUtil::is_openvdb_volume_file(OIIO::ustring filename)
{


	if (filename.substr(filename.length() - u_openvdb_file_extension.length(), 
		u_openvdb_file_extension.length()) == u_openvdb_file_extension)
		return true;
	else
		return false;
}

bool OpenVDBUtil::is_openvdb_volume_file(OIIO::ustring filename, ustring &openvdb_version)
{
	if (is_openvdb_volume_file(filename))
	{
		openvdb::io::File file(filename.string());
		file.open();

		openvdb_version.empty();
		openvdb_version = file.version();

		if(openvdb_version.length() > 0)
			return true;
	}

	return false;
}


CCL_NAMESPACE_END