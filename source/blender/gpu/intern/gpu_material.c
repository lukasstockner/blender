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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_listBase.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

#include "GPU_node.h"
#include "GPU_material.h"

#include "gpu_nodes.h"
#include "gpu_codegen.h"

#include <string.h>

struct GPUMaterial {
	ListBase nodes;
	ListBase passes;
	GPUNodeBuf *outbuf;
	GPUVertexAttribs attribs;
};

/* Functions */

GPUMaterial *GPU_material_construct_begin()
{
	GPUMaterial *material = MEM_callocN(sizeof(GPUMaterial), "GPUMaterial");

	return material;
}

static void gpu_material_create_vertex_attributes(GPUMaterial *material)
{
	GPUNode *node;
	GPUInput *input;
	GPUVertexAttribs *attribs;
	int a;

	/* convert attributes requested by node inputs to an array of layers,
	 * checking for duplicates and assigning id's starting from zero. */

	attribs = &material->attribs;
	memset(attribs, 0, sizeof(*attribs));

	for(node=material->nodes.first; node; node=node->next) {
		for(input=node->inputs.first; input; input=input->next) {
			if(input->samp == GPU_S_ATTRIB) {
				for(a=0; a<attribs->totlayer; a++) {
					if(attribs->layer[a].type == input->attribtype &&
						strcmp(attribs->layer[a].name, input->attribname) == 0)
						break;
				}

				if(a == attribs->totlayer && a < GPU_MAX_ATTRIB) {
					input->attribid = attribs->totlayer++;
					input->attribfirst = 1;

					attribs->layer[a].type = input->attribtype;
					attribs->layer[a].glindex = input->attribid;
					BLI_strncpy(attribs->layer[a].name, input->attribname,
						sizeof(attribs->layer[a].name));
				}
				else
					input->attribid = attribs->layer[a].glindex;
			}
		}
	}
}

static void gpu_material_set_attrib_id(GPUMaterial *material)
{
	GPUVertexAttribs *attribs;
	GPUShader *shader;
	GPUPass *pass;
	char name[32];
	int a, b;

	attribs= &material->attribs;

	pass= material->passes.last;
	if(!pass) {
		attribs->totlayer = 0;
		return;
	}
	
	shader= pass->shader;
	if(!shader) {
		attribs->totlayer = 0;
		return;
	}

	/* convert from attribute number to the actual id assigned by opengl,
	 * in case the attrib does not get a valid index back, it was probably
	 * removed by the glsl compiler by dead code elimination */

	for(a=0, b=0; a<attribs->totlayer; a++) {
		sprintf(name, "att%d", attribs->layer[a].glindex);
		attribs->layer[a].glindex = GPU_shader_get_attribute(shader, name);
		
		if(attribs->layer[a].glindex >= 0) {
			attribs->layer[b] = attribs->layer[a];
			b++;
		}
	}

	attribs->totlayer = b;
}

int GPU_material_construct_end(GPUMaterial *material)
{
	if (material->outbuf) {
		ListBase passes;
		GPUNodeBuf *outbuf;

		gpu_material_create_vertex_attributes(material);

		outbuf = material->outbuf;
		passes = GPU_generate_single_pass(&material->nodes, outbuf->source, 1);
		material->passes = passes;

		if(!passes.first)
			return 0;

		gpu_material_set_attrib_id(material);
		return 1;
	}

	return 0;
}

void GPU_material_free(GPUMaterial *material)
{
	while (material->passes.first)
		GPU_pass_free(&material->passes, material->passes.first);

	GPU_nodes_free(&material->nodes);
	MEM_freeN(material);
}

void GPU_material_bind(GPUMaterial *material)
{
	if (material->passes.last)
		GPU_pass_bind(material->passes.last);
}

void GPU_material_unbind(GPUMaterial *material)
{
	if (material->passes.last)
		GPU_pass_unbind(material->passes.last);
}

void GPU_material_vertex_attributes(GPUMaterial *material, GPUVertexAttribs *attribs)
{
	*attribs = material->attribs;
}

/* Nodes */

