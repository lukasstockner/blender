/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __SVM_H__
#define __SVM_H__

/* Shader Virtual Machine
 *
 * A shader is a list of nodes to be executed. These are simply read one after
 * the other and executed, using an node counter. Each node and it's associated
 * data is encoded as one or more uint4's in a 1D texture. If the data is larger
 * than an uint4, the node can increase the node counter to compensate for this.
 * Floats are encoded as int and then converted to float again.
 *
 * Nodes write their output into a stack. All stack data in the stack is
 * floats, since it's all factors, colors and vectors. The stack will be stored
 * in local memory on the GPU, as it would take too many register and indexes in
 * ways not known at compile time. This seems the only solution even though it
 * may be slow, with two positive factors. If the same shader is being executed,
 * memory access will be coalesced, and on fermi cards, memory will actually be
 * cached.
 *
 * The result of shader execution will be a single closure. This means the
 * closure type, associated label, data and weight. Sampling from multiple
 * closures is supported through the mix closure node, the logic for that is
 * mostly taken care of in the SVM compiler.
 */

#include "svm_types.h"

CCL_NAMESPACE_BEGIN

/* Stack */

ccl_device_inline float3 stack_load_float3(float *stack, uint a)
{
	kernel_assert(a+2 < SVM_STACK_SIZE);

	return make_float3(stack[a+0], stack[a+1], stack[a+2]);
}

ccl_device_inline void stack_store_float3(float *stack, uint a, float3 f)
{
	kernel_assert(a+2 < SVM_STACK_SIZE);

	stack[a+0] = f.x;
	stack[a+1] = f.y;
	stack[a+2] = f.z;
}

ccl_device_inline float stack_load_float(float *stack, uint a)
{
	kernel_assert(a < SVM_STACK_SIZE);

	return stack[a];
}

ccl_device_inline float stack_load_float_default(float *stack, uint a, uint value)
{
	return (a == (uint)SVM_STACK_INVALID)? __uint_as_float(value): stack_load_float(stack, a);
}

ccl_device_inline void stack_store_float(float *stack, uint a, float f)
{
	kernel_assert(a < SVM_STACK_SIZE);

	stack[a] = f;
}

ccl_device_inline int stack_load_int(float *stack, uint a)
{
	kernel_assert(a < SVM_STACK_SIZE);

	return __float_as_int(stack[a]);
}

ccl_device_inline int stack_load_int_default(float *stack, uint a, uint value)
{
	return (a == (uint)SVM_STACK_INVALID)? (int)value: stack_load_int(stack, a);
}

ccl_device_inline void stack_store_int(float *stack, uint a, int i)
{
	kernel_assert(a < SVM_STACK_SIZE);

	stack[a] = __int_as_float(i);
}

ccl_device_inline bool stack_valid(uint a)
{
	return a != (uint)SVM_STACK_INVALID;
}

/* Reading Nodes */

ccl_device_inline uint4 read_node(__ADDR_SPACE__ KernelGlobals *kg, int *offset)
{
	uint4 node = kernel_tex_fetch(__svm_nodes, *offset);
	(*offset)++;
	return node;
}

ccl_device_inline float4 read_node_float(__ADDR_SPACE__ KernelGlobals *kg, int *offset)
{
	uint4 node = kernel_tex_fetch(__svm_nodes, *offset);
	float4 f = make_float4(__uint_as_float(node.x), __uint_as_float(node.y), __uint_as_float(node.z), __uint_as_float(node.w));
	(*offset)++;
	return f;
}

ccl_device_inline float4 fetch_node_float(__ADDR_SPACE__ KernelGlobals *kg, int offset)
{
	uint4 node = kernel_tex_fetch(__svm_nodes, offset);
	return make_float4(__uint_as_float(node.x), __uint_as_float(node.y), __uint_as_float(node.z), __uint_as_float(node.w));
}

ccl_device_inline void decode_node_uchar4(uint i, uint *x, uint *y, uint *z, uint *w)
{
	if(x) *x = (i & 0xFF);
	if(y) *y = ((i >> 8) & 0xFF);
	if(z) *z = ((i >> 16) & 0xFF);
	if(w) *w = ((i >> 24) & 0xFF);
}

CCL_NAMESPACE_END

/* Nodes */

#include "svm_noise.h"
#include "svm_texture.h"

