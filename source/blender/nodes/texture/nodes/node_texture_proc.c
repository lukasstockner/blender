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

/** \file blender/nodes/texture/nodes/node_texture_proc.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"

#include "RE_shader_ext.h"

/*
 * In this file: wrappers to use procedural textures as nodes
 */


static bNodeSocketTemplate outputs_both[] = {
	{ SOCK_RGBA, 0, N_("Color"),  1.0f, 0.0f, 0.0f, 1.0f },
	{ SOCK_VECTOR, 0, N_("Normal"), 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, PROP_DIRECTION },
	{ -1, 0, "" }
};
static bNodeSocketTemplate outputs_color_only[] = {
	{ SOCK_RGBA, 0, N_("Color") },
	{ -1, 0, "" }
};

/* Inputs common to all, #defined because nodes will need their own inputs too */
#define I 2 /* count */
#define COMMON_INPUTS \
	{ SOCK_RGBA, 1, "Color 1", 0.0f, 0.0f, 0.0f, 1.0f }, \
	{ SOCK_RGBA, 1, "Color 2", 1.0f, 1.0f, 1.0f, 1.0f }

/* Calls multitex and copies the result to the outputs. Called by xxx_exec, which handles inputs. */
static void do_proc(float *result,
                    TexCallData *cdata,
                    const float col1[4],
                    const float col2[4],
                    bool is_normal,
                    Tex *tex,
                    const short thread)
{
	TexResult texres;
	int textype;

	if (is_normal) {
		texres.nor = result;
	}
	else
		texres.nor = NULL;

	textype = multitex_nodes(tex, cdata->co, cdata->dxt, cdata->dyt, cdata->osatex,
	                         &texres, thread, 0, cdata->shi, cdata->mtex, NULL);

	if (is_normal)
		return;

	if (textype & TEX_RGB) {
		copy_v4_v4(result, &texres.tr);
	}
	else {
		copy_v4_v4(result, col1);
		ramp_blend(MA_RAMP_BLEND, result, texres.tin, col2);
	}
}

typedef void (*MapFn) (Tex *tex, bNodeStack **in);

static void texfn(float *result,
                  TexCallData *cdata,
                  bNode *node,
                  bNodeStack **in,
                  bool is_normal,
                  MapFn map_inputs,
                  short thread)
{
	Tex tex = *((Tex *)(node->storage));
	float col1[4], col2[4];
	tex_input_rgba(col1, in[0]);
	tex_input_rgba(col2, in[1]);

	map_inputs(&tex, in);

	do_proc(result, cdata, col1, col2, is_normal, &tex, thread);
}

/* Boilerplate generators */

#define ProcNoInputs(name) \
		static void name##_map_inputs(Tex *UNUSED(tex), bNodeStack **UNUSED(in)) \
		{}

