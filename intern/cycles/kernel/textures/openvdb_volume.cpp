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
#include <openvdb/openvdb.h>

CCL_NAMESPACE_BEGIN

typedef struct VDBVolumeFile {
	openvdb::io::File file;
    ustring version;
    
	openvdb::GridPtrVecPtr grids;
    openvdb::MetaMap::Ptr meta;
    
    VDBVolumeFile(ustring filename) : file(filename.string())
    {
        file.open();
        
        grids = file.getGrids();
        meta = file.getMetadata();
        version = file.version();
    }
    ~VDBVolumeFile()
    {
        file.close();
    }
} VDBVolumeFile;

class OpenVDBUtil
{
public:
    static ustring u_openvdb_file_extension;
    
	static void initialize_library();
	static bool open_file(OIIO::ustring filename, VDBVolumeFile *vdb_volume);
	static bool is_vdb_volume_file(OIIO::ustring filename);
    static VDBVolumeFile *get_volume_from_file(OIIO::ustring filename);
    static int get_number_of_grids(VDBVolumeFile *vdb_volume);
    
private:
    static bool vdb_file_check_extension(ustring filename);
    static bool vdb_file_check_valid_header(ustring filename);
};


/* static ustrings */
ustring OpenVDBUtil::u_openvdb_file_extension(".vdb");

class OpenVDBVolumeAccessor {
public:
private:
	openvdb::GridBase::Ptr grid;
	eOpenVDBGridType grid_type; // This is needed because it's impossible to know the grid value type beforehand;
};


void OpenVDBUtil::initialize_library()
{
    //any additional configuration needed?
	openvdb::initialize();
}

bool OpenVDBUtil::vdb_file_check_valid_header(ustring filename)
{
    OpenVDBUtil::initialize_library();
    openvdb::io::File file(filename.string());
    file.open();
    
    OIIO::ustring openvdb_version;
    
    openvdb_version.empty();
    openvdb_version = file.version();
    
    file.close();
    
    if(openvdb_version.length() > 0)
        return true;
    
	return false;
}

bool OpenVDBUtil::vdb_file_check_extension(ustring filename)
{
    if (filename.substr(filename.length() - u_openvdb_file_extension.length(),
                        u_openvdb_file_extension.length()) == u_openvdb_file_extension)
		return true;
    
    return false;
}


bool OpenVDBUtil::is_vdb_volume_file(OIIO::ustring filename)
{
    OIIO::ustring openvdb_version;
    
    if (vdb_file_check_extension(filename)) {
        if (vdb_file_check_valid_header(filename))
            return true;
    }
    
    return false;
}

VDBVolumeFile *OpenVDBUtil::get_volume_from_file(ustring filename)
{
    VDBVolumeFile vdb_volume(filename);
    return &vdb_volume;
}

int OpenVDBUtil::get_number_of_grids(VDBVolumeFile *vdb_volume)
{
    return vdb_volume->grids->size();
}


// VDBTextureSystem
VDBTextureSystem *VDBTextureSystem::init() {
    OpenVDBUtil::initialize_library();
    VDBTextureSystem vdb_ts;
    
    return &vdb_ts;
}

bool VDBTextureSystem::is_vdb_volume(ustring filename)
{
    if (vdb_files.find(filename) != vdb_files.end())
        return true;
    else
        return OpenVDBUtil::is_vdb_volume_file(filename);
}

bool VDBTextureSystem::perform_lookup(ustring filename, TextureOpt &options, OSL::ShaderGlobals *sg,
                                      const Imath::V3f &P, const Imath::V3f &dPdx,
                                      const Imath::V3f &dPdy, const Imath::V3f &dPdz,
                                      float *result)
{
    // - check if file is open;
    OpenVDBMap::const_iterator open_file = vdb_files.find(filename);
    
    if (open_file == vdb_files.end())
    {
        VDBVolumeFile file(filename);
        
        // add it to map
        open_file = (vdb_files.insert(std::make_pair(filename, &file))).first;        
    }

    VDBVolumeFile *myVDB = open_file->second;
    openvdb::GridPtrVecIter iter = myVDB->grids->begin();
    
    if (OpenVDBUtil::get_number_of_grids(myVDB) == 1) {
       
       // Name of the grid will be unimportant if it's the only one present in the file.
    }
    else
    {
        for(iter = myVDB->grids->begin(); iter != myVDB->grids->end(); ++iter)
        {
            openvdb::GridBase::Ptr grid = *iter;
            // Traversal is needed to find the correct grid by name;
        }
    }
    
    // perform lookup.
    
    return false;
}

CCL_NAMESPACE_END
