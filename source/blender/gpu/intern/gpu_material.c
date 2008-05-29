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
#include "DNA_lamp_types.h"
#include "DNA_listBase.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
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
	int alpha;
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

void GPU_material_bind(Object *ob, GPUMaterial *material)
{
	if (material->passes.last) {
		GPUPass *pass = (GPUPass*)material->passes.last;
		GPUShader *shader = pass->shader;

		GPU_pass_bind(pass);

		GPU_shader_uniform_vector(shader, "unfobmat", 16, 1, (float*)ob->obmat);
		if(G.vd) /* hack */
			GPU_shader_uniform_vector(shader, "unfviewmat", 16, 1, (float*)G.vd->viewmat);

		if(material->alpha) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		}
	}
}

void GPU_material_unbind(GPUMaterial *material)
{
	if (material->passes.last) {
		GPUPass *pass = (GPUPass*)material->passes.last;

		if(material->alpha)
			glDisable(GL_BLEND);

		GPU_pass_unbind(pass);
	}
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

#if 0
static void declare(char *name, ...)
{
	char *c;

}

static void call(char *name, ...)
{
	va_list params;
	int i;

	va_start(params, totparam)
	for(i=0; i<totparam; i++) {
	}
	va_end(params);

	int i;
	double val;
	printf ("Floats passed: ");
	va_list vl;
	va_start(vl,amount);
	for (i=0;i<amount;i++)
	{
		val=va_arg(vl,double);
		printf ("\t%.2f",val);
	}
	va_end(vl);
	printf ("\n");
}

#include <stdarg.h>

static int gpu_str_prefix(char *str, char *prefix)
{
	while(*str && *prefix) {
		if(*str != *prefix)
			return 0;

		str++;
		prefix++;
	}
	
	return (*str == *prefix);
}

static char *gpu_str_skip(char *str)
{
	/* skip a variable/function name */
	while(*str) {
		if(ELEM6(*str, ' ', '(', ')', ',', '\t', '\n'))
			break;
		else
			str++;
	}

	/* skip the next special characters */
	while(*str) {
		if(ELEM6(*str, ' ', '(', ')', ',', '\t', '\n'))
			str++;
		else
			break;
	}

	return str;
}

static char* GPU_DATATYPE_STR[17] = {"", "float", "vec2", "vec3", "vec4",
	0, 0, 0, 0, "mat3", 0, 0, 0, 0, 0, 0, "mat4"};

#define decl(function, contents) GPU_declare(mat, #function, #contents);
static void GPU_declare(GPUMaterial *mat, char *function, char *contents)
{
	int a, totparam, params[256];

	/* skip void and function name */
	if(!gpu_str_prefix(function, "void "))
		return;
	
	function = gpu_str_skip(function);
	function = gpu_str_skip(function);

	/* get parameters */
	totparam= 0;

	while(*function) {
		for(a=1; a<=16; a++) {
			if(gpu_str_prefix(function, "out "))
				function = gpu_str_skip(function);
			else if(gpu_str_prefix(function, "in "))
				function = gpu_str_skip(function);

			if(gpu_str_prefix(function, GPU_DATATYPE_STR[a])) {
				params[totparam]= a;
				totparam++;

				function = gpu_str_skip(function);
				function = gpu_str_skip(function);
				continue;
			}
		}
		
		break;
	}
}

static void call(GPUMaterial *mat, char *name, ...)
{
	GPUNode *node;
	va_list params;
	int i, totparam = 0;

	node = GPU_mat_node_create(mat, name, NULL, NULL);

	va_start(params, name);
	for(i=0; i<totparam; i++) {
	}
	va_end(params);
}
#endif

/* Code generation */

#if 0
static void material_lights(GPUMaterial *mat, Material *ma, GPUNodeBuf *col, GPUNodeBuf *spec, GPUNodeBuf *nor)
{
	Base *base;
	Object *lampob;
	Lamp *la;
	GPUNode *node;
	GPUNodeBuf *lv, *dist, *visifac, *outcol;
	float hard;
	
	for(base=G.scene->base.first; base; base=base->next) {
		if(base->object->type==OB_LAMP) {
			if(G.vd && base->lay & G.vd->lay) {
				lampob= base->object;
				la= lampob->data;

				/* lamp_visibility */
				if(la->type==LA_SUN || la->type==LA_HEMI)
					node = GPU_mat_node_create(mat, "lamp_visibility_sun_hemi", NULL, NULL);
				else
					node = GPU_mat_node_create(mat, "lamp_visibility_other", NULL, NULL);

				GPU_mat_node_uniform(node, GPU_VEC4, lampob->obmat[3]);
				GPU_mat_node_uniform(node, GPU_VEC4, lampob->obmat[2]);
				GPU_node_output(node, GPU_VEC3, "lv", &lv);
				GPU_node_output(node, GPU_FLOAT, "dist", &dist);
				GPU_node_output(node, GPU_FLOAT, "visifac", &visifac);

				/* shade_one_light */
				node = GPU_mat_node_create(mat, "shade_one_light", NULL, NULL);
				GPU_node_input(node, GPU_VEC4, "col", NULL, col);
				GPU_mat_node_uniform(node, GPU_FLOAT, &ma->ref);
				GPU_node_input(node, GPU_VEC4, "spec", NULL, spec);
				GPU_mat_node_uniform(node, GPU_FLOAT, &ma->spec);
				hard= ma->har;
				GPU_mat_node_uniform(node, GPU_FLOAT, &hard);
				GPU_node_input(node, GPU_VEC3, "nor", NULL, nor);
				GPU_node_input(node, GPU_VEC3, "lv", NULL, lv);
				GPU_node_input(node, GPU_FLOAT, "visifac", NULL, visifac);
				GPU_node_output(node, GPU_VEC4, "outcol", &outcol);

				/* shade_one_light */
				node = GPU_mat_node_create(mat, "shade_add", NULL, NULL);
				GPU_node_input(node, GPU_VEC4, "col1", NULL, mat->outbuf);
				GPU_node_input(node, GPU_VEC4, "col2", NULL, outcol);
				GPU_node_output(node, GPU_VEC4, "outcol", &mat->outbuf);
			}
		}
	}
}
#if 0
				la= base->object->data;
				
				glPushMatrix();
				glLoadMatrixf((float *)G.vd->viewmat);
				
				where_is_object_simul(base->object);
				VECCOPY(vec, base->object->obmat[3]);
				
				if(la->type==LA_SUN) {
					vec[0]= base->object->obmat[2][0];
					vec[1]= base->object->obmat[2][1];
					vec[2]= base->object->obmat[2][2];
					vec[3]= 0.0;
					glLightfv(GL_LIGHT0+count, GL_POSITION, vec); 
				}
				else {
					vec[3]= 1.0;
					glLightfv(GL_LIGHT0+count, GL_POSITION, vec); 
					glLightf(GL_LIGHT0+count, GL_CONSTANT_ATTENUATION, 1.0);
					glLightf(GL_LIGHT0+count, GL_LINEAR_ATTENUATION, la->att1/la->dist);
					/* post 2.25 engine supports quad lights */
					glLightf(GL_LIGHT0+count, GL_QUADRATIC_ATTENUATION, la->att2/(la->dist*la->dist));
					
					if(la->type==LA_SPOT) {
						vec[0]= -base->object->obmat[2][0];
						vec[1]= -base->object->obmat[2][1];
						vec[2]= -base->object->obmat[2][2];
						glLightfv(GL_LIGHT0+count, GL_SPOT_DIRECTION, vec);
						glLightf(GL_LIGHT0+count, GL_SPOT_CUTOFF, la->spotsize/2.0);
						glLightf(GL_LIGHT0+count, GL_SPOT_EXPONENT, 128.0*la->spotblend);
					}
					else glLightf(GL_LIGHT0+count, GL_SPOT_CUTOFF, 180.0);
				}
				
				vec[0]= la->energy*la->r;
				vec[1]= la->energy*la->g;
				vec[2]= la->energy*la->b;
				vec[3]= 1.0;
				glLightfv(GL_LIGHT0+count, GL_DIFFUSE, vec); 
				glLightfv(GL_LIGHT0+count, GL_SPECULAR, vec);//zero); 
				glEnable(GL_LIGHT0+count);
				
				glPopMatrix();					
				
				count++;
				if(count>7) break;
			}
			}
		}
		base= base->next;
	}

	return count;
}
#endif
#endif