GPUNode *GPU_mat_node_create(GPUMaterial *material, char *name, GPUNodeStack *in, GPUNodeStack *out)
{
	GPUNode *node;
	int i;
	
	node = GPU_node_begin(name, 0, 0);

	if (in)
		for (i = 0; in[i].type != GPU_NONE; i++)
			GPU_node_input(node, in[i].type, in[i].name, in[i].vec, in[i].buf);
	
	if (out)
		for (i = 0; out[i].type != GPU_NONE; i++)
			GPU_node_output(node, out[i].type, out[i].name, &out[i].buf);

	if (strcmp(node->name, "output") == 0) {
		if (!material->outbuf && node->inputs.first) {
			GPUInput *inp = (GPUInput*)node->inputs.first;
			material->outbuf = inp->buf;
		}
		GPU_node_free(node);
		node = NULL;
	}
	else
		BLI_addtail(&material->nodes, node);
	
	return node;
}

void GPU_mat_node_uniform(GPUNode *node, GPUType type, void *ptr)
{
	GPU_node_input(node, type, "", ptr, NULL);
}

void GPU_mat_node_texture(GPUNode *node, GPUType type, int size, float *pixels)
{
	GPU_node_input(node, type, "", &size, pixels);
}

void GPU_mat_node_image(GPUNode *node, GPUType type, Image *ima, ImageUser *iuser)
{
	GPU_node_input(node, type, "", ima, iuser);
}

void GPU_mat_node_attribute(GPUNode *node, GPUType type, int laytype, char *name)
{
	GPU_node_input_array(node, GPU_ATTRIB, "", &type, &laytype, name);
}

void GPU_mat_node_socket(GPUNode *node, GPUNodeStack *sock)
{
	GPU_node_input(node, sock->type, sock->name, sock->vec, sock->buf);
}

void GPU_mat_node_output(GPUNode *node, GPUType type, char *name, GPUNodeStack *out)
{
	memset(out, 0, sizeof(*out));

	out->type = type;
	out->name = name;
	GPU_node_output(node, out->type, out->name, &out->buf);
}

/* Code generation */

static GPUNodeBuf *texture_rgb_blend(GPUMaterial *mat, GPUNodeBuf *texbuf, GPUNodeBuf *outbuf, GPUNodeBuf *factbuf, GPUNodeBuf *facgbuf, int blendtype)
{
	GPUNodeBuf *inbuf;
	GPUNode *node;

	if(blendtype == MTEX_BLEND)
		node = GPU_mat_node_create(mat, "mtex_blend", NULL, NULL);
	else
		node = GPU_mat_node_create(mat, "mtex_blend", NULL, NULL);

	GPU_node_input(node, GPU_VEC3, "out", NULL, outbuf);
	GPU_node_input(node, GPU_VEC3, "tex", NULL, texbuf);
	GPU_node_input(node, GPU_FLOAT, "fact", NULL, factbuf);
	GPU_node_input(node, GPU_FLOAT, "facg", NULL, facgbuf);

	GPU_node_output(node, GPU_VEC3, "in", &inbuf);

	return inbuf;
}

