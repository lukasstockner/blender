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
# Contributor(s): Daniel Genrich
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	intern/util
	intern/source
	../memutil
)

set(INC_SYS
	${PNG_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	intern/smoke_API.cpp
	
	intern/source/advectionplugins.cpp
	intern/source/animplugins.cpp
	intern/source/conjugategrad.cpp
	intern/source/fileio.cpp
	intern/source/fluidsolver.cpp
	intern/source/glutgui.cpp
	intern/source/guihelpers.cpp
	intern/source/initplugins.cpp
	intern/source/smoke.cpp
	intern/source/poissonsolvers.cpp
	intern/source/smokeplugins.cpp
	intern/source/solverinit.cpp
	intern/source/solverparams.cpp
	intern/source/stdplugins.cpp
	intern/source/vortexpart.cpp
	intern/source/vortexplugins.cpp
	
	intern/util/globals.cpp
	intern/util/paramset.cpp
)

# quiet -Wundef
add_definitions(-DDDF_DEBUG=0)

if(WITH_OPENMP)
	add_definitions(-DDDF_OPENMP=1)
else()
	add_definitions(-DDDF_OPENMP=0)
endif()

#if(WITH_FFTW3)
#	add_definitions(-DWITH_FFTW3)
#	list(APPEND INC
#		${FFTW3_INCLUDE_DIRS}
#	)
#endif()

blender_add_lib(bf_intern_smoke "${SRC}" "${INC}" "${INC_SYS}")

