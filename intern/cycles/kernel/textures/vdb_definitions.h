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

#include "vdb_lookup.h"
#include <openvdb/tools/Interpolation.h>
//#include <OpenImageIO/ustring.h>
#include <OSL/oslconfig.h>
#include <vector.h>

CCL_NAMESPACE_BEGIN

using OpenImageIO::ustring;

typedef boost::shared_ptr<VDBAccessor> VDBAccessorPtr;
typedef vector<VDBAccessorPtr> AccessorVector;

typedef struct VDBVolumeFile {
	openvdb::io::File file;
    ustring version;
        
    AccessorVector accessors;
	openvdb::GridPtrVecPtr grids;
    openvdb::MetaMap::Ptr meta;
    
    VDBAccessorPtr getAccessor(ustring grid_name)
    {
        VDBAccessorPtr ptr;
        
        for (int i = 0; i < accessors.size(); i++)
        {
            if (accessors[i]->getGridPtr()->getName() == grid_name.string())
            {
                ptr = accessors[i];
                break;
            }
        }
        
        return ptr;
    }
    
    VDBAccessorPtr getAccessor()
    {
        return accessors[0];
    }
    
    VDBVolumeFile(ustring filename) : file(filename.string())
    {
        file.open();
        std::cout << "Opening file: " << filename.string() << std::endl;
        grids = file.getGrids();
        for(int i = 0; i < grids->size(); i++)
        {
            VDBAccessorPtr ptr(new VDBAccessor((*grids)[i]));
            accessors.push_back(ptr);
        }
            
        meta = file.getMetadata();
        version = file.version();
        //std::cout << "Opened file. AccessorVector has " << accessors.size() << " elements." << std::endl;
        file.close();
    }
    ~VDBVolumeFile()
    {
        accessors.clear();
    }
    
    
    
} VDBVolumeFile;

CCL_NAMESPACE_END

#endif /* __VDB_DEFINITIONS_H__ */