#define ProcDef(name) \
	static void name##_exec(void *data, int thread, bNode *node, bNodeExecData *execdata, bNodeStack **in, bNodeStack **out) \
	{                                                                                                    \
		int outs = BLI_countlist(&node->outputs);                                                        \
		TexCallData *cdata = (TexCallData *)data;                                                        \
		if (outs >= 1) {                                                                                 \
			texfn(out[0]->vec, cdata, node, in, false, &name##_map_inputs, thread);                      \
			tex_do_preview(execdata->preview, cdata->co, out[0]->vec, cdata->do_manage);                 \
		}                                                                                                \
		if (outs >= 2) texfn(out[1]->vec, cdata, node, in, true, &name##_map_inputs, thread);            \
	}


/* --- VORONOI -- */
static bNodeSocketTemplate voronoi_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("W1"), 1.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("W2"), 0.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("W3"), 0.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("W4"), 0.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f, PROP_NONE },

	{ SOCK_FLOAT, 1, N_("iScale"), 1.0f, 0.0f, 0.0f, 0.0f,    0.01f,  10.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Size"),   0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 4.0f, PROP_UNSIGNED },

	{ -1, 0, "" }
};
static void voronoi_map_inputs(Tex *tex, bNodeStack **in)
{
	tex->vn_w1 = tex_input_value(in[I + 0]);
	tex->vn_w2 = tex_input_value(in[I + 1]);
	tex->vn_w3 = tex_input_value(in[I + 2]);
	tex->vn_w4 = tex_input_value(in[I + 3]);

	tex->ns_outscale = tex_input_value(in[I + 4]);
	tex->noisesize   = tex_input_value(in[I + 5]);
}
ProcDef(voronoi)

/* --- BLEND -- */
static bNodeSocketTemplate blend_inputs[] = {
	COMMON_INPUTS,
	{ -1, 0, "" }
};
ProcNoInputs(blend)
ProcDef(blend)

/* -- MAGIC -- */
static bNodeSocketTemplate magic_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Turbulence"), 5.0f, 0.0f, 0.0f, 0.0f,   0.0f, 200.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void magic_map_inputs(Tex *tex, bNodeStack **in)
{
	tex->turbul = tex_input_value(in[I + 0]);
}
ProcDef(magic)

/* --- MARBLE --- */
static bNodeSocketTemplate marble_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Turbulence"), 5.0f,  0.0f, 0.0f, 0.0f,   0.0f, 200.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void marble_map_inputs(Tex *tex, bNodeStack **in)
{
	tex->noisesize = tex_input_value(in[I + 0]);
	tex->turbul    = tex_input_value(in[I + 1]);
}
ProcDef(marble)

/* --- CLOUDS --- */
static bNodeSocketTemplate clouds_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void clouds_map_inputs(Tex *tex, bNodeStack **in)
{
	tex->noisesize = tex_input_value(in[I + 0]);
}
ProcDef(clouds)

/* --- DISTORTED NOISE --- */
static bNodeSocketTemplate distnoise_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f,  2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Distortion"), 1.00f, 0.0f, 0.0f, 0.0f,   0.0000f, 10.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void distnoise_map_inputs(Tex *tex, bNodeStack **in)
{
	tex->noisesize   = tex_input_value(in[I + 0]);
	tex->dist_amount = tex_input_value(in[I + 1]);
}
ProcDef(distnoise)

/* --- WOOD --- */
static bNodeSocketTemplate wood_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Turbulence"), 5.0f,  0.0f, 0.0f, 0.0f,   0.0f, 200.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void wood_map_inputs(Tex *tex, bNodeStack **in)
{
	tex->noisesize = tex_input_value(in[I + 0]);
	tex->turbul    = tex_input_value(in[I + 1]);
}
ProcDef(wood)

/* --- MUSGRAVE --- */
static bNodeSocketTemplate musgrave_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("H"),          1.0f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Lacunarity"), 2.0f, 0.0f, 0.0f, 0.0f,   0.0f,    6.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Octaves"),    2.0f, 0.0f, 0.0f, 0.0f,   0.0f,    8.0f, PROP_UNSIGNED },

	{ SOCK_FLOAT, 1, N_("iScale"),     1.0f,  0.0f, 0.0f, 0.0f,  0.0f,   10.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,  0.0001f, 2.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void musgrave_map_inputs(Tex *tex, bNodeStack **in)
{
	tex->mg_H          = tex_input_value(in[I + 0]);
	tex->mg_lacunarity = tex_input_value(in[I + 1]);
	tex->mg_octaves    = tex_input_value(in[I + 2]);
	tex->ns_outscale   = tex_input_value(in[I + 3]);
	tex->noisesize     = tex_input_value(in[I + 4]);
}
ProcDef(musgrave)

/* --- NOISE --- */
static bNodeSocketTemplate noise_inputs[] = {
	COMMON_INPUTS,
	{ -1, 0, "" }
};
ProcNoInputs(noise)
ProcDef(noise)

/* --- STUCCI --- */
static bNodeSocketTemplate stucci_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Turbulence"), 5.0f,  0.0f, 0.0f, 0.0f,   0.0f, 200.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void stucci_map_inputs(Tex *tex, bNodeStack **in)
{
	tex->noisesize = tex_input_value(in[I + 0]);
	tex->turbul    = tex_input_value(in[I + 1]);
}
ProcDef(stucci)

/* --- */

static void init(bNodeTree *UNUSED(ntree), bNode *node)
{
	Tex *tex = MEM_callocN(sizeof(Tex), "Tex");
	node->storage = tex;

	default_tex(tex);
	tex->type = node->type - TEX_NODE_PROC;

	if (tex->type == TEX_WOOD)
		tex->stype = TEX_BANDNOISE;
}

/* Node type definitions */
#define TexDef(TEXTYPE, outputs, name, Name) \
void register_node_type_tex_proc_##name(void) \
{ \
	static bNodeType ntype; \
	\
	tex_node_type_base(&ntype, TEX_NODE_PROC+TEXTYPE, Name, NODE_CLASS_TEXTURE, NODE_PREVIEW); \
	node_type_socket_templates(&ntype, name##_inputs, outputs); \
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE); \
	node_type_init(&ntype, init); \
	node_type_storage(&ntype, "Tex", node_free_standard_storage, node_copy_standard_storage); \
	node_type_exec(&ntype, NULL, NULL, name##_exec); \
	\
	nodeRegisterType(&ntype); \
}

#define C outputs_color_only
#define CV outputs_both

TexDef(TEX_VORONOI,   CV, voronoi,   "Voronoi"  )
TexDef(TEX_BLEND,     C,  blend,     "Blend"    )
TexDef(TEX_MAGIC,     C,  magic,     "Magic"    )
TexDef(TEX_MARBLE,    CV, marble,    "Marble"   )
TexDef(TEX_CLOUDS,    CV, clouds,    "Clouds"   )
TexDef(TEX_WOOD,      CV, wood,      "Wood"     )
TexDef(TEX_MUSGRAVE,  CV, musgrave,  "Musgrave" )
TexDef(TEX_NOISE,     C,  noise,     "Noise"    )
TexDef(TEX_STUCCI,    CV, stucci,    "Stucci"   )
TexDef(TEX_DISTNOISE, CV, distnoise, "Distorted Noise" )