#include "svm_attribute.h"
#include "svm_gradient.h"
#include "svm_blackbody.h"
#include "svm_closure.h"
#include "svm_noisetex.h"
#include "svm_convert.h"
#include "svm_displace.h"
#include "svm_fresnel.h"
#include "svm_wireframe.h"
#include "svm_wavelength.h"
#include "svm_camera.h"
#include "svm_geometry.h"
#include "svm_hsv.h"
#include "svm_image.h"
#include "svm_gamma.h"
#include "svm_brightness.h"
#include "svm_invert.h"
#include "svm_light_path.h"
#include "svm_magic.h"
#include "svm_mapping.h"
#include "svm_normal.h"
#include "svm_wave.h"
#include "svm_math_util.h"
#include "svm_math.h"
#include "svm_mix.h"
#include "svm_ramp.h"
#include "svm_sepcomb_hsv.h"
#include "svm_sepcomb_vector.h"
#include "svm_musgrave.h"
#include "svm_sky.h"
#include "svm_tex_coord.h"
#include "svm_value.h"
#include "svm_voronoi.h"
#include "svm_checker.h"
#include "svm_brick.h"
#include "svm_vector_transform.h"

CCL_NAMESPACE_BEGIN

#ifndef __SPLIT_KERNEL__
/* If __SPLIT_KERNEL__ is not enabled, we enable all closures */
#define __NODE_SHADER_JUMP__ 1
#define __NODE_END__ 1
#define __NODE_ATTR_BUMP_DX__ 1
#define __NODE_ATTR_BUMP_DY__ 1
#define __NODE_ATTR__ 1
#define __NODE_BLACKBODY__ 1
#define __NODE_BRIGHTCONTRAST__ 1
#define __NODE_CAMERA__ 1
#define __NODE_CLOSURE_AMBIENT_OCCLUSION__ 1
#define __NODE_CLOSURE_BACKGROUND__ 1
#define __NODE_CLOSURE_BSDF__ 1
#define __NODE_CLOSURE_EMISSION__ 1
#define __NODE_CLOSURE_HOLDOUT__ 1
#define __NODE_CLOSURE_SET_NORMAL__ 1
#define __NODE_CLOSURE_SET_WEIGHT__ 1
#define __NODE_CLOSURE_VOLUME__ 1
#define __NODE_CLOSURE_WEIGHT__ 1
#define __NODE_COMBINE_HSV__ 1
#define __NODE_COMBINE_VECTOR__ 1
#define __NODE_CONVERT__ 1
#define __NODE_EMISSION_WEIGHT__ 1
#define __NODE_FRESNEL__ 1
#define __NODE_GAMMA__ 1
#define __NODE_GEOMETRY_BUMP_DX__ 1
#define __NODE_GEOMETRY_BUMP_DY__ 1
#define __NODE_GEOMETRY__ 1
#define __NODE_HAIR_INFO__ 1
#define __NODE_HSV__ 1
#define __NODE_INVERT__ 1
#define __NODE_JUMP_IF_ONE__ 1
#define __NODE_JUMP_IF_ZERO__ 1
#define __NODE_LAYER_WEIGHT__ 1
#define __NODE_LIGHT_FALLOFF__ 1
#define __NODE_LIGHT_PATH__ 1
#define __NODE_MAPPING__ 1
#define __NODE_MATH__ 1
#define __NODE_MIN_MAX__ 1
#define __NODE_MIX_CLOSURE__ 1
#define __NODE_MIX__ 1
#define __NODE_NORMAL_MAP__ 1
#define __NODE_NORMAL__ 1
#define __NODE_OBJECT_INFO__ 1
#define __NODE_PARTICLE_INFO__ 1
#define __NODE_RGB_CURVES__ 1
#define __NODE_RGB_RAMP__ 1
#define __NODE_SEPARATE_HSV__ 1
#define __NODE_SEPARATE_VECTOR__ 1
#define __NODE_SET_BUMP__ 1
#define __NODE_SET_DISPLACEMENT__ 1
#define __NODE_TANGENT__ 1
#define __NODE_TEX_BRICK__ 1
#define __NODE_TEX_CHECKER__ 1
#define __NODE_TEX_COORD_BUMP_DX__ 1
#define __NODE_TEX_COORD_BUMP_DY__ 1
#define __NODE_TEX_COORD__ 1
#define __NODE_TEX_ENVIRONMENT__ 1
#define __NODE_TEX_GRADIENT__ 1
#define __NODE_TEX_IMAGE_BOX__ 1
#define __NODE_TEX_IMAGE__ 1
#define __NODE_TEX_MAGIC__ 1
#define __NODE_TEX_MUSGRAVE__ 1
#define __NODE_TEX_NOISE__ 1
#define __NODE_TEX_SKY__ 1
#define __NODE_TEX_VORONOI__ 1
#define __NODE_TEX_WAVE__ 1
#define __NODE_VALUE_F__ 1
#define __NODE_VALUE_V__ 1
#define __NODE_VECTOR_CURVES__ 1
#define __NODE_VECTOR_MATH__ 1
#define __NODE_VECTOR_TRANSFORM__ 1
#define __NODE_WAVELENGTH__ 1
#define __NODE_WIREFRAME__ 1
#endif

