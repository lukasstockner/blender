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
# The Original Code is Copyright (C) 2013, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	intern
	util
	../blenkernel
	../blenlib
	../makesdna
	../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
	test.cpp

	util/util_path.h
	util/util_path.cpp

	PTC_api.h
	PTC_api.cpp
)

if(WITH_ALEMBIC)
	list(APPEND INC_SYS ${ALEMBIC_INCLUDE_DIRS} ${OPENEXR_INCLUDE_DIR}/OpenEXR)

	list(APPEND SRC
	intern/particles.h
	intern/particles.cpp
	intern/reader.h
	intern/reader.cpp
	intern/schema.h
	intern/types.h
	intern/writer.h
	intern/writer.cpp
	)

	add_definitions(-DWITH_ALEMBIC)
endif()

blender_add_lib(bf_pointcache "${SRC}" "${INC}" "${INC_SYS}")
