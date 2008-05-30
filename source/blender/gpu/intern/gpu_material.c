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

#include "GPU_extensions.h"
#include "GPU_material.h"

#include "gpu_codegen.h"

#include <string.h>

struct GPUMaterial {
	/* for creating the material */
	ListBase nodes;
	GPUNodeLink *outlink;

	/* for binding the material */
	GPUPass *pass;
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
	GPU_nodes_create_vertex_attributes(&material->nodes, &material->attribs);
}

static void gpu_material_set_attrib_id(GPUMaterial *material)
{
	GPUVertexAttribs *attribs;
	GPUShader *shader;
	GPUPass *pass;
	char name[32];
	int a, b;

	attribs= &material->attribs;
	pass= material->pass;
	if(!pass) {
		attribs->totlayer = 0;
		return;
	}
	
	shader= GPU_pass_shader(pass);
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
	if (material->outlink) {
		GPUNodeLink *outlink;

		gpu_material_create_vertex_attributes(material);

		outlink = material->outlink;
		material->pass = GPU_generate_pass(&material->nodes, outlink, 1);

		if(!material->pass)
			return 0;

		gpu_material_set_attrib_id(material);
		return 1;
	}

	return 0;
}

void GPU_material_free(GPUMaterial *material)
{
	if(material->pass)
		GPU_pass_free(material->pass);

	GPU_nodes_free(&material->nodes);
	MEM_freeN(material);
}

