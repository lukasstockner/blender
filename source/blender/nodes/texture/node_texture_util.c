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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/node_texture_util.c
 *  \ingroup nodes
 */


/*
 * HOW TEXTURE NODES WORK
 *
 * In contrast to Shader nodes, which place a color into the output
 * stack when executed, Texture nodes place a TexDelegate* there. To
 * obtain a color value from this, a node further up the chain reads
 * the TexDelegate* from its input stack, and uses tex_call_delegate to
 * retrieve the color from the delegate.
 *
 * comments: (ton)
 *
 * This system needs recode, a node system should rely on the stack, and
 * callbacks for nodes only should evaluate own node, not recursively go
 * over other previous ones.
 */

#include <assert.h>
#include "node_texture_util.h"


int tex_node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *ntree)
{
	return STREQ(ntree->idname, "TextureNodeTree");
}

void tex_node_type_base(struct bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
	node_type_base(ntype, type, name, nclass, flag);
	
	ntype->poll = tex_node_poll_default;
	ntype->update_internal_links = node_update_internal_links_default;
}

/* TODO(sergey): De-duplicate with the shader nodes. */
static void tex_input(float *in, int type_in, bNodeStack *ns)
{
	const float *from = ns->vec;

	if (type_in == SOCK_FLOAT) {
		if (ns->sockettype == SOCK_FLOAT)
			*in = *from;
		else
			*in = (from[0] + from[1] + from[2]) / 3.0f;
	}
	else if (type_in == SOCK_VECTOR) {
		if (ns->sockettype == SOCK_FLOAT) {
			in[0] = from[0];
			in[1] = from[0];
			in[2] = from[0];
		}
		else {
			copy_v3_v3(in, from);
		}
	}
	else { /* type_in==SOCK_RGBA */
		if (ns->sockettype == SOCK_RGBA) {
			copy_v4_v4(in, from);
		}
		else if (ns->sockettype == SOCK_FLOAT) {
			in[0] = from[0];
			in[1] = from[0];
			in[2] = from[0];
			in[3] = 1.0f;
		}
		else {
			copy_v3_v3(in, from);
			in[3] = 1.0f;
		}
	}
}

void tex_input_vec(float *out, bNodeStack *in)
{
	tex_input(out, SOCK_FLOAT, in);
}

void tex_input_rgba(float *out, bNodeStack *in)
{
	tex_input(out, SOCK_RGBA, in);
	
	if (in->hasoutput && in->sockettype == SOCK_FLOAT) {
		out[1] = out[2] = out[0];
		out[3] = 1;
	}
	
	if (in->hasoutput && in->sockettype == SOCK_VECTOR) {
		out[0] = out[0] * 0.5f + 0.5f;
		out[1] = out[1] * 0.5f + 0.5f;
		out[2] = out[2] * 0.5f + 0.5f;
		out[3] = 1;
	}
}

float tex_input_value(bNodeStack *in)
{
	float out[4];
	tex_input_vec(out, in);
	return out[0];
}

void params_from_cdata(TexParams *out, TexCallData *in)
{
	out->co = in->co;
	out->dxt = in->dxt;
	out->dyt = in->dyt;
	out->previewco = in->co;
	out->osatex = in->osatex;
	out->cfra = in->cfra;
	out->shi = in->shi;
	out->mtex = in->mtex;
}

void tex_do_preview(bNodePreview *preview, const float coord[2], const float col[4], bool do_manage)
{
	if (preview) {
		int xs = ((coord[0] + 1.0f) * 0.5f) * preview->xsize;
		int ys = ((coord[1] + 1.0f) * 0.5f) * preview->ysize;
		
		BKE_node_preview_set_pixel(preview, col, xs, ys, do_manage);
	}
}

void tex_output(bNode *node, bNodeExecData *execdata, bNodeStack **in, bNodeStack *out, TexFn texfn, TexCallData *cdata)
{
	(void) node;
	(void) execdata;
	(void) in;
	(void) out;
	(void) texfn;
	(void) cdata;
}

void ntreeTexCheckCyclics(struct bNodeTree *ntree)
{
	bNode *node;
	for (node = ntree->nodes.first; node; node = node->next) {

		if (node->type == TEX_NODE_TEXTURE && node->id) {
			/* custom2 stops the node from rendering */
			if (node->custom1) {
				node->custom2 = 1;
				node->custom1 = 0;
			}
			else {
				Tex *tex = (Tex *)node->id;
				
				node->custom2 = 0;
			
				node->custom1 = 1;
				if (tex->use_nodes && tex->nodetree) {
					ntreeTexCheckCyclics(tex->nodetree);
				}
				node->custom1 = 0;
			}
		}

	}
}
