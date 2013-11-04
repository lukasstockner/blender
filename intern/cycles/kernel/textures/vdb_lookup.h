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
#include <openvdb/tools/Interpolation.h> // for sampling mechanisms

CCL_NAMESPACE_BEGIN

typedef enum VDB_GridType
{
    VDB_GRID_NOT_SET,
    VDB_GRID_BOOL,
    VDB_GRID_FLOAT,
    VDB_GRID_DOUBLE,
    VDB_GRID_INT32,
    VDB_GRID_INT64,
    VDB_GRID_VEC3I,
    VDB_GRID_VEC3F,
    VDB_GRID_VEC3D
} VDB_GridType;


class VDBAccessor {
public:
    VDBAccessor(openvdb::GridBase::Ptr grid);
    VDBAccessor() { }
    ~VDBAccessor() { }
    
    //getGrid
    openvdb::GridBase::Ptr getGridPtr();
    
    void vdb_lookup_single_point(float x, float y, float z, float *result);
    void vdb_lookup_multiple_points(float x[], float y[], float z[], float *result);

    
private:
    openvdb::GridBase::Ptr m_grid;
    VDB_GridType m_type;

    template <typename GridType>
    void vdb_lookup_nearest_neighbor(float x, float y, float z, float *result);
    
    template <typename GridType>
    void vdb_lookup_trilinear(float x, float y, float z, float *result);
    
    template <typename GridType>
    typename GridType::ValueType vdb_lookup_single_point(float x, float y, float z);
    
    template <typename GridType>
    typename GridType::ValueType* vdb_lookup_multiple_points(float x[], float y[], float z[], int num);
    
    template <typename GridType>
    typename GridType::TreeType::template ValueAccessor<typename GridType::TreeType> get_value_accessor();
};

VDBAccessor::VDBAccessor(openvdb::GridBase::Ptr grid)
{
    if (grid->isType<openvdb::BoolGrid>())
        m_type = VDB_GRID_BOOL;
    else if (grid->isType<openvdb::FloatGrid>())
        m_type = VDB_GRID_FLOAT;
    else if (grid->isType<openvdb::DoubleGrid>())
        m_type = VDB_GRID_DOUBLE;
    else if (grid->isType<openvdb::Int32Grid>())
        m_type = VDB_GRID_INT32;
    else if (grid->isType<openvdb::Int64Grid>())
        m_type = VDB_GRID_INT64;
    else if (grid->isType<openvdb::Vec3IGrid>())
        m_type = VDB_GRID_VEC3I;
    else if (grid->isType<openvdb::Vec3fGrid>())
        m_type = VDB_GRID_VEC3F;
    else if (grid->isType<openvdb::Vec3DGrid>())
        m_type = VDB_GRID_VEC3D;
    else
        m_type = VDB_GRID_NOT_SET;
    
    m_grid = grid;
}

openvdb::GridBase::Ptr VDBAccessor::getGridPtr()
{
    return m_grid;
}


template <typename VectorType>
void copyVectorToFloatArray(VectorType &vec, float *array, int num)
{
    for (int i = 0; i < num; i++)
        array[i] = vec[i];
}

void VDBAccessor::vdb_lookup_single_point(float x, float y, float z, float *result)
{
    switch (m_type) {
        case VDB_GRID_BOOL:
            *result = static_cast<float>(VDBAccessor::vdb_lookup_single_point<openvdb::BoolGrid>(x, y, z));
            break;
        case VDB_GRID_FLOAT:
            *result = VDBAccessor::vdb_lookup_single_point<openvdb::FloatGrid>(x, y, z);
            break;
        case VDB_GRID_DOUBLE:
            *result = static_cast<float>(VDBAccessor::vdb_lookup_single_point<openvdb::DoubleGrid>(x, y, z));
            break;
        case VDB_GRID_INT32:
            *result = static_cast<float>(VDBAccessor::vdb_lookup_single_point<openvdb::Int32Grid>(x, y, z));
            break;
        case VDB_GRID_INT64:
            *result = static_cast<float>(VDBAccessor::vdb_lookup_single_point<openvdb::Int64Grid>(x, y, z));
            break;
        case VDB_GRID_VEC3I:
        {
            openvdb::Vec3i result3i = VDBAccessor::vdb_lookup_single_point<openvdb::Vec3IGrid>(x, y, z);
            copyVectorToFloatArray(result3i, result, 3);
            break;
        }
        case VDB_GRID_VEC3F:
        {
            openvdb::Vec3f result3f = VDBAccessor::vdb_lookup_single_point<openvdb::Vec3fGrid>(x, y, z);
            copyVectorToFloatArray(result3f, result, 3);
            break;
        }
        case VDB_GRID_VEC3D:
        {
            openvdb::Vec3d result3d = VDBAccessor::vdb_lookup_single_point<openvdb::Vec3DGrid>(x, y, z);
            copyVectorToFloatArray(result3d, result, 3);
            break;
        }
        default:
            break;
    }
}

template <typename GridType>
typename GridType::ValueType VDBAccessor::vdb_lookup_single_point(float x, float y, float z)
{
    typename GridType::Ptr grid = openvdb::gridPtrCast<GridType>(getGridPtr());
    typename GridType::Accessor acc = grid->getAccessor();
    
    openvdb::tools::GridSampler<openvdb::tree::ValueAccessor<typename GridType::TreeType>, openvdb::tools::PointSampler> sampler(acc);
    openvdb::Vec3d p(x, y, z);

    return sampler.wsSample(p);
}

template <typename GridType>
typename GridType::ValueType* VDBAccessor::vdb_lookup_multiple_points(float x[], float y[], float z[], int num)
{
    GridType typedGrid = *(openvdb::gridPtrCast<GridType>(m_grid));
    typename GridType::ValueType result;
    openvdb::math::Coord xyz;
    
    typename GridType::ConstAccessor accessor = typedGrid.getAccessor();
    GridType::ValueType *result = new typename GridType::ValueType[num];
    
    for (int pos = 0; pos < num; pos++) {
        
        xyz = openvdb::math::Coord((openvdb::Int32) x[pos],
                                   (openvdb::Int32) y[pos],
                                   (openvdb::Int32) z[pos]);
        result[pos] = accessor.getValue(xyz);
    }
    
    return result;
}


CCL_NAMESPACE_END

#endif /* __VDB_LOOKUP_H__ */
