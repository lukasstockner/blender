# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Rafael Campos.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	internal
)

set(INC_SYS
	${BOOST_INCLUDE_DIR}
	${TBB_INCLUDE_DIRS}
	${OPENEXR_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	internal/openvdb/Grid.cc
	internal/openvdb/io/Archive.cc
	internal/openvdb/io/Compression.cc
	internal/openvdb/io/File.cc
	internal/openvdb/io/GridDescriptor.cc
	internal/openvdb/io/Stream.cc
	internal/openvdb/math/Hermite.cc
	internal/openvdb/math/Maps.cc
	internal/openvdb/math/Proximity.cc
	internal/openvdb/math/QuantizedUnitVec.cc
	internal/openvdb/math/Transform.cc
	internal/openvdb/metadata/Metadata.cc
	internal/openvdb/metadata/MetaMap.cc
	internal/openvdb/openvdb.cc
	internal/openvdb/Platform.cc
	internal/openvdb/util/Formats.cc
	internal/openvdb/util/Util.cc

	internal/openvdb/Exceptions.h
	internal/openvdb/Grid.h
	internal/openvdb/io/Archive.h
	internal/openvdb/io/Compression.h
	internal/openvdb/io/File.h
	internal/openvdb/io/GridDescriptor.h
	internal/openvdb/io/Stream.h
	internal/openvdb/math/BBox.h
	internal/openvdb/math/Coord.h
	internal/openvdb/math/FiniteDifference.h
	internal/openvdb/math/Hermite.h
	internal/openvdb/math/LegacyFrustum.h
	internal/openvdb/math/Maps.h
	internal/openvdb/math/Mat.h
	internal/openvdb/math/Mat3.h
	internal/openvdb/math/Mat4.h
	internal/openvdb/math/Math.h
	internal/openvdb/math/Operators.h
	internal/openvdb/math/Proximity.h
	internal/openvdb/math/QuantizedUnitVec.h
	internal/openvdb/math/Quat.h
	internal/openvdb/math/Stats.h
	internal/openvdb/math/Stencils.h
	internal/openvdb/math/Transform.h
	internal/openvdb/math/Tuple.h
	internal/openvdb/math/Vec2.h
	internal/openvdb/math/Vec3.h
	internal/openvdb/math/Vec4.h
	internal/openvdb/Metadata.h
	internal/openvdb/metadata/Metadata.h
	internal/openvdb/metadata/MetaMap.h
	internal/openvdb/metadata/StringMetadata.h
	internal/openvdb/openvdb.h
	internal/openvdb/Platform.h
	internal/openvdb/PlatformConfig.h
	internal/openvdb/tools/Composite.h
	internal/openvdb/tools/Dense.h
	internal/openvdb/tools/Filter.h
	internal/openvdb/tools/GridOperators.h
	internal/openvdb/tools/GridTransformer.h
	internal/openvdb/tools/Interpolation.h
	internal/openvdb/tools/LevelSetAdvect.h
	internal/openvdb/tools/LevelSetFilter.h
	internal/openvdb/tools/LevelSetFracture.h
	internal/openvdb/tools/LevelSetRebuild.h
	internal/openvdb/tools/LevelSetSphere.h
	internal/openvdb/tools/LevelSetTracker.h
	internal/openvdb/tools/LevelSetUtil.h
	internal/openvdb/tools/MeshToVolume.h
	internal/openvdb/tools/Morphology.h
	internal/openvdb/tools/ParticlesToLevelSet.h
	internal/openvdb/tools/PointAdvect.h
	internal/openvdb/tools/PointScatter.h
	internal/openvdb/tools/Statistics.h
	internal/openvdb/tools/ValueTransformer.h
	internal/openvdb/tools/VolumeToMesh.h
	internal/openvdb/tree/InternalNode.h
	internal/openvdb/tree/Iterator.h
	internal/openvdb/tree/LeafManager.h
	internal/openvdb/tree/LeafNode.h
	internal/openvdb/tree/LeafNodeBool.h
	internal/openvdb/tree/NodeUnion.h
	internal/openvdb/tree/RootNode.h
	internal/openvdb/tree/Tree.h
	internal/openvdb/tree/TreeIterator.h
	internal/openvdb/tree/Util.h
	internal/openvdb/tree/ValueAccessor.h
	internal/openvdb/Types.h
	internal/openvdb/util/Formats.h
	internal/openvdb/util/logging.h	
	internal/openvdb/util/MapsUtil.h
	internal/openvdb/util/Name.h
	internal/openvdb/util/NodeMasks.h
	internal/openvdb/util/NullInterrupter.h
	internal/openvdb/util/Util.h
	internal/openvdb/version.h
)

blender_add_lib(extern_openvdb "${SRC}" "${INC}" "${INC_SYS}")