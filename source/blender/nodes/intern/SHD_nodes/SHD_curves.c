/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "../SHD_util.h"


/* **************** CURVE VEC  ******************** */
static bNodeSocketType sh_node_curve_vec_in[]= {
	{	SOCK_VECTOR, 1, "Vector",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_curve_vec_out[]= {
	{	SOCK_VECTOR, 0, "Vector",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_curve_vec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	float vec[3];
	
	/* stack order input:  vec */
	/* stack order output: vec */
	nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	curvemapping_evaluate3F(node->storage, out[0]->vec, vec);
}

static void node_shader_init_curve_vec(bNode* node)
{
   node->storage= curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
}

static GPUNodeLink *gpu_curvemapping_texture(CurveMapping *cumap)
{
	GPUNodeLink *link;
	float *pixels;
	int a, size = CM_TABLE+1;

	pixels = MEM_mallocN(sizeof(float)*size*4, "GPUCurveMap");
	curvemapping_initialize(cumap);

	for(a=0; a<size; a++) {
		pixels[a*4+0]= cumap->cm[0].table[a].y;
		pixels[a*4+1]= cumap->cm[1].table[a].y;
		pixels[a*4+2]= cumap->cm[2].table[a].y;
		if(cumap->cm[3].table)
			pixels[a*4+3]= cumap->cm[3].table[a].y;
		else
			pixels[a*4+3]= 1.0f;
	}

	link = GPU_texture(size, pixels);

	return link;
}

static GPUNode *gpu_shader_curve_vec(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	GPUNodeLink *tex = gpu_curvemapping_texture(node->storage);
	return GPU_stack_link(mat, "curves_vec", in, out, tex);
}

bNodeType sh_node_curve_vec= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_CURVE_VEC,
	/* name        */	"Vector Curves",
	/* width+range */	200, 140, 320,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	sh_node_curve_vec_in,
	/* output sock */	sh_node_curve_vec_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_shader_exec_curve_vec,
	/* butfunc     */	NULL,
	/* initfunc    */	node_shader_init_curve_vec,
	/* freestoragefunc    */	node_free_curves,
	/* copystoragefunc    */	node_copy_curves,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_curve_vec
	
};

/* **************** CURVE RGB  ******************** */
static bNodeSocketType sh_node_curve_rgb_in[]= {
	{	SOCK_RGBA, 1, "Color",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_curve_rgb_out[]= {
	{	SOCK_RGBA, 0, "Color",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_curve_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	float vec[3];
	
	/* stack order input:  vec */
	/* stack order output: vec */
	nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	curvemapping_evaluateRGBF(node->storage, out[0]->vec, vec);
}

static void node_shader_init_curve_rgb(bNode *node)
{
   node->storage= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
}

static GPUNode *gpu_shader_curve_rgb(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	GPUNodeLink *tex = gpu_curvemapping_texture(node->storage);

	return GPU_stack_link(mat, "curves_rgb", in, out, tex);
}

bNodeType sh_node_curve_rgb= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_CURVE_RGB,
	/* name        */	"RGB Curves",
	/* width+range */	200, 140, 320,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	sh_node_curve_rgb_in,
	/* output sock */	sh_node_curve_rgb_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_shader_exec_curve_rgb,
	/* butfunc     */ 	NULL,
	/* initfunc    */   node_shader_init_curve_rgb,
	/* freestoragefunc    */	node_free_curves,
	/* copystoragefunc    */	node_copy_curves,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_curve_rgb
};

