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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../blenkernel
	../blenlib
	../bmesh
	../imbuf
	../makesdna
	../makesrna

	# For node muting stuff...
	../nodes
	../nodes/intern

	../../../intern/guardedalloc
	../../../intern/smoke/extern
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	intern/gpu_buffers.c
	intern/gpu_codegen.c
	intern/gpu_debug.c
	intern/gpu_draw.c
	intern/gpu_extensions.c
	intern/gpu_extension_wrapper.c
	intern/gpu_immediate.c
	intern/gpu_immediate_glsl.c
	intern/gpu_immediate_gl11.c
	intern/gpu_immediate_vbo.c
	intern/gpu_lighting.c
	intern/gpu_lighting_gl11.c
	intern/gpu_lighting_glsl.c
	intern/gpu_material.c
	intern/gpu_matrix.c
	intern/gpu_object.c
	intern/gpu_object_gles.c
	intern/gpu_object_gl11.c
	intern/gpu_pixels.c
	intern/gpu_primitives.c
	intern/gpu_raster.c
	intern/gpu_simple_shader.c
	intern/gpu_state_latch.c
	intern/gpu_view.c
	intern/gpu_view_gl.c

	GPU_buffers.h
	GPU_colors.h
	GPU_compatibility.h
	GPU_draw.h
	GPU_extensions.h
	GPU_material.h
	GPU_utility.h
	GPU_simple_shader.h
	GPU_matrix.h
	GPU_primitives.h
	GPU_object.h

	intern/gpu_codegen.h
	intern/gpu_deprecated.h
	intern/gpu_extension_wrapper.h
	intern/gpu_glew.h
	intern/gpu_immediate.h
	intern/gpu_immediate_inline.h
	intern/gpu_immediate_internal.h
	intern/gpu_lighting.h
	intern/gpu_lighting_inline.h
	intern/gpu_lighting_internal.h
	intern/gpu_object_gles.h
	intern/gpu_object_gl11.h
	intern/gpu_pixels.h
	intern/gpu_primitives_inline.h
	intern/gpu_profile.h
	intern/gpu_raster.h
	intern/gpu_safety.h
	intern/gpu_state_latch.h
	intern/gpu_view.h
	intern/gpu_view_gl.h
)

data_to_c_simple(shaders/gpu_shader_material.glsl SRC)
data_to_c_simple(shaders/gpu_shader_sep_gaussian_blur_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_sep_gaussian_blur_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_simple_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_simple_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_vertex.glsl SRC)
data_to_c_simple(shaders/gpu_shader_vsm_store_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_vsm_store_vert.glsl SRC)

if(WITH_MOD_SMOKE)
	add_definitions(-DWITH_SMOKE)
endif()

if(WITH_IMAGE_DDS)
	add_definitions(-DWITH_DDS)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_gpu "${SRC}" "${INC}" "${INC_SYS}")