static GPUNodeBuf *texture_rgb_blend(GPUMaterial *mat, GPUNodeBuf *texbuf, GPUNodeBuf *outbuf, GPUNodeBuf *factbuf, GPUNodeBuf *facgbuf, int blendtype)
{
	GPUNodeBuf *inbuf;
	GPUNode *node;

	if(blendtype == MTEX_BLEND)
		node = GPU_mat_node_create(mat, "mtex_rgb_blend", NULL, NULL);
	else
		node = GPU_mat_node_create(mat, "mtex_rgb_blend", NULL, NULL);

	GPU_node_input(node, GPU_VEC3, "out", NULL, outbuf);
	GPU_node_input(node, GPU_VEC3, "tex", NULL, texbuf);
	GPU_node_input(node, GPU_FLOAT, "fact", NULL, factbuf);
	GPU_node_input(node, GPU_FLOAT, "facg", NULL, facgbuf);

	GPU_node_output(node, GPU_VEC3, "in", &inbuf);

	return inbuf;
}

static GPUNodeBuf *texture_value_blend(GPUMaterial *mat, GPUNodeBuf *texbuf, GPUNodeBuf *outbuf, GPUNodeBuf *factbuf, GPUNodeBuf *facgbuf, int blendtype)
{
	GPUNodeBuf *inbuf;
	GPUNode *node;

	if(blendtype == MTEX_BLEND)
		node = GPU_mat_node_create(mat, "mtex_value_blend", NULL, NULL);
	else
		node = GPU_mat_node_create(mat, "mtex_value_blend", NULL, NULL);

	GPU_node_input(node, GPU_FLOAT, "out", NULL, outbuf);
	GPU_node_input(node, GPU_FLOAT, "tex", NULL, texbuf);
	GPU_node_input(node, GPU_FLOAT, "fact", NULL, factbuf);
	GPU_node_input(node, GPU_FLOAT, "facg", NULL, facgbuf);

	GPU_node_output(node, GPU_FLOAT, "in", &inbuf);

	return inbuf;
}

