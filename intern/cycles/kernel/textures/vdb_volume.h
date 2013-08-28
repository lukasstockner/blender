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

#ifndef __VDB_VOLUME_H__
#define __VDB_VOLUME_H__

#include <OSL/oslexec.h>
#include "util_map.h"

using OpenImageIO::ustring;

CCL_NAMESPACE_BEGIN

struct VDBVolumeFile;
class OpenVDBVolumeAccessor;
class OpenVDBUtil;

class VDBTextureSystem {
public:
    typedef boost::shared_ptr<VDBVolumeFile> VDBFilePtr;
    typedef unordered_map<ustring, VDBFilePtr, OIIO::ustringHash> VDBMap;
    typedef boost::shared_ptr<VDBTextureSystem> Ptr;
    
    static VDBTextureSystem::Ptr init();
    static void free (VDBTextureSystem::Ptr &vdb_ts);
    
    static bool valid_vdb_file (ustring filename);
    
    bool perform_lookup (ustring filename, OIIO::TextureSystem::Perthread *thread_info,
                         OIIO::TextureOpt &options, const Imath::V3f &P, const Imath::V3f &dPdx,
                    const Imath::V3f &dPdy, const Imath::V3f &dPdz,
                    float *result);
  
    VDBTextureSystem() { }
    ~VDBTextureSystem() { }
    
private:
    VDBMap vdb_files;
    VDBMap::const_iterator add_vdb_to_map(ustring filename);
};

CCL_NAMESPACE_END

#endif /* __VDB_VOLUME_H__ */