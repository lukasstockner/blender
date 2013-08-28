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

#ifndef __VDB_DEFINITIONS_H__
#define __VDB_DEFINITIONS_H__

#include <openvdb/openvdb.h>
#include <OpenImageIO/ustring.h>

CCL_NAMESPACE_BEGIN

using OpenImageIO::ustring;

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

CCL_NAMESPACE_END

#endif /* __VDB_DEFINITIONS_H__ */
