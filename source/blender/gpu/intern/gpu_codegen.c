/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_dynstr.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_heap.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "GPU_node.h"
#include "GPU_extensions.h"

#include "gpu_codegen.h"
#include "gpu_nodes.h"
#include "material_vertex_shader.glsl.c"
#include "material_shaders.glsl.c"

#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#define _vsnprintf vsnprintf
#endif

static char* GPU_DATATYPE_STR[17] = {"", "float", "vec2", "vec3", "vec4",
	0, 0, 0, 0, "mat3", 0, 0, 0, 0, 0, 0, "mat4"};

/* Strings */

static void BLI_dynstr_printf(DynStr *dynstr, const char *format, ...)
{
	va_list args;
	int retval;
	char str[2048];

	/* todo: windows support */
	va_start(args, format);
	retval = vsnprintf(str, sizeof(str), format, args);
	va_end(args);

	if (retval >= sizeof(str))
		fprintf(stderr, "BLI_dynstr_printf: limit exceeded\n");
	else
		BLI_dynstr_append(dynstr, str);
}

/* GPU codegen */

static void codegen_convert_datatype(DynStr *ds, int from, int to, char *tmp, int id)
{
	char name[1024];

	snprintf(name, sizeof(name), "%s%d", tmp, id);

	if (from == to) {
		BLI_dynstr_append(ds, name);
	}
	else if (to == GPU_FLOAT) {
		if (from == GPU_VEC4)
			BLI_dynstr_printf(ds, "dot(%s.rgb, vec3(0.35, 0.45, 0.2))", name);
		else if (from == GPU_VEC3)
			BLI_dynstr_printf(ds, "dot(%s, vec3(0.33))", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_printf(ds, "%s.r", name);
	}
	else if (to == GPU_VEC2) {
		if (from == GPU_VEC4)
			BLI_dynstr_printf(ds, "vec2(dot(%s.rgb, vec3(0.35, 0.45, 0.2)), %s.a)", name, name);
		else if (from == GPU_VEC3)
			BLI_dynstr_printf(ds, "vec2(dot(%s.rgb, vec3(0.33)), 1.0)", name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_printf(ds, "vec2(%s, 1.0)", name);
	}
	else if (to == GPU_VEC3) {
		if (from == GPU_VEC4)
			BLI_dynstr_printf(ds, "%s.rgb", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_printf(ds, "vec3(%s.r, %s.r, %s.r)", name, name, name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_printf(ds, "vec3(%s, %s, %s)", name, name, name);
	}
	else {
		if (from == GPU_VEC3)
			BLI_dynstr_printf(ds, "vec4(%s, 1.0)", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_printf(ds, "vec4(%s.r, %s.r, %s.r, %s.g)", name, name, name, name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_printf(ds, "vec4(%s, %s, %s, 1.0)", name, name, name);
	}
}

static int codegen_input_has_texture(GPUInput *input)
{
	if (input->buf)
		return input->buf->tex != 0;
	else if(input->ima)
		return 1;
	else
		return input->tex != 0;
}

static void codegen_set_unique_ids(ListBase *nodes)
{
	GHash *bindhash, *definehash;
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;
	int id = 1, texid = 0;

	bindhash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	definehash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);

	for (node=nodes->first; node; node=node->next) {
		for (input=node->inputs.first; input; input=input->next) {
			/* set id for unique names of uniform variables */
			input->id = id++;
			input->bindtex = 0;
			input->definetex = 0;

			/* set texid used for settings texture slot with multitexture */
			if (codegen_input_has_texture(input) &&
			    ((input->samp == GPU_TEX_RAND) || (input->samp == GPU_TEX_PIXEL))) {
				if (input->buf) {
					/* input is texture from buffer, assign only one texid per
					   buffer to avoid sampling the same texture twice */
					if (!BLI_ghash_haskey(bindhash, input->buf)) {
						input->texid = texid++;
						input->bindtex = 1;
						BLI_ghash_insert(bindhash, input->buf, SET_INT_IN_POINTER(input->texid));
					}
					else
						input->texid = GET_INT_FROM_POINTER(BLI_ghash_lookup(bindhash, input->buf));
				}
				else if(input->ima) {
					/* input is texture from image, assign only one texid per
					   buffer to avoid sampling the same texture twice */
					if (!BLI_ghash_haskey(bindhash, input->ima)) {
						input->texid = texid++;
						input->bindtex = 1;
						BLI_ghash_insert(bindhash, input->ima, SET_INT_IN_POINTER(input->texid));
					}
					else
						input->texid = GET_INT_FROM_POINTER(BLI_ghash_lookup(bindhash, input->ima));
				}
				else {
					/* input is user created texture, we know there there is
					   only one, so assign new texid */
					input->bindtex = 1;
					input->texid = texid++;
				}

				/* make sure this pixel is defined exactly once */
				if (input->samp == GPU_TEX_PIXEL) {
					if(input->ima) {
						if (!BLI_ghash_haskey(definehash, input->ima)) {
							input->definetex = 1;
							BLI_ghash_insert(definehash, input->ima, SET_INT_IN_POINTER(input->texid));
						}
					}
					else {
						if (!BLI_ghash_haskey(definehash, input->buf)) {
							input->definetex = 1;
							BLI_ghash_insert(definehash, input->buf, SET_INT_IN_POINTER(input->texid));
						}
					}
				}
			}
		}

		for (output=node->outputs.first; output; output=output->next)
			/* set id for unique names of tmp variables storing output */
			output->id = id++;
	}

	BLI_ghash_free(bindhash, NULL, NULL);
	BLI_ghash_free(definehash, NULL, NULL);
}

static void codegen_print_uniforms_functions(DynStr *ds, ListBase *nodes)
{
	GPUNode *node;
	GPUInput *input;

	/* print uniforms */
	for (node=nodes->first; node; node=node->next) {
		for (input=node->inputs.first; input; input=input->next) {
			if ((input->samp == GPU_TEX_RAND) || (input->samp == GPU_TEX_PIXEL)) {
				/* create exactly one sampler for each texture */
				if (codegen_input_has_texture(input) && input->bindtex)
					BLI_dynstr_printf(ds, "uniform %s samp%d;\n",
						(input->textarget == GL_TEXTURE_1D)? "sampler1D": "sampler2D",
						input->texid);
			}
			else if (input->samp == GPU_VEC_UNIFORM) {
				/* and create uniform vectors or matrices for all vectors */
				BLI_dynstr_printf(ds, "uniform %s unf%d;\n",
					GPU_DATATYPE_STR[input->type], input->id);
			}
			else if (input->samp == GPU_ARR_UNIFORM) {
				BLI_dynstr_printf(ds, "uniform %s unf%d[%d];\n",
					GPU_DATATYPE_STR[input->type], input->id, input->arraysize);
			}
			else if (input->samp == GPU_S_ATTRIB && input->attribfirst) {
				BLI_dynstr_printf(ds, "varying %s var%d;\n",
					GPU_DATATYPE_STR[input->type], input->attribid);
			}
		}
	}

	BLI_dynstr_append(ds, "\n");

	BLI_dynstr_append(ds, datatoc_material_shaders_glsl);
}

static void codegen_declare_tmps(DynStr *ds, ListBase *nodes)
{
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;

	for (node=nodes->first; node; node=node->next) {
		/* load pixels from textures */
		for (input=node->inputs.first; input; input=input->next) {
			if (input->samp == GPU_TEX_PIXEL) {
				if (codegen_input_has_texture(input) && input->definetex) {
					BLI_dynstr_printf(ds, "\tvec4 tex%d = texture2D(", input->texid);
					BLI_dynstr_printf(ds, "samp%d, gl_TexCoord[%d].st);\n",
						input->texid, input->texid);
				}
			}
		}

		/* declare temporary variables for node output storage */
		for (output=node->outputs.first; output; output=output->next)
			BLI_dynstr_printf(ds, "\t%s tmp%d;\n",
				GPU_DATATYPE_STR[output->type], output->id);
	}

	BLI_dynstr_append(ds, "\n");
}

static void codegen_call_functions(DynStr *ds, ListBase *nodes, GPUOutput *finaloutput)
{
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;

	for (node=nodes->first; node; node=node->next) {
		BLI_dynstr_printf(ds, "\t%s(", node->name);
		
		for (input=node->inputs.first; input; input=input->next) {
			if (input->samp == GPU_TEX_RAND) {
				BLI_dynstr_printf(ds, "samp%d", input->texid);
				if (input->buf)
					BLI_dynstr_printf(ds, ", gl_TexCoord[%d].st", input->texid);
			}
			else if (input->samp == GPU_TEX_PIXEL) {
				if (input->buf && input->buf->source)
					codegen_convert_datatype(ds, input->buf->source->type /* was GPU_VEC4 */, input->type,
						"tmp", input->buf->source->id);
				else
					codegen_convert_datatype(ds, input->buf->source->type /* was GPU_VEC4 */, input->type,
						"tex", input->texid);
			}
			else if ((input->samp == GPU_VEC_UNIFORM) ||
			         (input->samp == GPU_ARR_UNIFORM))
				BLI_dynstr_printf(ds, "unf%d", input->id);
			else if (input->samp == GPU_S_ATTRIB)
				BLI_dynstr_printf(ds, "var%d", input->attribid);

			BLI_dynstr_append(ds, ", ");
		}

		for (output=node->outputs.first; output; output=output->next) {
			BLI_dynstr_printf(ds, "tmp%d", output->id);
			if (output->next)
				BLI_dynstr_append(ds, ", ");
		}

		BLI_dynstr_append(ds, ");\n");
	}

	BLI_dynstr_append(ds, "\n\tgl_FragColor = ");
	codegen_convert_datatype(ds, finaloutput->type, GPU_VEC4, "tmp", finaloutput->id);
	BLI_dynstr_append(ds, ";\n");
}

char *code_generate(ListBase *nodes, GPUOutput *output)
{
	DynStr *ds = BLI_dynstr_new();
	char *code;

	codegen_set_unique_ids(nodes);
	codegen_print_uniforms_functions(ds, nodes);

	BLI_dynstr_append(ds, "void main(void)\n");
	BLI_dynstr_append(ds, "{\n");

	codegen_declare_tmps(ds, nodes);
	codegen_call_functions(ds, nodes, output);

	BLI_dynstr_append(ds, "}\n");
#if 0
	BLI_dynstr_append(ds, "void main(void) { gl_FragColor = vec4(gl_TexCoord[0].xy, 0.0, 1.0); }");
#endif

	/* create shader */
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	//if(G.f & G_DEBUG) printf("%s\n", code);

	return code;
}

char *code_generate_vertex(ListBase *nodes)
{
	DynStr *ds = BLI_dynstr_new();
	GPUNode *node;
	GPUInput *input;
	char *code;
	
	for (node=nodes->first; node; node=node->next) {
		for (input=node->inputs.first; input; input=input->next) {
			if (input->samp == GPU_S_ATTRIB && input->attribfirst) {
				BLI_dynstr_printf(ds, "attribute %s att%d;\n",
					GPU_DATATYPE_STR[input->type], input->attribid);
				BLI_dynstr_printf(ds, "varying %s var%d;\n",
					GPU_DATATYPE_STR[input->type], input->attribid);
			}
		}
	}

	BLI_dynstr_append(ds, "\n");
	BLI_dynstr_append(ds, datatoc_material_vertex_shader_glsl);

	for (node=nodes->first; node; node=node->next)
		for (input=node->inputs.first; input; input=input->next)
			if (input->samp == GPU_S_ATTRIB && input->attribfirst)
				BLI_dynstr_printf(ds, "\tvar%d = att%d;\n", input->attribid, input->attribid);

	BLI_dynstr_append(ds, "}\n\n");

	code = BLI_dynstr_get_cstring(ds);

	BLI_dynstr_free(ds);

	//if(G.f & G_DEBUG) printf("%s\n", code);

	return code;
}

void GPU_pass_bind(GPUPass *pass)
{
	GPUNode *node;
	GPUInput *input;
	GPUShader *shader = pass->shader;
	ListBase *nodes = &pass->nodes;
	DynStr *ds;
	char *name;

	if (!shader)
		return;

	GPU_shader_bind(shader);

	for (node=nodes->first; node; node=node->next) {
		for (input=node->inputs.first; input; input=input->next) {
			if (input->buf)
				input->tex = input->buf->tex;
			else if (input->ima)
				input->tex = GPU_texture_from_blender(input->ima, input->iuser);

			ds = BLI_dynstr_new();
			if (input->samp == GPU_S_ATTRIB)
				BLI_dynstr_printf(ds, "att%d", input->attribid);
			else if (input->tex)
				BLI_dynstr_printf(ds, "samp%d", input->texid);
			else
				BLI_dynstr_printf(ds, "unf%d", input->id);
			name = BLI_dynstr_get_cstring(ds);
			BLI_dynstr_free(ds);

			if (input->samp == GPU_S_ATTRIB) {
				/*if (input->attribfirst)
					GPU_shader_bind_attribute(shader, input->attribid, name);*/
			}
			else if (input->tex) {
				if (input->bindtex) {
					GPU_texture_bind(input->tex, input->texid);
					GPU_shader_uniform_texture(shader, name, input->tex);
				}
			}
			else if (input->arraysize)
				GPU_shader_uniform_vector(shader, name, input->type,
					input->arraysize, input->vec);
			else
				GPU_shader_uniform_vector(shader, name, input->type, 1, input->vec);

			MEM_freeN(name);
		}
	}
}

void GPU_pass_unbind(GPUPass *pass)
{
	GPUNode *node;
	GPUInput *input;
	GPUShader *shader = pass->shader;
	ListBase *nodes = &pass->nodes;

	if (!shader)
		return;

	for (node=nodes->first; node; node=node->next) {
		for (input=node->inputs.first; input; input=input->next) {
			if (input->tex)
				if(input->bindtex)
					GPU_texture_unbind(input->tex);
			if (input->buf || input->ima)
				input->tex = 0;
		}
	}
	
	GPU_shader_unbind(shader);
}

#if 0
/* MIO Scheduling */

static void gpu_set_output_numinputs_forcedbreak(ListBase *lb)
{
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;
	int forcedbreak;

	for (node=lb->first; node; node=node->next) {
		for (output=node->outputs.first; output; output=output->next) {
			output->numinputs = 0;
			output->forcedbreak = 0;
		}
	}

	for (node=lb->first; node; node=node->next) {
		forcedbreak = 0; // TODO (BLI_countlist(&node->outputs) > 1);
		for (input=node->inputs.first; input; input=input->next) {
			if (input->buf && input->buf->source) {
				input->buf->source->numinputs++;
				if (forcedbreak || (input->samp == GPU_TEX_RAND))
					input->buf->source->forcedbreak = 1;
			}
		}
	}
}

static void gpu_insert_sorted_sethi_ullman(ListBase *lb, GPUInput *newinput)
{
	/* list is sorted high to low */
	GPUInput *input = NULL;
	GPUSortedInput *sinput;
	int sethi_ullman;

	newinput->sort.input = newinput;

	if (newinput->buf && newinput->buf->source) {
		sethi_ullman = newinput->buf->source->node->sethi_ullman;

		for (sinput=lb->first; sinput; sinput=sinput->next) {
			input = sinput->input;
			if (!input->buf || !input->buf->source)
				break;
			else if (sethi_ullman > input->buf->source->node->sethi_ullman)
				break;
		}

		BLI_insertlinkbefore(lb, (input)? &input->sort: NULL,
			&newinput->sort);
	}
	else
		BLI_addtail(lb, &newinput->sort);
}

static int gpu_number_sethi_ullman(GPUNode *node)
{
	GPUInput *input;
	GPUOutput *output;
	ListBase sortedlb;
	int val, maxval = 0, minval = 0, noutputs = 0;

	if (node) {
		for (output=node->outputs.first; output; output=output->next)
			if (output->numinputs > 0)
				noutputs++;

		sortedlb.first = sortedlb.last = NULL;
		for (input=node->inputs.first; input; input=input->next) {
			if (input->buf && input->buf->source) {
				val = gpu_number_sethi_ullman(input->buf->source->node);

				if (input != node->inputs.first) {
					if (val > maxval) maxval = val;
					if (val < minval) minval = val;
				}
				else
					maxval = minval = val;
			}

			gpu_insert_sorted_sethi_ullman(&sortedlb, input);
		}

		node->sortedinputs = sortedlb;

		/* return buffer needed to evaluate subtree */
		node->sethi_ullman = (maxval == minval)? maxval+noutputs: maxval;
		return node->sethi_ullman;
	}
	else
		return 0; /* constant leaf node */
}

static void gpu_set_sethi_ullman_order(GPUNode *node, int *order)
{
	GPUInput *input;
	GPUSortedInput *sinput;

	if (node) {
		for (sinput=node->sortedinputs.first; sinput; sinput=sinput->next) {
			input= sinput->input;
			if (input->buf && input->buf->source)
				gpu_set_sethi_ullman_order(input->buf->source->node, order);
		}

		node->order = (*order)++;
	}
}

static int gpu_update_ready_list(Heap *heap, GPUNode *node)
{
	GPUInput *input;
	GPUSortedInput *sinput;
	int ready = 1;

	if (node) {
		//printf("update node %s %d %d\n", node->name, node->ready, node->scheduled);
		if (!node->ready && !node->scheduled) {
			for (sinput=node->sortedinputs.first; sinput; sinput=sinput->next) {
				input= sinput->input;
				if (input->buf && input->buf->source) {
					ready = ready && gpu_update_ready_list(heap, input->buf->source->node);
					//printf("%d\n", input->buf->source->forcedbreak);
					if (input->buf->source->forcedbreak)
						ready = 0;
				}
			}

			//printf("isready? %d\n", ready);

			if (ready) {
				BLI_heap_insert(heap, node->order, node);
				node->ready = ready;
			}
		}

		return node->scheduled;
	}
	else
		return 1;
}
#endif

/* Passes */

GPUPass *gpu_pass_create(ListBase *nodes, GPUOutput *output, int shared, int vertexshader)
{
	GPUShader *shader;
	GPUPass *pass;
	char *code, *vertexcode;

	code = code_generate(nodes, output);
	vertexcode = (vertexshader)? code_generate_vertex(nodes): NULL;
	shader = GPU_shader_create(vertexcode, code);
	MEM_freeN(code);
	MEM_freeN(vertexcode);

	if (!shader) {
		GPU_nodes_free(nodes);
		return NULL;
	}
	
	pass = MEM_callocN(sizeof(GPUPass), "GPUPass");

	pass->nodes = *nodes;
	pass->output = output;
	pass->shader = shader;
	pass->sharednodes = shared;

	return pass;
}

#if 0
ListBase GPU_generate_passes(ListBase *nodes, struct GPUOutput *output, int vertexshader)
{
	GPUShaderResources res, newres;
	ListBase schedule, rollback, passes;
	Heap *readylist;
	GPUNode *node;
	GPUPass *pass;
	GPUOutput *outp;
	int order = 0, first, shared;

	memset(&passes, 0, sizeof(passes));

	/* TODO: support multi-output nodes? */

	/* do sethi-ullman ordering, inputs are sorted based on this */
	gpu_set_output_numinputs_forcedbreak(nodes);
	gpu_number_sethi_ullman(output->node);
	gpu_set_sethi_ullman_order(output->node, &order);

	while (nodes->first) {
		readylist = BLI_heap_new();
		gpu_update_ready_list(readylist, output->node);

		//printf("sizes: %d %d\n", BLI_heap_size(readylist), BLI_countlist(nodes));

		if (BLI_heap_size(readylist) == 0) {
			BLI_heap_free(readylist, NULL);
			break;
		}

		schedule.first = schedule.last = NULL;
		rollback.first = rollback.last = NULL;
		first = 1;
		memset(&res, 0, sizeof(res));

		while (!BLI_heap_empty(readylist)) {
			node = BLI_heap_popmin(readylist);
			node->ready = 0;

			//printf("pop %s\n", node->name);

			newres = res;
			newres.alu_instructions += node->res.alu_instructions;
			newres.tex_instructions += node->res.tex_instructions;
			newres.constants += node->res.constants;
			newres.uniforms += node->res.uniforms;

			if (first || GPU_shader_resources_verify(&newres)) {
				node->scheduled = 1;
				gpu_update_ready_list(readylist, output->node);

				BLI_remlink(nodes, node);
				if (first || 1 /* TODO: gpu_outputs_verify() */)
					BLI_addtail(&schedule, node);
				else
					BLI_addtail(&rollback, node);
			}
			else
				res = newres;

			first = 0;
		}

		BLI_heap_free(readylist, NULL);

		if (0 /* TODO: !gpu_outputs_verify() */) {
			addlisttolist(nodes, &rollback);
			for (node=rollback.first; node; node=node->next)
				node->scheduled = 0;
		}
		else
			addlisttolist(&schedule, &rollback);

		for (node=nodes->first; node; node=node->next)
			node->ready = 0;

		/* TODO: mutliple outputs don't need separate functions,
		   but can use same and discard output! */
		node = schedule.last;
		for (outp=node->outputs.first; outp; outp=outp->next) {
			if (outp->buf) {
				shared = (outp != node->outputs.last);
				pass = gpu_pass_create(&schedule, outp, shared, vertexshader);
				BLI_addtail(&passes, pass);
			}
		}
	}

	// printf("graph divided in %d groups.\n", BLI_countlist(&passes));

	return passes;
}
#endif

ListBase GPU_generate_single_pass(ListBase *nodes, struct GPUOutput *output, int vertexshader)
{
	ListBase passes;
	GPUPass *pass;

	/* TODO: pruning */
	memset(&passes, 0, sizeof(passes));

	pass = gpu_pass_create(nodes, output, 0, vertexshader);
	if(pass)
		BLI_addtail(&passes, pass);

	memset(nodes, 0, sizeof(*nodes));

	return passes;
}

void GPU_pass_free(ListBase *passes, GPUPass *pass)
{
	BLI_remlink(passes, pass);

	GPU_shader_free(pass->shader);
	if (!pass->sharednodes)
		GPU_nodes_free(&pass->nodes);
	MEM_freeN(pass);
}

