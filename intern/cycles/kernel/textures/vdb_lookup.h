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

#ifndef __VDB_LOOKUP_H__
#define __VDB_LOOKUP_H__

#include <openvdb/openvdb.h>

template <typename GridType>
class VDBLookup {
public:
    static typename GridType::ValueType vdb_lookup_single_point(const GridType &grid, int i, int j, int k);
    static typename GridType::ValueType* vdb_lookup_multiple_points(const GridType &grid, int i[], int j[], int k[], int num);
};

template <typename GridType>
typename GridType::ValueType VDBLookup<GridType>::vdb_lookup_single_point(const GridType &grid, int i, int j, int k)
{
    typename GridType::ValueType result;
    openvdb::math::Coord xyz(i, j, k);
    
    result = grid.tree().getValue(xyz);
    
    return result;
}

template <typename GridType>
typename GridType::ValueType* VDBLookup<GridType>::vdb_lookup_multiple_points(const GridType &grid, int i[], int j[], int k[], int num)
{
    typename GridType::ValueType result;
    openvdb::math::Coord xyz;
    
    typename GridType::ConstAccessor accessor = grid.getAccessor();
    GridType::ValueType *result = new typename GridType::ValueType[num];
    
    for (int pos = 0; pos < num; pos++) {
        
        xyz = openvdb::math::Coord(i[pos], j[pos], k[pos]);
        result[pos] = accessor.getValue(xyz);
    }
    
    return result;
}


#endif /* __VDB_LOOKUP_H__ */