static void material_tex_nodes_create(GPUMaterial *mat, Material *ma, GPUNodeBuf **colbuf_r, GPUNodeBuf **specbuf_r, GPUNodeBuf **norbuf_r, GPUNodeBuf **alphabuf_r)
{
	MTex *mtex;
	Tex *tex;
	GPUNodeBuf *texcobuf, *tinbuf, *trgbbuf, *tnorbuf, *stencilbuf = NULL;
	GPUNodeBuf *colbuf, *specbuf, *colfacbuf, *norbuf, *newnorbuf;
	GPUNodeBuf *defvarbuf, *varfacbuf, *alphabuf;
	GPUNode *node;
	float col[4], spec[4], one = 1.0f;
	int tex_nr, rgbnor;

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

	node = GPU_mat_node_create(mat, "setvalue", NULL, NULL);
	GPU_mat_node_uniform(node, GPU_FLOAT, &ma->alpha);
	GPU_node_output(node, GPU_FLOAT, "alpha", &alphabuf);

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
				rgbnor= TEX_RGB;
		    }
			else continue;

			/* texture output */

			if((rgbnor & TEX_RGB) && (mtex->texflag & MTEX_RGBTOINT)) {
				node = GPU_mat_node_create(mat, "mtex_rgbtoint", NULL, NULL);
				GPU_node_input(node, GPU_VEC4, "trgb", NULL, trgbbuf);
				GPU_node_output(node, GPU_FLOAT, "tin", &tinbuf);
				rgbnor -= TEX_RGB;
			}

			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgbnor & TEX_RGB) {
					node = GPU_mat_node_create(mat, "mtex_rgb_invert", NULL, NULL);
					GPU_node_input(node, GPU_VEC4, "trgb", NULL, trgbbuf);
					GPU_node_output(node, GPU_VEC4, "trgb", &trgbbuf);
				}
				else {
					node = GPU_mat_node_create(mat, "mtex_value_invert", NULL, NULL);
					GPU_node_input(node, GPU_FLOAT, "tin", NULL, tinbuf);
					GPU_node_output(node, GPU_FLOAT, "tin", &tinbuf);
				}
			}

			if(mtex->texflag & MTEX_STENCIL) {
				if(!stencilbuf) {
					node = GPU_mat_node_create(mat, "setvalue", NULL, NULL);
					GPU_mat_node_uniform(node, GPU_FLOAT, &one);
					GPU_node_output(node, GPU_FLOAT, "stencil", &stencilbuf);
				}

				if(rgbnor & TEX_RGB) {
					node = GPU_mat_node_create(mat, "mtex_rgb_stencil", NULL, NULL);
					GPU_node_input(node, GPU_FLOAT, "stencil", NULL, stencilbuf);
					GPU_node_input(node, GPU_VEC4, "trgb", NULL, trgbbuf);
					GPU_node_output(node, GPU_FLOAT, "stencil", &stencilbuf);
					GPU_node_output(node, GPU_VEC4, "trgb", &trgbbuf);
				}
				else {
					node = GPU_mat_node_create(mat, "mtex_value_stencil", NULL, NULL);
					GPU_node_input(node, GPU_FLOAT, "stencil", NULL, stencilbuf);
					GPU_node_input(node, GPU_FLOAT, "tin", NULL, tinbuf);
					GPU_node_output(node, GPU_FLOAT, "stencil", &stencilbuf);
					GPU_node_output(node, GPU_FLOAT, "tinbuf", &tinbuf);
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

				if((rgbnor & TEX_RGB)==0);
				else if(mtex->mapto & MAP_ALPHA) {
					if(!stencilbuf) {
						node = GPU_mat_node_create(mat, "setvalue", NULL, NULL);
						GPU_mat_node_uniform(node, GPU_FLOAT, &one);
						GPU_node_output(node, GPU_FLOAT, "stencil", &stencilbuf);
					}

					node = GPU_mat_node_create(mat, "setvalue", NULL, NULL);
					GPU_node_input(node, GPU_FLOAT, "stencil", NULL, stencilbuf);
					GPU_node_output(node, GPU_FLOAT, "tinbuf", &tinbuf);
				}
				else {
					node = GPU_mat_node_create(mat, "mtex_alpha_from_col", NULL, NULL);
					GPU_node_input(node, GPU_VEC4, "trgb", NULL, trgbbuf);
					GPU_node_output(node, GPU_FLOAT, "tin", &tinbuf);
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

			if(mtex->mapto & MAP_VARS) {
				if(!stencilbuf) {
					node = GPU_mat_node_create(mat, "setvalue", NULL, NULL);
					GPU_mat_node_uniform(node, GPU_FLOAT, &one);
					GPU_node_output(node, GPU_FLOAT, "stencil", &stencilbuf);
				}

				node = GPU_mat_node_create(mat, "math_multiply", NULL, NULL);
				GPU_mat_node_uniform(node, GPU_FLOAT, &mtex->varfac);
				GPU_node_input(node, GPU_FLOAT, "stencil", NULL, stencilbuf);
				GPU_node_output(node, GPU_FLOAT, "varfac", &varfacbuf);

				if(rgbnor & TEX_RGB) {
					if(1) { // todo, check texres.talpha
						node = GPU_mat_node_create(mat, "mtex_alpha_from_col", NULL, NULL);
						GPU_node_input(node, GPU_VEC4, "trgb", NULL, trgbbuf);
						GPU_node_output(node, GPU_FLOAT, "tinbuf", &tinbuf);
					}
					else {
						node = GPU_mat_node_create(mat, "mtex_rgbtoint", NULL, NULL);
						GPU_node_input(node, GPU_VEC4, "trgb", NULL, trgbbuf);
						GPU_node_output(node, GPU_FLOAT, "tin", &tinbuf);
					}
				}

				if(mtex->mapto & MAP_ALPHA) {
					node = GPU_mat_node_create(mat, "setvalue", NULL, NULL);
					GPU_mat_node_uniform(node, GPU_FLOAT, &mtex->def_var);
					GPU_node_output(node, GPU_FLOAT, "defvar", &defvarbuf);

					alphabuf = texture_value_blend(mat, defvarbuf, alphabuf, tinbuf, varfacbuf, mtex->blendtype);
				}
			}
		}
	}

	*colbuf_r= colbuf;
	*specbuf_r= specbuf;
	*norbuf_r = norbuf;
	*alphabuf_r= alphabuf;
}

