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

#include "camera.h"
#include "graph.h"
#include "nodes.h"
#include "camera_nodes.h"
#include "scene.h"

#include "blender_sync.h"
#include "blender_util.h"

CCL_NAMESPACE_BEGIN

typedef map<void*, ShaderInput*> PtrInputMap;
typedef map<void*, ShaderOutput*> PtrOutputMap;

static void set_default_value(ShaderInput *input, BL::Node b_node, BL::NodeSocket b_sock, BL::BlendData b_data, BL::ID b_id)
{
	/* copy values for non linked inputs */
	switch(input->type) {
	case SHADER_SOCKET_FLOAT: {
		input->set(get_float(b_sock.ptr, "default_value"));
		break;
	}
	case SHADER_SOCKET_INT: {
		input->set((float)get_int(b_sock.ptr, "default_value"));
		break;
	}
	case SHADER_SOCKET_COLOR: {
		input->set(float4_to_float3(get_float4(b_sock.ptr, "default_value")));
		break;
	}
	case SHADER_SOCKET_NORMAL:
	case SHADER_SOCKET_POINT:
	case SHADER_SOCKET_VECTOR: {
		input->set(get_float3(b_sock.ptr, "default_value"));
		break;
	}
	case SHADER_SOCKET_STRING: {
		input->set((ustring)blender_absolute_path(b_data, b_id, get_string(b_sock.ptr, "default_value")));
		break;
	}

	case SHADER_SOCKET_CLOSURE:
	case SHADER_SOCKET_UNDEFINED:
		break;
	}
}

static bool node_use_modified_socket_name(ShaderNode *node)
{
	if (node->special_type == SHADER_SPECIAL_TYPE_SCRIPT)
		return false;

	return true;
}

static ShaderInput *node_find_input_by_name(ShaderNode *node, BL::Node b_node, BL::NodeSocket b_socket)
{
	string name = b_socket.name();

	if (node_use_modified_socket_name(node)) {
		BL::Node::inputs_iterator b_input;
		bool found = false;
		int counter = 0, total = 0;

		for (b_node.inputs.begin(b_input); b_input != b_node.inputs.end(); ++b_input) {
			if (b_input->name() == name) {
				if (!found)
					counter++;
				total++;
			}

			if(b_input->ptr.data == b_socket.ptr.data)
				found = true;
		}

		/* rename if needed */
		if (name == "Shader")
			name = "Closure";

		if (total > 1)
			name = string_printf("%s%d", name.c_str(), counter);
	}

	return node->input(name.c_str());
}

static ShaderOutput *node_find_output_by_name(ShaderNode *node, BL::Node b_node, BL::NodeSocket b_socket)
{
	string name = b_socket.name();

	if (node_use_modified_socket_name(node)) {
		BL::Node::outputs_iterator b_output;
		bool found = false;
		int counter = 0, total = 0;

		for (b_node.outputs.begin(b_output); b_output != b_node.outputs.end(); ++b_output) {
			if (b_output->name() == name) {
				if (!found)
					counter++;
				total++;
			}

			if(b_output->ptr.data == b_socket.ptr.data)
				found = true;
		}

		/* rename if needed */
		if (name == "Shader")
			name = "Closure";

		if (total > 1)
			name = string_printf("%s%d", name.c_str(), counter);
	}

	return node->output(name.c_str());
}