void GPU_material_bind(Object *ob, GPUMaterial *material)
{
	if(material->pass) {
		GPUShader *shader = GPU_pass_shader(material->pass);

		GPU_pass_bind(material->pass);

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
	if (material->pass) {
		if(material->alpha)
			glDisable(GL_BLEND);

		GPU_pass_unbind(material->pass);
	}
}

void GPU_material_vertex_attributes(GPUMaterial *material, GPUVertexAttribs *attribs)
{
	*attribs = material->attribs;
}

void GPU_material_output_link(GPUMaterial *material, GPUNodeLink *link)
{
	if(!material->outlink)
		material->outlink= link;
}

void GPU_material_enable_alpha(GPUMaterial *material)
{
	material->alpha= 1;
}

void gpu_material_add_node(GPUMaterial *material, GPUNode *node)
{
	BLI_addtail(&material->nodes, node);
}

/* Code generation */

#if 0
static void material_lights(GPUMaterial *mat, Material *ma, GPUNodeLink *col, GPUNodeLink *spec, GPUNodeLink *nor)
{
	Base *base;
	Object *lampob;
	Lamp *la;
	GPUNode *node;
	GPUNodeLink *lv, *dist, *visifac, *outcol;
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
				GPU_node_input(node, GPU_VEC4, "col1", NULL, mat->outlink);
				GPU_node_input(node, GPU_VEC4, "col2", NULL, outcol);
				GPU_node_output(node, GPU_VEC4, "outcol", &mat->outlink);
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

static void texture_rgb_blend(GPUMaterial *mat, GPUNodeLink *tex, GPUNodeLink *out, GPUNodeLink *fact, GPUNodeLink *facg, int blendtype, GPUNodeLink **in)
{
	GPU_link(mat, "mtex_rgb_blend", out, tex, fact, facg, in);
}

static void texture_value_blend(GPUMaterial *mat, GPUNodeLink *tex, GPUNodeLink *out, GPUNodeLink *fact, GPUNodeLink *facg, int blendtype, GPUNodeLink **in)
{
	GPU_link(mat, "mtex_value_blend", out, tex, fact, facg, in);
}

static void do_material_tex_gpu(GPUMaterial *mat, Material *ma, GPUNodeLink **col_r, GPUNodeLink **spec_r, GPUNodeLink **nor_r, GPUNodeLink **alpha_r)
{
	MTex *mtex;
	Tex *tex;
	GPUNodeLink *texco, *tin, *trgb, *tnor, *tcol, *stencil = NULL;
	GPUNodeLink *col, *spec, *colfac, *nor, *newnor;
	GPUNodeLink *defvar, *varfac, *alpha;
	float macol[4], maspec[4], mtexrgb[4], one = 1.0f;
	int tex_nr, rgbnor, talpha;

	/* set default values */
	VECCOPY(macol, &ma->r);
	macol[3]= 1.0f;
	VECCOPY(maspec, &ma->specr);
	maspec[3]= 1.0f;

	GPU_link(mat, "setrgb", GPU_uniform(macol), &col);
	GPU_link(mat, "setrgb", GPU_uniform(maspec), &spec);
	GPU_link(mat, "texco_norm", &nor);
	GPU_link(mat, "setvalue", GPU_uniform(&ma->alpha), &alpha);
	GPU_link(mat, "setvalue", GPU_uniform(&one), &stencil);

	/* go over texture slots */
	for(tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		/* separate tex switching */
		if(ma->septex & (1<<tex_nr)) continue;
		
		if(ma->mtex[tex_nr]) {
			mtex= ma->mtex[tex_nr];
			
			tex= mtex->tex;
			if(tex==0) continue;

			/* which coords */
			if(mtex->texco==TEXCO_ORCO)
				GPU_link(mat, "texco_orco", GPU_attribute(CD_ORCO, ""), &texco);
			else if(mtex->texco==TEXCO_NORM)
				GPU_link(mat, "texco_norm", &texco);
			else if(mtex->texco==TEXCO_UV)
				GPU_link(mat, "texco_uv", GPU_attribute(CD_MTFACE, mtex->uvname), &texco);
			else
				continue;

			GPU_link(mat, "mtex_2d_mapping", texco, &texco);
			GPU_link(mat, "mtex_mapping", texco, GPU_uniform(mtex->size), GPU_uniform(mtex->ofs), &texco);

			if(tex && tex->type == TEX_IMAGE && tex->ima) {
				GPU_link(mat, "mtex_image", texco, GPU_image(tex->ima, NULL), &tin, &trgb, &tnor);
				rgbnor= TEX_RGB;
				talpha= 1;
		    }
			else continue;

			/* texture output */
			if((rgbnor & TEX_RGB) && (mtex->texflag & MTEX_RGBTOINT)) {
				GPU_link(mat, "mtex_rgbtoint", trgb, &tin);
				rgbnor -= TEX_RGB;
			}

			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgbnor & TEX_RGB)
					GPU_link(mat, "mtex_rgb_invert", trgb, &trgb);
				else
					GPU_link(mat, "mtex_value_invert", tin, &tin);
			}

			if(mtex->texflag & MTEX_STENCIL) {
				if(rgbnor & TEX_RGB)
					GPU_link(mat, "mtex_rgb_stencil", stencil, trgb, &stencil, &trgb);
				else
					GPU_link(mat, "mtex_value_stencil", stencil, tin, &stencil, &tin);
			}

			/* mapping */
			if(mtex->mapto & (MAP_COL+MAP_COLSPEC)) {
				/* stencil maps on the texture control slider, not texture intensity value */
				GPU_link(mat, "math_multiply", GPU_uniform(&mtex->colfac), stencil, &colfac);

				if((rgbnor & TEX_RGB)==0) {
					VECCOPY(mtexrgb, &mtex->r);
					mtexrgb[3]= 1.0f;
					GPU_link(mat, "setrgb", GPU_uniform(mtexrgb), &tcol);
				}
				else {
					GPU_link(mat, "setrgb", trgb, &tcol);

					if(mtex->mapto & MAP_ALPHA)
						GPU_link(mat, "setvalue", stencil, &tin);
					else
						GPU_link(mat, "mtex_alpha_from_col", trgb, &tin);
				}
				
				if(mtex->mapto & MAP_COL)
					texture_rgb_blend(mat, tcol, col, tin, colfac, mtex->blendtype, &col);
				if(mtex->mapto & MAP_COLSPEC)
					texture_rgb_blend(mat, tcol, spec, tin, colfac, mtex->blendtype, &spec);
			}

			if(mtex->mapto & MAP_NORM) {
				if(mtex->maptoneg & MAP_NORM) tex->norfac= -mtex->norfac;
				else tex->norfac= mtex->norfac;
				
				if((tex->type==TEX_IMAGE) && (tex->imaflag & TEX_NORMALMAP)) {
					tex->norfac = mtex->norfac;

					if(mtex->normapspace == MTEX_NSPACE_TANGENT)
						GPU_link(mat, "mtex_nspace_tangent", GPU_attribute(CD_TANGENT, ""), nor, tnor, &newnor);
					else
						newnor = tnor;

					GPU_link(mat, "mtex_blend_normal", GPU_uniform(&mtex->norfac), nor, newnor, &nor);
				}
			}

			if(mtex->mapto & MAP_VARS) {
				GPU_link(mat, "math_multiply", GPU_uniform(&mtex->varfac), stencil, &varfac);

				if(rgbnor & TEX_RGB) {
					if(talpha)
						GPU_link(mat, "mtex_alpha_from_col", trgb, &tin);
					else
						GPU_link(mat, "mtex_rgbtoint", trgb, &tin);
				}

				if(mtex->mapto & MAP_ALPHA) {
					GPU_link(mat, "setvalue", GPU_uniform(&mtex->def_var), &defvar);
					texture_value_blend(mat, defvar, alpha, tin, varfac, mtex->blendtype, &alpha);
				}
			}
		}
	}

	/* return buffers */
	*col_r= col;
	*spec_r= spec;
	*nor_r = nor;
	*alpha_r= alpha;
}

GPUNodeLink *GPU_blender_material(GPUMaterial *mat, Material *ma)
{
	GPUNodeLink *col, *spec, *nor, *alpha, *combined, *out = NULL;
	float hard;

	do_material_tex_gpu(mat, ma, &col, &spec, &nor, &alpha);

	if(ma->alpha < 1.0f)
		GPU_material_enable_alpha(mat);

	if(ma->mode & MA_SHLESS) {
		out = col;
	}
	else {
		GPU_link(mat, "shade_emit", GPU_uniform(&ma->emit), col, &out);

		//material_lights(mat, ma, col, spec, nor);
		
		hard= ma->har;
		GPU_link(mat, "material_simple", col, GPU_uniform(&ma->ref), spec,
			GPU_uniform(&ma->spec), GPU_uniform(&hard), nor, &combined);
		GPU_link(mat, "shade_add", out, combined, &out);
	}

	GPU_link(mat, "mtex_alpha_to_col", out, alpha, &out);

	return out;
}

GPUMaterial *GPU_material_from_blender(Material *ma)
{
	GPUMaterial *mat;

	mat = GPU_material_construct_begin();

	GPU_material_output_link(mat, GPU_blender_material(mat, ma));

	if(!GPU_material_construct_end(mat)) {
		GPU_material_free(mat);
		mat= NULL;
	}

	return mat;
}

