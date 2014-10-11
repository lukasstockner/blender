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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/nodes/node_texture_decompose.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"
#include <math.h>

static bNodeSocketTemplate inputs[] = {
	{ SOCK_RGBA, 1, N_("Color"), 0.0f, 0.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};
static bNodeSocketTemplate outputs[] = {
	{ SOCK_FLOAT, 0, N_("Red") },
	{ SOCK_FLOAT, 0, N_("Green") },
	{ SOCK_FLOAT, 0, N_("Blue") },
	{ SOCK_FLOAT, 0, N_("Alpha") },
	{ -1, 0, "" }
};

static void exec(void *UNUSED(data),
                 int UNUSED(thread),
                 bNode *UNUSED(node),
                 bNodeExecData *UNUSED(execdata),
                 bNodeStack **in,
                 bNodeStack **out)
{
	float color[4];
	tex_input_rgba(color, in[0]);
	out[0]->vec[0] = color[0];
	out[1]->vec[0] = color[1];
	out[2]->vec[0] = color[2];
	out[3]->vec[0] = color[3];
}

void register_node_type_tex_decompose(void)
{
	static bNodeType ntype;

	tex_node_type_base(&ntype, TEX_NODE_DECOMPOSE, "Separate RGBA", NODE_CLASS_OP_COLOR, 0);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_exec(&ntype, NULL, NULL, exec);

	nodeRegisterType(&ntype);
}