static void material_nodes_create(GPUMaterial *mat, Material *ma)
{
	GPUNode *node;
	GPUNodeBuf *colbuf, *specbuf, *norbuf, *alphabuf, *combinedbuf;
	float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float hard;

	material_tex_nodes_create(mat, ma, &colbuf, &specbuf, &norbuf, &alphabuf);

	mat->alpha= (ma->alpha < 1.0f);

	if(ma->mode & MA_SHLESS) {
		mat->outbuf = colbuf;
	}
	else {
		if(ma->emit > 0.0f) {
			node = GPU_mat_node_create(mat, "shade_emit", NULL, NULL);
			GPU_mat_node_uniform(node, GPU_FLOAT, &ma->emit);
			GPU_node_input(node, GPU_VEC4, "col", NULL, colbuf);
			GPU_node_output(node, GPU_VEC4, "outcol", &mat->outbuf);
		}
		else {
			node = GPU_mat_node_create(mat, "setrgb", NULL, NULL);
			GPU_mat_node_uniform(node, GPU_VEC4, zero);
			GPU_node_output(node, GPU_VEC4, "col", &mat->outbuf);
		}

		//material_lights(mat, ma, colbuf, specbuf, norbuf);
		
		hard= ma->har;
		node = GPU_mat_node_create(mat, "material_simple", NULL, NULL);
		GPU_node_input(node, GPU_VEC4, "col", NULL, colbuf);
		GPU_mat_node_uniform(node, GPU_FLOAT, &ma->ref);
		GPU_node_input(node, GPU_VEC4, "spec", NULL, specbuf);
		GPU_mat_node_uniform(node, GPU_FLOAT, &ma->spec);
		hard= ma->har;
		GPU_mat_node_uniform(node, GPU_FLOAT, &hard);
		GPU_node_input(node, GPU_VEC3, "nor", NULL, norbuf);
		GPU_node_output(node, GPU_VEC4, "combined", &combinedbuf);
		
		node = GPU_mat_node_create(mat, "shade_add", NULL, NULL);
		GPU_node_input(node, GPU_VEC4, "col1", NULL, mat->outbuf);
		GPU_node_input(node, GPU_VEC4, "col2", NULL, combinedbuf);
		GPU_node_output(node, GPU_VEC4, "outcol", &mat->outbuf);
	}

	node = GPU_mat_node_create(mat, "mtex_alpha_to_col", NULL, NULL);
	GPU_node_input(node, GPU_VEC4, "col", NULL, mat->outbuf);
	GPU_node_input(node, GPU_FLOAT, "alpha", NULL, alphabuf);
	GPU_node_output(node, GPU_VEC4, "col", &mat->outbuf);
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

