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

#include "vdb_definitions.h"
#include "vdb_util.h"
#include "vdb_lookup.h"
#include "vdb_volume.h"
#include <string.h>

CCL_NAMESPACE_BEGIN

VDBTextureSystem::Ptr VDBTextureSystem::init() {
    OpenVDBUtil::initialize_library();
    Ptr vdb_ts(new VDBTextureSystem());
    std::cout << "Initialized VDBTextureSystem" << std::endl;
    return vdb_ts;
}

bool VDBTextureSystem::valid_vdb_file(ustring filename)
{
    //This opens the file, much too slow!
    //return OpenVDBUtil::is_vdb_volume_file(filename);

    size_t length = filename.length();

    if(length < 4)
        return false;

    return strcmp(filename.data() + length - 4, ".vdb") == 0;
}

bool VDBTextureSystem::perform_lookup(ustring filename, OIIO::TextureSystem::Perthread *thread_info,
                                      OSL::TextureOpt &options, const Imath::V3f &P,
                                      const Imath::V3f &dPdx,const Imath::V3f &dPdy,
                                      const Imath::V3f &dPdz, float *result)
{
    // check if file is open;
    VDBMap::const_iterator open_file = vdb_files.find(filename);
    
    if (open_file == vdb_files.end())
    {
        open_file = add_vdb_to_map(filename);
        
        OpenVDBUtil::print_info(*open_file->second);
    }

    VDBFilePtr vdb_p = open_file->second;
    
    if (!vdb_p) {
        return false;
    }
    else
    {
        VDBAccessorPtr accessor;
        if (options.subimagename) {
            accessor = vdb_p->getAccessor(options.subimagename);
        }
        else
        {
            accessor = vdb_p->getAccessor();
        }
        
        if (!accessor) return false;
        
        for (int i = 0; i < options.nchannels; i++)
            result[i] = 0;
        
        accessor->vdb_lookup_single_point(P[0], P[1], P[2], result);
        
        return true;
    }
}

VDBTextureSystem::VDBMap::const_iterator VDBTextureSystem::add_vdb_to_map(const ustring &filename)
{
    VDBFilePtr vdb_sp(new VDBVolumeFile(filename));
    return (vdb_files.insert(std::make_pair(filename, vdb_sp))).first;
}

void VDBTextureSystem::free(VDBTextureSystem::Ptr &vdb_ts)
{
    vdb_ts.reset(); 
}

CCL_NAMESPACE_END