static void material_tex_nodes_create(GPUMaterial *mat, Material *ma, GPUNodeBuf **colbuf_r, GPUNodeBuf **specbuf_r, GPUNodeBuf **norbuf_r)
{
	MTex *mtex;
	Tex *tex;
	GPUNodeBuf *texcobuf, *tinbuf, *trgbbuf, *tnorbuf, *stencilbuf = NULL;
	GPUNodeBuf *colbuf, *specbuf, *colfacbuf, *norbuf, *newnorbuf;
	GPUNode *node;
	float col[4], spec[4];
	int tex_nr;

	VECCOPY(col, &ma->r);
	col[3]= 1.0f;
	VECCOPY(spec, &ma->specr);
	spec[3]= 1.0f;

	node = GPU_mat_node_create(mat, "setrgb", NULL, NULL);
	GPU_mat_node_uniform(node, GPU_VEC4, col);
	GPU_node_output(node, GPU_VEC4, "col", &colbuf);

	node = GPU_mat_node_create(mat, "setrgb", NULL, NULL);
	GPU_mat_node_uniform(node, GPU_VEC4, spec);
	GPU_node_output(node, GPU_VEC4, "spec", &specbuf);

	node = GPU_mat_node_create(mat, "texco_norm", NULL, NULL);
	GPU_node_output(node, GPU_VEC3, "texco", &norbuf);

	for(tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		/* separate tex switching */
		if(ma->septex & (1<<tex_nr)) continue;
		
		if(ma->mtex[tex_nr]) {
			mtex= ma->mtex[tex_nr];
			
			tex= mtex->tex;
			if(tex==0) continue;

			/* which coords */
			if(mtex->texco==TEXCO_ORCO) {
				node = GPU_mat_node_create(mat, "texco_orco", NULL, NULL);
				GPU_mat_node_attribute(node, GPU_VEC3, CD_ORCO, "");
				GPU_node_output(node, GPU_VEC3, "texco", &texcobuf);
			}
			else if(mtex->texco==TEXCO_NORM) {
				node = GPU_mat_node_create(mat, "texco_norm", NULL, NULL);
				GPU_node_output(node, GPU_VEC3, "texco", &texcobuf);
			}
			else if(mtex->texco==TEXCO_UV) {
				node = GPU_mat_node_create(mat, "texco_uv", NULL, NULL);
				GPU_mat_node_attribute(node, GPU_VEC2, CD_MTFACE, mtex->uvname);
				GPU_node_output(node, GPU_VEC3, "texco", &texcobuf);
			}
			else continue;

			node = GPU_mat_node_create(mat, "mtex_2d_mapping", NULL, NULL);
			GPU_node_input(node, GPU_VEC3, "texco", NULL, texcobuf);
			GPU_node_output(node, GPU_VEC3, "texco", &texcobuf);

			node = GPU_mat_node_create(mat, "mtex_mapping", NULL, NULL);
			GPU_node_input(node, GPU_VEC3, "texco", NULL, texcobuf);
			GPU_mat_node_uniform(node, GPU_VEC3, mtex->size);
			GPU_mat_node_uniform(node, GPU_VEC3, mtex->ofs);
			GPU_node_output(node, GPU_VEC3, "texco", &texcobuf);

			if(tex && tex->type == TEX_IMAGE && tex->ima) {
				node = GPU_mat_node_create(mat, "mtex_image", NULL, NULL);
				GPU_node_input(node, GPU_VEC3, "texco", NULL, texcobuf);
				GPU_mat_node_image(node, GPU_TEX2D, tex->ima, NULL);

				GPU_node_output(node, GPU_FLOAT, "tin", &tinbuf);
				GPU_node_output(node, GPU_VEC4, "trgb", &trgbbuf);
				GPU_node_output(node, GPU_VEC3, "tnor", &tnorbuf);
		    }
			else continue;

			/* texture output */

			if(mtex->texflag & MTEX_RGBTOINT) {
				node = GPU_mat_node_create(mat, "mtex_rgbtoint", NULL, NULL);
				GPU_node_input(node, GPU_VEC4, "trgb", NULL, trgbbuf);
				GPU_node_output(node, GPU_FLOAT, "tin", &tinbuf);

			}
			if(mtex->texflag & MTEX_NEGATIVE) {
				node = GPU_mat_node_create(mat, "mtex_invert", NULL, NULL);
				GPU_node_input(node, GPU_VEC4, "trgb", NULL, trgbbuf);
				GPU_node_output(node, GPU_VEC4, "trgb", &trgbbuf);
			}

			if(mtex->texflag & MTEX_STENCIL) {
				if(!stencilbuf) {
					node = GPU_mat_node_create(mat, "setvalue", NULL, NULL);
					GPU_node_input(node, GPU_FLOAT, "tin", NULL, tinbuf);
					GPU_node_output(node, GPU_FLOAT, "stencil", &stencilbuf);
				}
				else {
					node = GPU_mat_node_create(mat, "math_multiply", NULL, NULL);
					GPU_node_input(node, GPU_FLOAT, "tin", NULL, tinbuf);
					GPU_node_input(node, GPU_FLOAT, "stencil", NULL, stencilbuf);
					GPU_node_output(node, GPU_FLOAT, "stencil", &stencilbuf);
				}
			}

			/* mapping */
			if(mtex->mapto & (MAP_COL+MAP_COLSPEC)) {
				/* stencil maps on the texture control slider, not texture intensity value */
				if(stencilbuf) {
					node = GPU_mat_node_create(mat, "math_multiply", NULL, NULL);
					GPU_mat_node_uniform(node, GPU_FLOAT, &mtex->colfac);
					GPU_node_input(node, GPU_FLOAT, "stencil", NULL, stencilbuf);
					GPU_node_output(node, GPU_FLOAT, "colfac", &colfacbuf);
				}
				else {
					node = GPU_mat_node_create(mat, "setvalue", NULL, NULL);
					GPU_mat_node_uniform(node, GPU_FLOAT, &mtex->colfac);
					GPU_node_output(node, GPU_FLOAT, "colfac", &colfacbuf);
				}
				
				if(mtex->mapto & MAP_COL)
					colbuf = texture_rgb_blend(mat, trgbbuf, colbuf, tinbuf, colfacbuf, mtex->blendtype);
				if(mtex->mapto & MAP_COLSPEC)
					specbuf = texture_rgb_blend(mat, trgbbuf, specbuf, tinbuf, colfacbuf, mtex->blendtype);
			}

			if(mtex->mapto & MAP_NORM) {
				if(mtex->maptoneg & MAP_NORM) tex->norfac= -mtex->norfac;
				else tex->norfac= mtex->norfac;
				
				if((tex->type==TEX_IMAGE) && (tex->imaflag & TEX_NORMALMAP)) {
					tex->norfac = mtex->norfac;

					if(mtex->normapspace == MTEX_NSPACE_TANGENT) {

						node = GPU_mat_node_create(mat, "mtex_nspace_tangent", NULL, NULL);
						GPU_mat_node_attribute(node, GPU_VEC3, CD_TANGENT, "");
						GPU_node_input(node, GPU_VEC3, "nor", NULL, norbuf);
						GPU_node_input(node, GPU_VEC3, "tnor", NULL, tnorbuf);
						GPU_node_output(node, GPU_VEC3, "nor", &newnorbuf);
					}
					else {
						newnorbuf = tnorbuf;
					}

					node = GPU_mat_node_create(mat, "mtex_blend_normal", NULL, NULL);
					GPU_mat_node_uniform(node, GPU_FLOAT, &mtex->norfac);
					GPU_node_input(node, GPU_VEC3, "nor", NULL, norbuf);
					GPU_node_input(node, GPU_VEC3, "newnor", NULL, newnorbuf);
					GPU_node_output(node, GPU_VEC3, "nor", &norbuf);
				}
			}
		}
	}

	*colbuf_r= colbuf;
	*specbuf_r= specbuf;
	*norbuf_r = norbuf;
}

