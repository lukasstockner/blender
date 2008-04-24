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

#ifndef __GPU_CODEGEN_H__
#define __GPU_CODEGEN_H__

#include "DNA_listBase.h"

struct ListBase;
struct GPUShader;
struct GPUOutput;
struct GPUNode;

typedef struct GPUPass {
	struct GPUPass *next, *prev;

	ListBase nodes;
	struct GPUOutput *output;
	struct GPUShader *shader;
	int sharednodes; /* are the nodes owned by another pass? */
} GPUPass;

/* Pass Generation
   - Takes a list of nodes and a desired output, and makes a list of passes,
     that are needed to compute the buffer in the output. Nodes used for a
     pass are removed from the list.
*/

ListBase GPU_generate_passes(ListBase *nodes, struct GPUOutput *output, int vertexshader);
ListBase GPU_generate_single_pass(ListBase *nodes, struct GPUOutput *output, int vertexshader);

void GPU_pass_bind(GPUPass *pass);
void GPU_pass_unbind(GPUPass *pass);

void GPU_pass_free(ListBase *passes, GPUPass *pass);

#if 0
char *GPU_codegen_code(struct ListBase *nodes, struct GPUOutput *output);
void GPU_codegen_bind(struct GPUShader *shader, struct ListBase *nodes);
void GPU_codegen_unbind(struct GPUShader *shader, struct ListBase *nodes);
List GPU_schedule(struct ListBase *nodes, struct GPUOutput *output,
	void (*execute)(struct ListBase *nodes, struct GPUOutput *output),
	void (*freenode)(struct GPUNode *node));
#endif

#endif

