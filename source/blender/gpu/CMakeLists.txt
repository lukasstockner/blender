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
    intern/gpu_aspect.c
	intern/gpu_aspectfuncs.c
	intern/gpu_basic_shader.c
	intern/gpu_buffers.c
	intern/gpu_codegen.c
	intern/gpu_common.c
	intern/gpu_debug.c
	intern/gpu_draw.c
	intern/gpu_extensions.c
	intern/gpu_extension_wrapper.c
	intern/gpu_font_shader.c
	intern/gpu_immediate.c
	intern/gpu_immediate_gl.c
	intern/gpu_lighting.c
	intern/gpu_material.c
	intern/gpu_matrix.c
	intern/gpu_pixels.c
	intern/gpu_primitives.c
	intern/gpu_raster.c
	intern/gpu_select.c
	intern/gpu_state_latch.c
	intern/gpu_view.c
	intern/gpu_view_gl.c

	GPU_basic_shader.h
	GPU_buffers.h
	GPU_colors.h
	GPU_draw.h
	GPU_compatibility.h
	GPU_extensions.h
	GPU_font_shader.h
	GPU_material.h
	GPU_matrix.h
	GPU_object.h
	GPU_primitives.h
	GPU_utility.h

	intern/gpu_aspect.h
	intern/gpu_aspectfuncs.h
	intern/gpu_codegen.h
	intern/gpu_common.h
	intern/gpu_deprecated.h
	intern/gpu_extension_wrapper.h
	intern/gpu_glew.h
	intern/gpu_immediate.h
	intern/gpu_immediate_inline.h
	intern/gpu_immediate_gl.h
	intern/gpu_lighting.h
	intern/gpu_pixels.h
	intern/gpu_primitives_inline.h
	intern/gpu_profile.h
	intern/gpu_raster.h
	intern/gpu_safety.h
	intern/gpu_select.h
	intern/gpu_state_latch.h
	intern/gpu_view.h
	intern/gpu_view_gl.h
)

if(WITH_GPU_SAFETY)
	list(APPEND SRC
		intern/gpu_safety.c
	)
endif()

data_to_c_simple(shaders/gpu_shader_vertex.glsl   SRC)
data_to_c_simple(shaders/gpu_shader_material.glsl SRC)

data_to_c_simple(shaders/gpu_shader_sep_gaussian_blur_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_sep_gaussian_blur_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_vsm_store_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_vsm_store_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_common_attribs.glsl   SRC)
data_to_c_simple(shaders/gpu_shader_common_constants.glsl SRC)
data_to_c_simple(shaders/gpu_shader_common_uniforms.glsl  SRC)

data_to_c_simple(shaders/gpu_shader_font_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_font_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_pixels_uniforms.glsl SRC)
data_to_c_simple(shaders/gpu_shader_pixels_frag.glsl     SRC)
data_to_c_simple(shaders/gpu_shader_pixels_vert.glsl     SRC)

data_to_c_simple(shaders/gpu_shader_basic_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_basic_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_raster_frag.glsl     SRC)
data_to_c_simple(shaders/gpu_shader_raster_uniforms.glsl SRC)
data_to_c_simple(shaders/gpu_shader_raster_vert.glsl     SRC)

if(WITH_MOD_SMOKE)
	add_definitions(-DWITH_SMOKE)
endif()

if(WITH_IMAGE_DDS)
	add_definitions(-DWITH_DDS)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_gpu "${SRC}" "${INC}" "${INC_SYS}")
