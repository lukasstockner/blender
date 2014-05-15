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
# Contributor(s): Sergey Sharybin.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../guardedalloc
)

set(INC_SYS
	${OPENSUBDIV_INCLUDE_DIR}
	${GLEW_INCLUDE_PATH}
)

set(SRC
	opensubdiv_capi.cc
	opensubdiv_gpu_capi.cc
	clInit.h
	cudaInit.h
	opensubdiv_capi.h
)

MACRO(OPENSUBDIV_DEFINE_COMPONENT component)
	if(${${component}})
		add_definitions(-D${component})
	endif()
ENDMACRO()

OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_OPENMP)
OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_OPENCL)
OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_CUDA)
OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_GLSL_COMPUTE)

data_to_c_simple(gpu_shader_opensubd_display.glsl SRC)

add_definitions(-DGLEW_STATIC)

blender_add_lib(bf_intern_opensubdiv "${SRC}" "${INC}" "${INC_SYS}")
