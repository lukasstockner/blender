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

ccl_device_inline uint4 read_node(KernelGlobals *kg, int *offset)
{
	uint4 node = kernel_tex_fetch(__svm_nodes, *offset);
	(*offset)++;
	return node;
}

ccl_device_inline float4 read_node_float(KernelGlobals *kg, int *offset)
{
	uint4 node = kernel_tex_fetch(__svm_nodes, *offset);
	float4 f = make_float4(__uint_as_float(node.x), __uint_as_float(node.y), __uint_as_float(node.z), __uint_as_float(node.w));
	(*offset)++;
	return f;
}

ccl_device_inline float4 fetch_node_float(KernelGlobals *kg, int offset)
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

#include "svm_math_util.h"

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

/* Main Interpreter Loop */
ccl_device_noinline void svm_eval_nodes(KernelGlobals *kg, ShaderData *sd, ShaderType type, int path_flag)
{
	float stack[SVM_STACK_SIZE];
	int offset = sd_fetch(shader) & SHADER_MASK;

	while(1) {
		uint4 node = read_node(kg, &offset);

		switch(node.x) {
#if defined(__NODE_SHADER_JUMP__) || !defined(__SPLIT_KERNEL__)
			case NODE_SHADER_JUMP: {
				if(type == SHADER_TYPE_SURFACE) offset = node.y;
				else if(type == SHADER_TYPE_VOLUME) offset = node.z;
				else if(type == SHADER_TYPE_DISPLACEMENT) offset = node.w;
				else return;
				break;
			}
#endif
#if defined(__NODE_CLOSURE_BSDF__) || !defined(__SPLIT_KERNEL__)
			case NODE_CLOSURE_BSDF:
				svm_node_closure_bsdf(kg, sd, stack, node, path_flag, &offset);
				break;
#endif
#if defined(__NODE_CLOSURE_EMISSION__) || !defined(__SPLIT_KERNEL__)
			case NODE_CLOSURE_EMISSION:
				svm_node_closure_emission(sd, stack, node);
				break;
#endif
#if defined(__NODE_CLOSURE_BACKGROUND__) || !defined(__SPLIT_KERNEL__)
			case NODE_CLOSURE_BACKGROUND:
				svm_node_closure_background(sd, stack, node);
				break;
#endif
#if defined(__NODE_CLOSURE_HOLDOUT__) || !defined(__SPLIT_KERNEL__)
			case NODE_CLOSURE_HOLDOUT:
				svm_node_closure_holdout(sd, stack, node);
				break;
#endif
#if defined(__NODE_CLOSURE_AMBIENT_OCCLUSION__) || !defined(__SPLIT_KERNEL__)
			case NODE_CLOSURE_AMBIENT_OCCLUSION:
				svm_node_closure_ambient_occlusion(sd, stack, node);
				break;
#endif
#if defined(__NODE_CLOSURE_VOLUME__) || !defined(__SPLIT_KERNEL__)
			case NODE_CLOSURE_VOLUME:
				svm_node_closure_volume(kg, sd, stack, node, path_flag);
				break;
#endif
#if defined(__NODE_CLOSURE_SET_WEIGHT__) || !defined(__SPLIT_KERNEL__)
			case NODE_CLOSURE_SET_WEIGHT:
				svm_node_closure_set_weight(sd, node.y, node.z, node.w);
				break;
#endif
#if defined(__NODE_CLOSURE_WEIGHT__) || !defined(__SPLIT_KERNEL__)
			case NODE_CLOSURE_WEIGHT:
				svm_node_closure_weight(sd, stack, node.y);
				break;
#endif
#if defined(__NODE_EMISSION_WEIGHT__) || !defined(__SPLIT_KERNEL__)
			case NODE_EMISSION_WEIGHT:
				svm_node_emission_weight(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_MIX_CLOSURE__) || !defined(__SPLIT_KERNEL__)
			case NODE_MIX_CLOSURE:
				svm_node_mix_closure(sd, stack, node);
				break;
#endif
#if defined(__NODE_JUMP_IF_ZERO__) || !defined(__SPLIT_KERNEL__)
			case NODE_JUMP_IF_ZERO:
				if(stack_load_float(stack, node.z) == 0.0f)
					offset += node.y;
				break;
#endif
#if defined(__NODE_JUMP_IF_ONE__) || !defined(__SPLIT_KERNEL__)
			case NODE_JUMP_IF_ONE:
				if(stack_load_float(stack, node.z) == 1.0f)
					offset += node.y;
				break;
#endif
#ifdef __TEXTURES__
#if defined(__NODE_TEX_IMAGE__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_IMAGE:
				svm_node_tex_image(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_TEX_IMAGE_BOX__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_IMAGE_BOX:
				svm_node_tex_image_box(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_TEX_ENVIRONMENT__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_ENVIRONMENT:
				svm_node_tex_environment(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_TEX_SKY__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_SKY:
				svm_node_tex_sky(kg, sd, stack, node, &offset);
				break;
#endif
#if defined(__NODE_TEX_GRADIENT__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_GRADIENT:
				svm_node_tex_gradient(sd, stack, node);
				break;
#endif
#if defined(__NODE_TEX_NOISE__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_NOISE:
				svm_node_tex_noise(kg, sd, stack, node, &offset);
				break;
#endif
#if defined(__NODE_TEX_VORONOI__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_VORONOI:
				svm_node_tex_voronoi(kg, sd, stack, node, &offset);
				break;
#endif
#if defined(__NODE_TEX_MUSGRAVE__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_MUSGRAVE:
				svm_node_tex_musgrave(kg, sd, stack, node, &offset);
				break;
#endif
#if defined(__NODE_TEX_WAVE__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_WAVE:
				svm_node_tex_wave(kg, sd, stack, node, &offset);
				break;
#endif
#if defined(__NODE_TEX_MAGIC__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_MAGIC:
				svm_node_tex_magic(kg, sd, stack, node, &offset);
				break;
#endif
#if defined(__NODE_TEX_CHECKER__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_CHECKER:
				svm_node_tex_checker(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_TEX_BRICK__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_BRICK:
				svm_node_tex_brick(kg, sd, stack, node, &offset);
				break;
#endif
#endif
#if defined(__NODE_CAMERA__) || !defined(__SPLIT_KERNEL__)
			case NODE_CAMERA:
				svm_node_camera(kg, sd, stack, node.y, node.z, node.w);
				break;
#endif
#if defined(__NODE_GEOMETRY__) || !defined(__SPLIT_KERNEL__)
			case NODE_GEOMETRY:
				svm_node_geometry(kg, sd, stack, node.y, node.z);
				break;
#endif
#ifdef __EXTRA_NODES__
#if defined(__NODE_GEOMETRY_BUMP_DX__) || !defined(__SPLIT_KERNEL__)
			case NODE_GEOMETRY_BUMP_DX:
				svm_node_geometry_bump_dx(kg, sd, stack, node.y, node.z);
				break;
#endif
#if defined(__NODE_GEOMETRY_BUMP_DY__) || !defined(__SPLIT_KERNEL__)
			case NODE_GEOMETRY_BUMP_DY:
				svm_node_geometry_bump_dy(kg, sd, stack, node.y, node.z);
				break;
#endif
#if defined(__NODE_LIGHT_PATH__) || !defined(__SPLIT_KERNEL__)
			case NODE_LIGHT_PATH:
				svm_node_light_path(sd, stack, node.y, node.z, path_flag);
				break;
#endif
#if defined(__NODE_OBJECT_INFO__) || !defined(__SPLIT_KERNEL__)
			case NODE_OBJECT_INFO:
				svm_node_object_info(kg, sd, stack, node.y, node.z);
				break;
#endif
#if defined(__NODE_PARTICLE_INFO__) || !defined(__SPLIT_KERNEL__)
			case NODE_PARTICLE_INFO:
				svm_node_particle_info(kg, sd, stack, node.y, node.z);
				break;
#endif
#ifdef __HAIR__
#if defined(__NODE_HAIR_INFO__) || !defined(__SPLIT_KERNEL__)
			case NODE_HAIR_INFO:
				svm_node_hair_info(kg, sd, stack, node.y, node.z);
				break;
#endif
#endif

#endif
#if defined(__NODE_CONVERT__) || !defined(__SPLIT_KERNEL__)
			case NODE_CONVERT:
				svm_node_convert(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if defined(__NODE_VALUE_F__) || !defined(__SPLIT_KERNEL__)
			case NODE_VALUE_F:
				svm_node_value_f(kg, sd, stack, node.y, node.z);
				break;
#endif
#if defined(__NODE_VALUE_V__) || !defined(__SPLIT_KERNEL__)
			case NODE_VALUE_V:
				svm_node_value_v(kg, sd, stack, node.y, &offset);
				break;
#endif
#ifdef __EXTRA_NODES__
#if defined(__NODE_INVERT__) || !defined(__SPLIT_KERNEL__)
			case NODE_INVERT:
				svm_node_invert(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if defined(__NODE_GAMMA__) || !defined(__SPLIT_KERNEL__)
			case NODE_GAMMA:
				svm_node_gamma(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if defined(__NODE_BRIGHTCONTRAST__) || !defined(__SPLIT_KERNEL__)
			case NODE_BRIGHTCONTRAST:
				svm_node_brightness(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if defined(__NODE_MIX__) || !defined(__SPLIT_KERNEL__)
			case NODE_MIX:
				svm_node_mix(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if defined(__NODE_SEPARATE_VECTOR__) || !defined(__SPLIT_KERNEL__)
			case NODE_SEPARATE_VECTOR:
				svm_node_separate_vector(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if defined(__NODE_COMBINE_VECTOR__) || !defined(__SPLIT_KERNEL__)
			case NODE_COMBINE_VECTOR:
				svm_node_combine_vector(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if defined(__NODE_SEPARATE_HSV__) || !defined(__SPLIT_KERNEL__)
			case NODE_SEPARATE_HSV:
				svm_node_separate_hsv(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if defined(__NODE_COMBINE_HSV__) || !defined(__SPLIT_KERNEL__)
			case NODE_COMBINE_HSV:
				svm_node_combine_hsv(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if defined(__NODE_HSV__) || !defined(__SPLIT_KERNEL__)
			case NODE_HSV:
				svm_node_hsv(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#endif
#if defined(__NODE_ATTR__) || !defined(__SPLIT_KERNEL__)
			case NODE_ATTR:
				svm_node_attr(kg, sd, stack, node);
				break;
#endif
#ifdef __EXTRA_NODES__
#if defined(__NODE_ATTR_BUMP_DX__) || !defined(__SPLIT_KERNEL__)
			case NODE_ATTR_BUMP_DX:
				svm_node_attr_bump_dx(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_ATTR_BUMP_DY__) || !defined(__SPLIT_KERNEL__)
			case NODE_ATTR_BUMP_DY:
				svm_node_attr_bump_dy(kg, sd, stack, node);
				break;
#endif
#endif
#if defined(__NODE_FRESNEL__) || !defined(__SPLIT_KERNEL__)
			case NODE_FRESNEL:
				svm_node_fresnel(sd, stack, node.y, node.z, node.w);
				break;
#endif
#if defined(__NODE_LAYER_WEIGHT__) || !defined(__SPLIT_KERNEL__)
			case NODE_LAYER_WEIGHT:
				svm_node_layer_weight(sd, stack, node);
				break;
#endif
#ifdef __EXTRA_NODES__
#if defined(__NODE_WIREFRAME__) || !defined(__SPLIT_KERNEL__)
			case NODE_WIREFRAME:
				svm_node_wireframe(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_WAVELENGTH__) || !defined(__SPLIT_KERNEL__)
			case NODE_WAVELENGTH:
				svm_node_wavelength(sd, stack, node.y, node.z);
				break;
#endif
#if defined(__NODE_BLACKBODY__) || !defined(__SPLIT_KERNEL__)
			case NODE_BLACKBODY:
				svm_node_blackbody(kg, sd, stack, node.y, node.z);
				break;
#endif
#if defined(__NODE_SET_DISPLACEMENT__) || !defined(__SPLIT_KERNEL__)
			case NODE_SET_DISPLACEMENT:
				svm_node_set_displacement(sd, stack, node.y);
				break;
#endif
#if defined(__NODE_SET_BUMP__) || !defined(__SPLIT_KERNEL__)
			case NODE_SET_BUMP:
				svm_node_set_bump(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_MATH__) || !defined(__SPLIT_KERNEL__)
			case NODE_MATH:
				svm_node_math(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if defined(__NODE_VECTOR_MATH__) || !defined(__SPLIT_KERNEL__)
			case NODE_VECTOR_MATH:
				svm_node_vector_math(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#if defined(__NODE_VECTOR_TRANSFORM__) || !defined(__SPLIT_KERNEL__)
			case NODE_VECTOR_TRANSFORM:
				svm_node_vector_transform(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_NORMAL__) || !defined(__SPLIT_KERNEL__)
			case NODE_NORMAL:
				svm_node_normal(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
#endif
#if defined(__NODE_MAPPING__) || !defined(__SPLIT_KERNEL__)
			case NODE_MAPPING:
				svm_node_mapping(kg, sd, stack, node.y, node.z, &offset);
				break;
#endif
#if defined(__NODE_MIN_MAX__) || !defined(__SPLIT_KERNEL__)
			case NODE_MIN_MAX:
				svm_node_min_max(kg, sd, stack, node.y, node.z, &offset);
				break;
#endif
#if defined(__NODE_TEX_COORD__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_COORD:
				svm_node_tex_coord(kg, sd, path_flag, stack, node, &offset);
				break;
#endif
#ifdef __EXTRA_NODES__
#if defined(__NODE_TEX_COORD_BUMP_DX__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_COORD_BUMP_DX:
				svm_node_tex_coord_bump_dx(kg, sd, path_flag, stack, node, &offset);
				break;
#endif
#if defined(__NODE_TEX_COORD_BUMP_DY__) || !defined(__SPLIT_KERNEL__)
			case NODE_TEX_COORD_BUMP_DY:
				svm_node_tex_coord_bump_dy(kg, sd, path_flag, stack, node, &offset);
				break;
#endif
#if defined(__NODE_CLOSURE_SET_NORMAL__) || !defined(__SPLIT_KERNEL__)
			case NODE_CLOSURE_SET_NORMAL:
				svm_node_set_normal(kg, sd, stack, node.y, node.z );
				break;
#endif
#if defined(__NODE_RGB_RAMP__) || !defined(__SPLIT_KERNEL__)
			case NODE_RGB_RAMP:
				svm_node_rgb_ramp(kg, sd, stack, node, &offset);
				break;
#endif
#if defined(__NODE_RGB_CURVES__) || !defined(__SPLIT_KERNEL__)
			case NODE_RGB_CURVES:
				svm_node_rgb_curves(kg, sd, stack, node, &offset);
				break;
#endif
#if defined(__NODE_VECTOR_CURVES__) || !defined(__SPLIT_KERNEL__)
			case NODE_VECTOR_CURVES:
				svm_node_vector_curves(kg, sd, stack, node, &offset);
				break;
#endif
#if defined(__NODE_LIGHT_FALLOFF__) || !defined(__SPLIT_KERNEL__)
			case NODE_LIGHT_FALLOFF:
				svm_node_light_falloff(sd, stack, node);
				break;
#endif
#endif
#if defined(__NODE_TANGENT__) || !defined(__SPLIT_KERNEL__)
			case NODE_TANGENT:
				svm_node_tangent(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_NORMAL_MAP__) || !defined(__SPLIT_KERNEL__)
			case NODE_NORMAL_MAP:
				svm_node_normal_map(kg, sd, stack, node);
				break;
#endif
#if defined(__NODE_END__) || !defined(__SPLIT_KERNEL__)
			case NODE_END:
#endif
			default:
				return;
		}
	}
}
CCL_NAMESPACE_END

#endif /* __SVM_H__ */

