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

set(INTERN_INC
	intern/gpu_aspect_intern.h
	intern/gpu_basic_intern.h
	intern/gpu_blender_aspect_intern.h
	intern/gpu_codegen.h
	intern/gpu_extensions_intern.h
	intern/gpu_immediate_intern.h
	intern/gpu_lighting_intern.h
	intern/gpu_pixels_intern.h
	intern/gpu_primitives_inline.h
	intern/gpu_profile.h
	intern/gpu_raster_intern.h
    intern/gpu_select_intern.h
	intern/gpu_sprite_intern.h
	intern/gpu_state_latch_intern.h
)

set(GLSL_SRC
    shaders/gpu_shader_vertex.glsl
    shaders/gpu_shader_material.glsl

    shaders/gpu_shader_sep_gaussian_blur_frag.glsl
    shaders/gpu_shader_sep_gaussian_blur_vert.glsl

    shaders/gpu_shader_vsm_store_frag.glsl
    shaders/gpu_shader_vsm_store_vert.glsl

    shaders/gpu_shader_common_attribs.glsl
    shaders/gpu_shader_common_constants.glsl
    shaders/gpu_shader_common_uniforms.glsl

    shaders/gpu_shader_font_frag.glsl
    shaders/gpu_shader_font_vert.glsl

    shaders/gpu_shader_pixels_uniforms.glsl
    shaders/gpu_shader_pixels_frag.glsl
    shaders/gpu_shader_pixels_vert.glsl

    shaders/gpu_shader_basic_frag.glsl
    shaders/gpu_shader_basic_vert.glsl

    shaders/gpu_shader_raster_frag.glsl
    shaders/gpu_shader_raster_uniforms.glsl
    shaders/gpu_shader_raster_vert.glsl
)

set(SRC
	intern/gpu_aspect.c
	intern/gpu_basic.c
	intern/gpu_blender_aspect.c
	intern/gpu_buffers.c
	intern/gpu_codegen.c
	intern/gpu_common.c
	intern/gpu_debug.c
	intern/gpu_draw.c
	intern/gpu_extensions.c
	intern/gpu_font.c
	intern/gpu_immediate.c
	intern/gpu_immediate_gl.c
	intern/gpu_init_exit.c
	intern/gpu_lighting.c
	intern/gpu_material.c
	intern/gpu_matrix.c
	intern/gpu_pixels.c
	intern/gpu_primitives.c
	intern/gpu_raster.c
	intern/gpu_select.c
	intern/gpu_sprite.c
	intern/gpu_state_latch.c
	intern/gpu_utility.c

	GPU_aspect.h
	GPU_basic.h
	GPU_blender_aspect.h
	GPU_buffers.h
	GPU_colors.h
	GPU_common.h
	GPU_deprecated.h
	GPU_draw.h
	GPU_extensions.h
	GPU_font.h
	GPU_glew.h
	GPU_init_exit.h
	GPU_immediate.h
	GPU_lighting.h
	GPU_material.h
	GPU_matrix.h
	GPU_primitives.h
	GPU_raster.h
	GPU_safety.h
    GPU_select.h
	GPU_sprite.h
	GPU_state_latch.h
	GPU_utility.h

    ${INTERN_INC}
    
    ${GLSL_SRC}
)

if(WITH_GPU_SAFETY)
	list(APPEND SRC
		intern/gpu_safety.c
	)
endif()

foreach(GLSL_FILE ${GLSL_SRC})
    data_to_c_simple(${GLSL_FILE} SRC)
    list(APPEND GEN_SRC "${CMAKE_CURRENT_BINARY_DIR}/${GLSL_FILE}.c")
endforeach()

source_group("Internal Header Files" FILES ${INTERN_INC})
source_group("Generated Files"       FILES ${GEN_SRC})
source_group("Shader Files"          FILES ${GLSL_SRC})

if(WITH_MOD_SMOKE)
	add_definitions(-DWITH_SMOKE)
endif()

if(WITH_IMAGE_DDS)
	add_definitions(-DWITH_DDS)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_gpu "${SRC}" "${INC}" "${INC_SYS}")
