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

CCL_NAMESPACE_BEGIN

VDBTextureSystem::Ptr VDBTextureSystem::init() {
    OpenVDBUtil::initialize_library();
    Ptr vdb_ts(new VDBTextureSystem());
    
    return vdb_ts;
}

bool VDBTextureSystem::valid_vdb_file(ustring filename)
{
        return OpenVDBUtil::is_vdb_volume_file(filename);
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
    
      /*
        float x, y, z = 0;
        x = OpenVDBUtil::nearest_neighbor(P[0]);
        y = OpenVDBUtil::nearest_neighbor(P[1]);
        z = OpenVDBUtil::nearest_neighbor(P[2]);
        
       */
        //accessor->vdb_lookup_single_point(x, y, z, result);
        
        openvdb::tools::GridSampler<openvdb::FloatTree, openvdb::tools::BoxSampler>
        sampler(openvdb::gridPtrCast<openvdb::FloatGrid>(accessor->getGridPtr())->constTree(), accessor->getGridPtr()->transform());
        openvdb::Vec3d p(P[0], P[1], P[2]);
        *result = sampler.wsSample(p);
      //  VDBLookup::vdb_lookup_single_point(grid, x, y, z, result);
        
     //   *result = VDBLookup<openvdb::Int32Grid>::vdb_lookup_single_point(*intGrid, (int)x, (int)y, (int)z);
        
        return true;
    }
}

VDBTextureSystem::VDBMap::const_iterator VDBTextureSystem::add_vdb_to_map(ustring filename)
{
    VDBFilePtr vdb_sp(new VDBVolumeFile(filename));
    
    return (vdb_files.insert(std::make_pair(filename, vdb_sp))).first;
}

void VDBTextureSystem::free(VDBTextureSystem::Ptr &vdb_ts)
{
    vdb_ts.reset(); 
}

CCL_NAMESPACE_END
