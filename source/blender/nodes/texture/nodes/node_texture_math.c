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

/** \file blender/nodes/texture/nodes/node_texture_math.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"


/* **************** SCALAR MATH ******************** */
static bNodeSocketTemplate inputs[] = {
	{ SOCK_FLOAT, 1, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f, PROP_NONE},
	{ SOCK_FLOAT, 1, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f, PROP_NONE},
	{ -1, 0, "" }
};

static bNodeSocketTemplate outputs[] = {
	{ SOCK_FLOAT, 0, N_("Value")},
	{ -1, 0, "" }
};

static void exec(void *UNUSED(data),
                 int UNUSED(thread),
                 bNode *node,
                 bNodeExecData *UNUSED(execdata),
                 bNodeStack **in,
                 bNodeStack **out)
{
	float in0 = tex_input_value(in[0]);
	float in1 = tex_input_value(in[1]);

	switch (node->custom1) {

		case 0: /* Add */
			out[0]->vec[0] = in0 + in1;
			break;
		case 1: /* Subtract */
			out[0]->vec[0] = in0 - in1;
			break;
		case 2: /* Multiply */
			out[0]->vec[0] = in0 * in1;
			break;
		case 3: /* Divide */
		{
			if (in1 == 0) /* We don't want to divide by zero. */
				out[0]->vec[0] = 0.0f;
			else
				out[0]->vec[0] = in0 / in1;
			break;
		}
		case 4: /* Sine */
		{
			out[0]->vec[0] = sinf(in0);
			break;
		}
		case 5: /* Cosine */
		{
			out[0]->vec[0] = cosf(in0);
			break;
		}
		case 6: /* Tangent */
		{
			out[0]->vec[0] = tanf(in0);
			break;
		}
		case 7: /* Arc-Sine */
		{
			/* Can't do the impossible... */
			if (in0 <= 1.0f && in0 >= -1.0f)
				out[0]->vec[0] = asinf(in0);
			else
				out[0]->vec[0] = 0.0f;
			break;
		}
		case 8: /* Arc-Cosine */
		{
			/* Can't do the impossible... */
			if (in0 <= 1.0f && in0 >= -1.0f)
				out[0]->vec[0]= acosf(in0);
			else
				out[0]->vec[0] = 0.0;
			break;
		}
		case 9: /* Arc-Tangent */
		{
			out[0]->vec[0] = atan(in0);
			break;
		}
		case 10: /* Power */
		{
			/* Only raise negative numbers by full integers */
			if (in0 >= 0.0f) {
				out[0]->vec[0] = pow(in0, in1);
			}
			else {
				float y_mod_1 = fmod(in1, 1.0f);
				if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
					out[0]->vec[0] = powf(in0, floor(in1 + 0.5f));
				}
				else {
					out[0]->vec[0] = 0.0f;
				}
			}
			break;
		}
		case 11: /* Logarithm */
		{
			/* Don't want any imaginary numbers... */
			if (in0 > 0.0f  && in1 > 0.0f)
				out[0]->vec[0] = log(in0) / log(in1);
			else
				out[0]->vec[0] = 0.0;
			break;
		}
		case 12: /* Minimum */
		{
			if (in0 < in1)
				out[0]->vec[0] = in0;
			else
				out[0]->vec[0] = in1;
			break;
		}
		case 13: /* Maximum */
		{
			if (in0 > in1)
				out[0]->vec[0] = in0;
			else
				out[0]->vec[0] = in1;
			break;
		}
		case 14: /* Round */
		{
			out[0]->vec[0] = (in0 < 0.0f) ? (int)(in0 - 0.5f) : (int)(in0 + 0.5f);
			break;
		}

		case 15: /* Less Than */
		{
			if (in0 < in1)
				out[0]->vec[0] = 1.0f;
			else
				out[0]->vec[0] = 0.0f;
			break;
		}

		case 16: /* Greater Than */
		{
			if (in0 > in1)
				out[0]->vec[0] = 1.0f;
			else
				out[0]->vec[0] = 0.0f;
			break;
		}

		case 17: /* Modulo */
		{
			if (in1 == 0.0f)
				out[0]->vec[0] = 0.0f;
			else
				out[0]->vec[0] = fmod(in0, in1);
			break;
		}

		case 18: /* Absolute */
		{
			out[0]->vec[0] = fabsf(in0);
			break;
		}

		default:
		{
			BLI_assert(0);
			break;
		}
	}
}

void register_node_type_tex_math(void)
{
	static bNodeType ntype;

	tex_node_type_base(&ntype, TEX_NODE_MATH, "Math", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_label(&ntype, node_math_label);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_exec(&ntype, NULL, NULL, exec);

	nodeRegisterType(&ntype);
}
