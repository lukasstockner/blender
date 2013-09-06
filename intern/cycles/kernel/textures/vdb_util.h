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

#ifndef __VDB_UTIL_H__
#define __VDB_UTIL_H__

#include "vdb_definitions.h"

CCL_NAMESPACE_BEGIN

class OpenVDBUtil
{
public:
    static ustring u_openvdb_file_extension;
    
	static void initialize_library();
	static bool open_file(OIIO::ustring filename, VDBVolumeFile *vdb_volume);
	static bool is_vdb_volume_file(OIIO::ustring filename);
    static VDBVolumeFile *get_volume_from_file(OIIO::ustring filename);
    static int get_number_of_grids(VDBVolumeFile &vdb_volume);
    static int nearest_neighbor(float worldCoord);
    static void print_info(VDBVolumeFile &vdb_file);
    
private:
    static bool vdb_file_format(ustring filename);
    static bool valid_file(ustring filename);
};


/* static ustrings */
ustring OpenVDBUtil::u_openvdb_file_extension(".vdb");


void OpenVDBUtil::initialize_library()
{
	openvdb::initialize(); std::cout << "initialized lib." << std::endl;
}

// Open file to check header information and determine
// if file is a valid VDB volume.
bool OpenVDBUtil::valid_file(ustring filename)
{
    OIIO::ustring openvdb_version;
    openvdb::io::File file(filename.string());
    
    try {
        file.open();
        
        openvdb_version.empty();
        openvdb_version = file.version();
        
        file.close();
        
    } catch (openvdb::IoError error) {
        std::cout << "ERROR: VDB file could not be opened (" << filename << ")." << std::endl;
        return false;
    }
    
    if(openvdb_version.length() > 0)
        return true;
    
	return false;
}

bool OpenVDBUtil::vdb_file_format(ustring filename)
{
    if (filename.substr(filename.length() - u_openvdb_file_extension.length(),
                        u_openvdb_file_extension.length()) == u_openvdb_file_extension)
		return true;
    
    return false;
}


bool OpenVDBUtil::is_vdb_volume_file(OIIO::ustring filename)
{
    if (vdb_file_format(filename)) {
        if (valid_file(filename))
            return true;
    }
    
    return false;
}


int OpenVDBUtil::get_number_of_grids(VDBVolumeFile &vdb_volume)
{
    return vdb_volume.grids->size();
}

int OpenVDBUtil::nearest_neighbor(float worldCoord)
{
    int x = static_cast<int>(floor(worldCoord + 0.5));
    return x;
}

std::string metaToString(
                 const openvdb::MetaMap::ConstMetaIterator& begin,
                 const openvdb::MetaMap::ConstMetaIterator& end,
                 const std::string& indent = "\n")
{
    std::ostringstream ostr;
    char sep[2] = { 0, 0 };
    for (openvdb::MetaMap::ConstMetaIterator it = begin; it != end; ++it)
    {
        ostr << sep << indent << it->first;
        if (it->second) {
            const std::string value = it->second->str();
            if (!value.empty()) ostr << ": " << value;
        }
        sep[0] = '\n';
    }
    return ostr.str();
}

void OpenVDBUtil::print_info(VDBVolumeFile &vdb_file)
{
    std::string str;
    
    std::cout << "VDB file loaded. Volume details: " << std::endl;
    
    // Output basic metadata information.
    if (vdb_file.meta) {
        str = metaToString(vdb_file.meta->beginMeta(), vdb_file.meta->endMeta());
        if (!str.empty()) std::cout << str << "\n";
    }
    
    // Iterate over all grids, and output name and value type.
    for (openvdb::GridPtrVec::const_iterator it = vdb_file.grids->begin(); it != vdb_file.grids->end(); ++it) {
        const openvdb::GridBase::ConstPtr grid = *it;
        if (!grid) continue;
        
        std::cout << "Grid name: " << grid->getName() << std::endl
        << "Grid value type: " <<  grid->valueType() << std::endl;
    
        // Print custom metadata, if any.
        if (vdb_file.meta) {
            str = metaToString(grid->beginMeta(), grid->endMeta(), " ");
            if (!str.empty()) std::cout << str << "\n";
            std::cout << std::flush;
        }
    }
}

CCL_NAMESPACE_END

#endif /* __VDB_UTIL_H__ */
