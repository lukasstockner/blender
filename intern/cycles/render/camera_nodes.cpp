/*
 * Copyright 2011-2014 Blender Foundation
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
 * limitations under the License
 */

#include "nodes.h"
#include "camera_nodes.h"
#include "svm.h"
#include "osl.h"

CCL_NAMESPACE_BEGIN

/* Path attribute */

PathAttributeNode::PathAttributeNode()
: ShaderNode("path_attribute")
{
	add_output("Raster", SHADER_SOCKET_VECTOR);
	add_output("Lens", SHADER_SOCKET_VECTOR);
	add_output("Time", SHADER_SOCKET_FLOAT);
}

void PathAttributeNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out;

	out = output("Raster");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_CAMERA_PATH_ATTRIBUTE,
		                  NODE_CAMERA_PATH_ATTRIBUTE_RASTER,
		                  out->stack_offset);
	}

	out = output("Lens");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_CAMERA_PATH_ATTRIBUTE,
		                  NODE_CAMERA_PATH_ATTRIBUTE_LENS,
		                  out->stack_offset);
	}

	out = output("Time");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_CAMERA_PATH_ATTRIBUTE,
		                  NODE_CAMERA_PATH_ATTRIBUTE_TIME,
		                  out->stack_offset);
	}
}

void PathAttributeNode::compile(OSLCompiler& compiler)
{
}

/* Sample perspective */

SamplePerspectiveNode::SamplePerspectiveNode()
: ShaderNode("sample_perspective")
{
	add_input("Raster", SHADER_SOCKET_VECTOR);
	add_input("Lens", SHADER_SOCKET_VECTOR);
	add_input("Time", SHADER_SOCKET_FLOAT);
	add_output("Ray Origin", SHADER_SOCKET_VECTOR);
	add_output("Ray Direction", SHADER_SOCKET_VECTOR);
	add_output("Ray Length", SHADER_SOCKET_FLOAT);
}

void SamplePerspectiveNode::compile(SVMCompiler& compiler)
{
	ShaderInput *raster_in = input("Raster");
	ShaderInput *lens_in = input("Lens");
	ShaderInput *time_in = input("Time");
	ShaderOutput *ray_origin_out = output("Ray Origin");
	ShaderOutput *ray_direction_out = output("Ray Direction");
	ShaderOutput *ray_length_out = output("Ray Length");

	if(raster_in->link) {
		compiler.stack_assign(raster_in);
	}
	if(lens_in->link) {
		compiler.stack_assign(lens_in);
	}
	if(time_in->link) {
		compiler.stack_assign(time_in);
	}
	if(!ray_origin_out->links.empty()) {
		compiler.stack_assign(ray_origin_out);
	}
	if(!ray_direction_out->links.empty()) {
		compiler.stack_assign(ray_direction_out);
	}
	if(!ray_length_out->links.empty()) {
		compiler.stack_assign(ray_length_out);
	}

	compiler.add_node(NODE_CAMERA_SAMPLE_PERSPECTIVE,
	                  compiler.encode_uchar4(raster_in->stack_offset,
	                                         lens_in->stack_offset,
	                                         time_in->stack_offset, 0),
	                  compiler.encode_uchar4(ray_origin_out->stack_offset,
	                                         ray_direction_out->stack_offset,
	                                         ray_length_out->stack_offset,
	                                         0));
}

void SamplePerspectiveNode::compile(OSLCompiler& compiler)
{
}

/* Polynomial distortion node. */

PolynomialDistortionNode::PolynomialDistortionNode()
 : ShaderNode("polynomial_distortion"),
   k1(0.0f), k2(0.0f), k3(0.0f),
   invert(false)
{
	add_input("Raster", SHADER_SOCKET_VECTOR);
	add_output("Raster", SHADER_SOCKET_VECTOR);
}

void PolynomialDistortionNode::compile(SVMCompiler& compiler)
{
	ShaderInput *raster_in = input("Raster");
	ShaderOutput *raster_out = output("Raster");

	compiler.stack_assign(raster_in);
	compiler.stack_assign(raster_out);

	float3 encoded_distortion = make_float3(k1, k2, k3);
	compiler.add_node(NODE_CAMERA_POLYNOMIAL_DISTORTION, encoded_distortion);
	compiler.add_node(NODE_CAMERA_POLYNOMIAL_DISTORTION,
	                  (int)invert,
	                  raster_in->stack_offset,
	                  raster_out->stack_offset);
}

void PolynomialDistortionNode::compile(OSLCompiler& compiler)
{
}

/* Camera Ray Output */

RayOutputNode::RayOutputNode()
: ShaderNode("ray_output")
{
	add_input("Ray Origin", SHADER_SOCKET_VECTOR);
	add_input("Ray Direction", SHADER_SOCKET_VECTOR);
	add_input("Ray Length", SHADER_SOCKET_FLOAT);
	add_input("Time", SHADER_SOCKET_FLOAT);
}

void RayOutputNode::compile(SVMCompiler& compiler)
{
	ShaderInput *ray_origin_in = input("Ray Origin");
	ShaderInput *ray_direction_in = input("Ray Direction");
	ShaderInput *ray_length_in = input("Ray Length");
	ShaderInput *time_in = input("Time");

	compiler.stack_assign(ray_origin_in);
	compiler.stack_assign(ray_direction_in);
	compiler.stack_assign(ray_length_in);
	compiler.stack_assign(time_in);

	compiler.add_node(NODE_CAMERA_RAY_OUTPUT,
	                  compiler.encode_uchar4(ray_origin_in->stack_offset,
	                                         ray_direction_in->stack_offset,
	                                         ray_length_in->stack_offset,
	                                         time_in->stack_offset));
}

void RayOutputNode::compile(OSLCompiler& compiler)
{
}

CCL_NAMESPACE_END