/* Main Interpreter Loop */
ccl_device_noinline void svm_eval_nodes(__ADDR_SPACE__ KernelGlobals *kg, __ADDR_SPACE__ ShaderData *sd, ShaderType type, int path_flag)
{
	float stack[SVM_STACK_SIZE];
	int offset = sd_fetch(shader) & SHADER_MASK;

	while(1) {
		uint4 node = read_node(kg, &offset);

		switch(node.x) {
#if __NODE_SHADER_JUMP__
			case NODE_SHADER_JUMP: {
				if(type == SHADER_TYPE_SURFACE) offset = node.y;
				else if(type == SHADER_TYPE_VOLUME) offset = node.z;
				else if(type == SHADER_TYPE_DISPLACEMENT) offset = node.w;
				else return;
				break;
			}
#endif
#if __NODE_CLOSURE_BSDF__
			case NODE_CLOSURE_BSDF:
				svm_node_closure_bsdf(kg, sd, stack, node, path_flag, &offset);
				break;
#endif
#if __NODE_CLOSURE_EMISSION__
			case NODE_CLOSURE_EMISSION:
				svm_node_closure_emission(sd, stack, node);
				break;
#endif
#if __NODE_CLOSURE_BACKGROUND__
			case NODE_CLOSURE_BACKGROUND:
				svm_node_closure_background(sd, stack, node);
				break;
#endif
#if __NODE_CLOSURE_HOLDOUT__
			case NODE_CLOSURE_HOLDOUT:
				svm_node_closure_holdout(sd, stack, node);
				break;
#endif
#if __NODE_CLOSURE_AMBIENT_OCCLUSION__
			case NODE_CLOSURE_AMBIENT_OCCLUSION:
				svm_node_closure_ambient_occlusion(sd, stack, node);
				break;
#endif
#if __NODE_CLOSURE_VOLUME__
			case NODE_CLOSURE_VOLUME:
				svm_node_closure_volume(kg, sd, stack, node, path_flag);
				break;
#endif
#if __NODE_CLOSURE_SET_WEIGHT__
			case NODE_CLOSURE_SET_WEIGHT:
				svm_node_closure_set_weight(sd, node.y, node.z, node.w);
				break;
#endif
#if __NODE_CLOSURE_WEIGHT__
			case NODE_CLOSURE_WEIGHT:
				svm_node_closure_weight(sd, stack, node.y);
				break;
#endif
#if __NODE_EMISSION_WEIGHT__
			case NODE_EMISSION_WEIGHT:
				svm_node_emission_weight(kg, sd, stack, node);
				break;
#endif
#if __NODE_MIX_CLOSURE__
			case NODE_MIX_CLOSURE:
				svm_node_mix_closure(sd, stack, node);
				break;
#endif
#if __NODE_JUMP_IF_ZERO__
			case NODE_JUMP_IF_ZERO:
				if(stack_load_float(stack, node.z) == 0.0f)
					offset += node.y;
				break;
#endif
#if __NODE_JUMP_IF_ONE__
			case NODE_JUMP_IF_ONE:
				if(stack_load_float(stack, node.z) == 1.0f)
					offset += node.y;
				break;
#endif
#ifdef __TEXTURES__
#if __NODE_TEX_IMAGE__
			case NODE_TEX_IMAGE:
				svm_node_tex_image(kg, sd, stack, node);
				break;
#endif
#if __NODE_TEX_IMAGE_BOX__
			case NODE_TEX_IMAGE_BOX:
				svm_node_tex_image_box(kg, sd, stack, node);
				break;
#endif
#if __NODE_TEX_ENVIRONMENT__
			case NODE_TEX_ENVIRONMENT:
				svm_node_tex_environment(kg, sd, stack, node);
				break;
#endif
#if __NODE_TEX_SKY__
			case NODE_TEX_SKY:
				svm_node_tex_sky(kg, sd, stack, node, &offset);
				break;
#endif
#if __NODE_TEX_GRADIENT__
			case NODE_TEX_GRADIENT:
				svm_node_tex_gradient(sd, stack, node);
				break;
#endif
#if __NODE_TEX_NOISE__
			case NODE_TEX_NOISE:
				svm_node_tex_noise(kg, sd, stack, node, &offset);
				break;
#endif
#if __NODE_TEX_VORONOI__
			case NODE_TEX_VORONOI:
				svm_node_tex_voronoi(kg, sd, stack, node, &offset);
				break;
#endif
#if __NODE_TEX_MUSGRAVE__
			case NODE_TEX_MUSGRAVE:
				svm_node_tex_musgrave(kg, sd, stack, node, &offset);
				break;
#endif
#if __NODE_TEX_WAVE__
			case NODE_TEX_WAVE:
				svm_node_tex_wave(kg, sd, stack, node, &offset);
				break;
#endif
#if __NODE_TEX_MAGIC__
			case NODE_TEX_MAGIC:
				svm_node_tex_magic(kg, sd, stack, node, &offset);
				break;
#endif
#if __NODE_TEX_CHECKER__
			case NODE_TEX_CHECKER:
				svm_node_tex_checker(kg, sd, stack, node);
				break;
#endif
#if __NODE_TEX_BRICK__
			case NODE_TEX_BRICK:
				svm_node_tex_brick(kg, sd, stack, node, &offset);
				break;
#endif
#endif
#if __NODE_CAMERA__
			case NODE_CAMERA:
				svm_node_camera(kg, sd, stack, node.y, node.z, node.w);
				break;
#endif
#if __NODE_GEOMETRY__
			case NODE_GEOMETRY:
				svm_node_geometry(kg, sd, stack, node.y, node.z);
				break;
#endif
#ifdef __EXTRA_NODES__
#if __NODE_GEOMETRY_BUMP_DX__
			case NODE_GEOMETRY_BUMP_DX:
				svm_node_geometry_bump_dx(kg, sd, stack, node.y, node.z);
				break;
#endif
#if __NODE_GEOMETRY_BUMP_DY__
			case NODE_GEOMETRY_BUMP_DY:
				svm_node_geometry_bump_dy(kg, sd, stack, node.y, node.z);
				break;
#endif
#if __NODE_LIGHT_PATH__
			case NODE_LIGHT_PATH:
				svm_node_light_path(sd, stack, node.y, node.z, path_flag);
				break;
#endif
#if __NODE_OBJECT_INFO__
			case NODE_OBJECT_INFO:
				svm_node_object_info(kg, sd, stack, node.y, node.z);
				break;
#endif
#if __NODE_PARTICLE_INFO__
			case NODE_PARTICLE_INFO:
				svm_node_particle_info(kg, sd, stack, node.y, node.z);
				break;
#endif
#ifdef __HAIR__
#if __NODE_HAIR_INFO__
			case NODE_HAIR_INFO:
				svm_node_hair_info(kg, sd, stack, node.y, node.z);
				break;
#endif
#endif

#endif
#if __NODE_CONVERT__
			case NODE_CONVERT:
				svm_node_convert(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if __NODE_VALUE_F__
			case NODE_VALUE_F:
				svm_node_value_f(kg, sd, stack, node.y, node.z);
				break;
#endif
#if __NODE_VALUE_V__
			case NODE_VALUE_V:
				svm_node_value_v(kg, sd, stack, node.y, &offset);
				break;
#endif
#ifdef __EXTRA_NODES__
#if __NODE_INVERT__
			case NODE_INVERT:
				svm_node_invert(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if __NODE_GAMMA__
			case NODE_GAMMA:
				svm_node_gamma(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if __NODE_BRIGHTCONTRAST__
			case NODE_BRIGHTCONTRAST:
				svm_node_brightness(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if __NODE_MIX__
			case NODE_MIX:
				svm_node_mix(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if __NODE_SEPARATE_VECTOR__
			case NODE_SEPARATE_VECTOR:
				svm_node_separate_vector(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if __NODE_COMBINE_VECTOR__
			case NODE_COMBINE_VECTOR:
				svm_node_combine_vector(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if __NODE_SEPARATE_HSV__
			case NODE_SEPARATE_HSV:
				svm_node_separate_hsv(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if __NODE_COMBINE_HSV__
			case NODE_COMBINE_HSV:
				svm_node_combine_hsv(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if __NODE_HSV__
			case NODE_HSV:
				svm_node_hsv(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#endif
#if __NODE_ATTR__
			case NODE_ATTR:
				svm_node_attr(kg, sd, stack, node);
				break;
#endif
#ifdef __EXTRA_NODES__
#if __NODE_ATTR_BUMP_DX__
			case NODE_ATTR_BUMP_DX:
				svm_node_attr_bump_dx(kg, sd, stack, node);
				break;
#endif
#if __NODE_ATTR_BUMP_DY__
			case NODE_ATTR_BUMP_DY:
				svm_node_attr_bump_dy(kg, sd, stack, node);
				break;
#endif
#endif
#if __NODE_FRESNEL__
			case NODE_FRESNEL:
				svm_node_fresnel(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if __NODE_LAYER_WEIGHT__
			case NODE_LAYER_WEIGHT:
				svm_node_layer_weight(sd, stack, node);
				break;
#endif
#ifdef __EXTRA_NODES__
#if __NODE_WIREFRAME__
			case NODE_WIREFRAME:
				svm_node_wireframe(kg, sd, stack, node);
				break;
#endif
#if __NODE_WAVELENGTH__
			case NODE_WAVELENGTH:
				svm_node_wavelength(sd, stack, node.y, node.z);
				break;
#endif
#if __NODE_BLACKBODY__
			case NODE_BLACKBODY:
				svm_node_blackbody(kg, sd, stack, node.y, node.z);
				break;
#endif
#if __NODE_SET_DISPLACEMENT__
			case NODE_SET_DISPLACEMENT:
				svm_node_set_displacement(sd, stack, node.y);
				break;
#endif
#if __NODE_SET_BUMP__
			case NODE_SET_BUMP:
				svm_node_set_bump(kg, sd, stack, node);
				break;
#endif
#if __NODE_MATH__
			case NODE_MATH:
				svm_node_math(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if __NODE_VECTOR_MATH__
			case NODE_VECTOR_MATH:
				svm_node_vector_math(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if __NODE_VECTOR_TRANSFORM__
			case NODE_VECTOR_TRANSFORM:
				svm_node_vector_transform(kg, sd, stack, node);
				break;
#endif
#if __NODE_NORMAL__
			case NODE_NORMAL:
				svm_node_normal(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#endif
#if __NODE_MAPPING__
			case NODE_MAPPING:
				svm_node_mapping(kg, sd, stack, node.y, node.z, &offset);
				break;
#endif
#if __NODE_MIN_MAX__
			case NODE_MIN_MAX:
				svm_node_min_max(kg, sd, stack, node.y, node.z, &offset);
				break;
#endif
#if __NODE_TEX_COORD__
			case NODE_TEX_COORD:
				svm_node_tex_coord(kg, sd, path_flag, stack, node, &offset);
				break;
#endif
#ifdef __EXTRA_NODES__
#if __NODE_TEX_COORD_BUMP_DX__
			case NODE_TEX_COORD_BUMP_DX:
				svm_node_tex_coord_bump_dx(kg, sd, path_flag, stack, node, &offset);
				break;
#endif
#if __NODE_TEX_COORD_BUMP_DY__
			case NODE_TEX_COORD_BUMP_DY:
				svm_node_tex_coord_bump_dy(kg, sd, path_flag, stack, node, &offset);
				break;
#endif
#if __NODE_CLOSURE_SET_NORMAL__
			case NODE_CLOSURE_SET_NORMAL:
				svm_node_set_normal(kg, sd, stack, node.y, node.z );
				break;
#endif
#if __NODE_RGB_RAMP__
			case NODE_RGB_RAMP:
				svm_node_rgb_ramp(kg, sd, stack, node, &offset);
				break;
#endif
#if __NODE_RGB_CURVES__
			case NODE_RGB_CURVES:
				svm_node_rgb_curves(kg, sd, stack, node, &offset);
				break;
#endif
#if __NODE_VECTOR_CURVES__
			case NODE_VECTOR_CURVES:
				svm_node_vector_curves(kg, sd, stack, node, &offset);
				break;
#endif
#if __NODE_LIGHT_FALLOFF__
			case NODE_LIGHT_FALLOFF:
				svm_node_light_falloff(sd, stack, node);
				break;
#endif
#endif
#if __NODE_TANGENT__
			case NODE_TANGENT:
				svm_node_tangent(kg, sd, stack, node);
				break;
#endif
#if __NODE_NORMAL_MAP__
			case NODE_NORMAL_MAP:
				svm_node_normal_map(kg, sd, stack, node);
				break;
#endif
#if __NODE_END__
			case NODE_END:
#endif
			default:
				return;
		}
	}
}
CCL_NAMESPACE_END

#endif /* __SVM_H__ */

