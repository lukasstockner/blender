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

#include <OSL/oslexec.h>
#include "util_map.h"


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

struct VDBVolumeFile;
class OpenVDBVolumeAccessor;
class OpenVDBUtil;

class VDBTextureSystem {
public:
    static VDBTextureSystem *init();
    static void terminate (VDBTextureSystem *vdb_ts);
    
    VDBTextureSystem() { }
    ~VDBTextureSystem() { }
    
    typedef unordered_map<ustring, VDBVolumeFile*, ustringHash> OpenVDBMap;
    
    bool is_vdb_volume (ustring filename);
    
    bool perform_lookup (ustring filename, TextureOpt &options, OSL::ShaderGlobals *sg,
                    const Imath::V3f &P, const Imath::V3f &dPdx,
                    const Imath::V3f &dPdy, const Imath::V3f &dPdz,
                    float *result);
    
private:
    OpenVDBMap vdb_files;
};


CCL_NAMESPACE_END

#endif /* __OPENVDB_VOLUME_H__ */