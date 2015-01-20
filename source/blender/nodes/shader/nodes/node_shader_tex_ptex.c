/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../node_shader_util.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_tex_ptex_in[] = {
	{	SOCK_VECTOR, 1, N_("Vector"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_ptex_out[] = {
	{	SOCK_RGBA, 0, N_("Color"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
	{	SOCK_FLOAT, 0, N_("Alpha"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
	{	-1, 0, ""	}
};

static int node_shader_gpu_tex_ptex(GPUMaterial *mat, bNode *node,
									bNodeExecData *UNUSED(execdata),
									GPUNodeStack *in, GPUNodeStack *out)
{
	const NodeTexPtex *storage = node->storage;

	if (!in[0].link)
		in[0].link = GPU_attribute(CD_TESSFACE_PTEX, "");

	return GPU_stack_link(mat, "node_tex_ptex", in, out,
						  GPU_node_link_ptex(GPU_PTEX_INPUT_IMAGE,
											 storage->layer_name, NULL),
						  GPU_node_link_ptex(GPU_PTEX_INPUT_MAP,
											 storage->layer_name, NULL));
}

static void node_shader_init_tex_ptex(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeTexPtex *tex = MEM_callocN(sizeof(NodeTexPtex), "NodeTexPtex");
	node->storage = tex;
}

void register_node_type_sh_tex_ptex(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_TEX_PTEX, "Ptex Texture",
					  NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_ptex_in,
							   sh_node_tex_ptex_out);
	node_type_init(&ntype, node_shader_init_tex_ptex);
	node_type_storage(&ntype, "NodeTexPtex", node_free_standard_storage,
					  node_copy_standard_storage);
	node_type_gpu(&ntype, node_shader_gpu_tex_ptex);

	nodeRegisterType(&ntype);
}