static void material_nodes_create(GPUMaterial *mat, Material *ma)
{
	GPUNode *node;
	GPUNodeBuf *colbuf, *specbuf, *norbuf, *combinedbuf;
	float hard;

	material_tex_nodes_create(mat, ma, &colbuf, &specbuf, &norbuf);

	if(ma->mode & MA_SHLESS) {
		mat->outbuf = colbuf;
	}
	else {
		node = GPU_mat_node_create(mat, "material_simple", NULL, NULL);
		GPU_node_input(node, GPU_VEC4, "col", NULL, colbuf);
		GPU_mat_node_uniform(node, GPU_FLOAT, &ma->ref);
		GPU_node_input(node, GPU_VEC4, "spec", NULL, specbuf);
		GPU_mat_node_uniform(node, GPU_FLOAT, &ma->spec);
		hard= ma->har;
		GPU_mat_node_uniform(node, GPU_FLOAT, &hard);
		GPU_node_input(node, GPU_VEC3, "nor", NULL, norbuf);
		GPU_node_output(node, GPU_VEC4, "combined", &combinedbuf);

		mat->outbuf = combinedbuf;
	}
}

GPUMaterial *GPU_material_from_blender(Material *ma)
{
	GPUMaterial *mat;

	mat = GPU_material_construct_begin();

	material_nodes_create(mat, ma);

	if(!GPU_material_construct_end(mat)) {
		GPU_material_free(mat);
		mat= NULL;
	}

	return mat;
}