static void add_nodes(BL::BlendData b_data, BL::NodeTree b_ntree, CameraNodesGraph *graph)
{
	PtrInputMap input_map;
	PtrOutputMap output_map;

	/* Synchronize nodes themselves. */
	BL::NodeTree::nodes_iterator b_node;
	for(b_ntree.nodes.begin(b_node); b_node != b_ntree.nodes.end(); ++b_node) {
		ShaderNode *node = NULL;
		string type = b_node->bl_idname();
		/* TODO(sergey): Support node groups. */
		if(type == "PathAttributeNodeType") {
			node = new PathAttributeNode();
		}
		else if(type == "CameraSamplePerspectiveNodeType") {
			node = new SamplePerspectiveNode();
		}
		else if(type == "PolynomialDistortionNodeType") {
			PolynomialDistortionNode *distortion_node;
			node = distortion_node = new PolynomialDistortionNode();

			distortion_node->k1 = get_float(b_node->ptr, "k1");
			distortion_node->k2 = get_float(b_node->ptr, "k2");
			distortion_node->k3 = get_float(b_node->ptr, "k3");
			distortion_node->invert = get_enum(b_node->ptr, "mode") == 1;
		}
		else if(type == "CameraRayOutputNodeType") {
			node = graph->output();
		}
		else {
			printf("Uknown camera node type: %s\n", type.c_str());
			/* Unknown node type, could happen when opening newer file in older
			 * blender or the node has been deprecated/removed.
			 */
		}

		if(node != NULL) {
			if(node != graph->output())
				graph->add(node);

			/* Map node sockets for linking */
			BL::Node::inputs_iterator b_input;
			for(b_node->inputs.begin(b_input);
			    b_input != b_node->inputs.end();
			    ++b_input)
			{
				ShaderInput *input = node_find_input_by_name(node,
				                                             *b_node,
				                                             *b_input);
				if(!input) {
					/* XXX should not happen, report error? */
					continue;
				}
				input_map[b_input->ptr.data] = input;

				set_default_value(input, *b_node, *b_input, b_data, b_ntree);
			}

			BL::Node::outputs_iterator b_output;
			for(b_node->outputs.begin(b_output);
			    b_output != b_node->outputs.end();
			    ++b_output)
			{
				ShaderOutput *output = node_find_output_by_name(node,
				                                                *b_node,
				                                                *b_output);
				if(!output) {
					/* XXX should not happen, report error? */
					continue;
				}
				output_map[b_output->ptr.data] = output;
			}

		}
	}

	/* Connect nodes with noodles. */
	BL::NodeTree::links_iterator b_link;
	for(b_ntree.links.begin(b_link); b_link != b_ntree.links.end(); ++b_link) {
		BL::NodeSocket b_from_sock = b_link->from_socket();
		BL::NodeSocket b_to_sock = b_link->to_socket();

		ShaderOutput *output = 0;
		ShaderInput *input = 0;

		PtrOutputMap::iterator output_it = output_map.find(b_from_sock.ptr.data);
		if(output_it != output_map.end())
			output = output_it->second;
		PtrInputMap::iterator input_it = input_map.find(b_to_sock.ptr.data);
		if(input_it != input_map.end())
			input = input_it->second;

		/* Either node may be NULL when the node was not exported, typically
		 * because the node type is not supported */
		if(output && input)
			graph->connect(output, input);
	}
}

void BlenderSync::sync_camera_nodes(BL::Object b_ob)
{
	if(!b_ob) {
		return;
	}

	BL::ID b_ob_data = b_ob.data();
	assert(b_ob_data.is_a(&RNA_Camera));

	BL::Camera b_camera(b_ob_data);
	PointerRNA ccamera = RNA_pointer_get(&b_ob_data.ptr, "cycles");
	string nodes_tree_name = get_string(ccamera, "nodes");

	if(nodes_tree_name != "") {
		BL::NodeTree b_ntree = b_data.node_groups[nodes_tree_name];
		if (b_ntree) {
			progress.set_status("Synchronizing camera nodes", b_ntree.name());
			CameraNodesGraph *graph = new CameraNodesGraph();
			add_nodes(b_data, b_ntree, graph);
			scene->camera->set_graph(graph);
			progress.set_status("");
		}
	}
}

void BlenderSync::sync_view_nodes(BL::RegionView3D b_rv3d)
{
	if(b_rv3d.view_perspective() ==
	   BL::RegionView3D::view_perspective_CAMERA)
	{
		sync_camera_nodes(b_scene.camera());
	}
}

CCL_NAMESPACE_END
