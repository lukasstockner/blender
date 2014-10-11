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

/** \file blender/nodes/texture/nodes/node_texture_checker.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"
#include <math.h>

static bNodeSocketTemplate inputs[] = {
	{ SOCK_RGBA, 1, N_("Color1"), 1.0f, 0.0f, 0.0f, 1.0f },
	{ SOCK_RGBA, 1, N_("Color2"), 1.0f, 1.0f, 1.0f, 1.0f },
	{ SOCK_FLOAT, 1, N_("Size"),   0.5f, 0.0f, 0.0f, 0.0f,  0.0f, 100.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static bNodeSocketTemplate outputs[] = {
	{ SOCK_RGBA, 0, N_("Color") },
	{ -1, 0, "" }
};

static void exec(void *data,
                 int UNUSED(thread),
                 bNode *UNUSED(node),
                 bNodeExecData *execdata,
                 bNodeStack **in,
                 bNodeStack **out)
{
	TexCallData *cdata = (TexCallData *)data;
	TexParams p;
	float x, y, z, sz;
	int xi, yi, zi;

	params_from_cdata(&p, cdata);

	x  = p.co[0];
	y  = p.co[1];
	z  = p.co[2];
	sz = tex_input_value(in[2]);

	/* 0.00001  because of unit sized stuff */
	xi = (int)fabsf(floorf(0.00001f + x / sz));
	yi = (int)fabsf(floorf(0.00001f + y / sz));
	zi = (int)fabsf(floorf(0.00001f + z / sz));

	if ( (xi % 2 == yi % 2) == (zi % 2) ) {
		tex_input_rgba(out[0]->vec, in[0]);
	}
	else {
		tex_input_rgba(out[0]->vec, in[1]);
	}

	tex_do_preview(execdata->preview, p.co, out[0]->vec, cdata->do_manage);
}

void register_node_type_tex_checker(void)
{
	static bNodeType ntype;
	
	tex_node_type_base(&ntype, TEX_NODE_CHECKER, "Checker", NODE_CLASS_PATTERN, NODE_PREVIEW);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_exec(&ntype, NULL, NULL, exec);
	
	nodeRegisterType(&ntype);
}
