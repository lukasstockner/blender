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

#ifndef __GPUNODES_H__
#define __GPUNODES_H__

#include "GPU_extensions.h"
#include "GPU_node.h"

struct Image;
struct ImageUser;

#if 0
#define GPU_FLOAT	1
#define GPU_VEC2	2
#define GPU_VEC3	3
#define GPU_VEC4	4
#define GPU_MAT3	9
#define GPU_MAT4	16
/* types only used for defining */
#define GPU_TEX1D	1001
#define GPU_TEX2D	1002
#endif

#define GPU_RAND1	2001
#define GPU_RAND4	2004

#define GPU_VEC_UNIFORM	1
#define GPU_TEX_PIXEL 	2
#define GPU_TEX_RAND 	3
#define GPU_ARR_UNIFORM	4
#define GPU_S_ATTRIB	5

struct ListBase;
struct GPUProgram;
struct GPUNodeBuf;

struct GPUNode {
	struct GPUNode *next, *prev;

	char *name;
	//const char *code;
	int w, h;
	int randinput;

	ListBase inputs;
	ListBase outputs;

#if 0
	GPUShaderResources res;

	/* code generation */
	int sethi_ullman;
	int order;
	int scheduled;
	int ready;
	ListBase sortedinputs;
#endif
};

typedef struct GPUOutput {
	struct GPUOutput *next, *prev;

	GPUNode *node;

	int type;				/* data type = length of vector/matrix */
	char *name;
	
	struct GPUNodeBuf *buf;	/* output buffer, only has texture at runtime */

#if 0
	/* code generation */
	int numinputs;			/* number of inputs this node is connected to */
	int forcedbreak;		/* shader is forced to stop here */
#endif

	int id;					/* unique id as created by code generator */
} GPUOutput;

typedef struct GPUSortedInput {
	struct GPUSortedInput *next, *prev;
	struct GPUInput *input;
} GPUSortedInput;

typedef struct GPUInput {
	struct GPUInput *next, *prev;

	GPUNode *node;

	int type;				/* datatype */
	int arraysize;			/* number of elements in an array */
	int samp;
	char *name;				/* input name */

	int id;					/* unique id as created by code generator */
	int texid;				/* number for multitexture */
	int attribid;			/* id for vertex attributes */
	int bindtex;			/* input is responsible for binding the texture? */
	int definetex;			/* input is responsible for defining the pixel? */
	int textarget;			/* GL_TEXTURE_* */

	float vec[16];			/* vector data */
	struct GPUNodeBuf *buf;
	GPUTexture *tex;		/* input texture, only set at runtime */
	struct Image *ima;		/* image */
	struct ImageUser *iuser;/* image user */
	int attribtype;			/* attribute type */
	char attribname[32];	/* attribute name */
	int attribfirst;		/* this is the first one that is bound */

#if 0
	/* code generation */
	GPUSortedInput sort;
#endif
} GPUInput;

struct GPUNodeBuf {
	GPUTexture *tex;
	int type;
	int w, h;
	int users;
	int nocache;

	int xof, yof;
	float mat[2][2];

	float paintprex, paintprey;
	float paintpostx, paintposty;
	float paintmat[2][2];

	GPUOutput *source;
};

/* Node Buffer Functions */

GPUNodeBuf *GPU_node_buf_create(int w, int h, int type);
#if 0
void GPU_node_buf_write(GPUNodeBuf *buf, GPUFrameBuffer *fb, int type,
	unsigned char *pixels, float *fpixels);
void GPU_node_buf_read(GPUNodeBuf *buf, GPUFrameBuffer *fb, int type,
	float *pixels);

void GPU_node_buffers_texcoord(ListBase *nodes, int w, int h, float x, float y);
#endif

/* Node Functions */

GPUNode *GPU_node_begin(char *name, int w, int h);
//void GPU_node_resources(GPUNode *node, int alu, int tex, int constants);
void GPU_node_input(GPUNode *node, int type, char *name, void *p1, void *p2);
void GPU_node_input_array(GPUNode *node, int type, char *name, void *ptr1, void *ptr2, void *ptr3);
void GPU_node_output(GPUNode *node, int type, char *name, GPUNodeBuf **buf);
void GPU_node_end(GPUNode *node);

void GPU_node_free(GPUNode *node);
void GPU_nodes_free(ListBase *nodes);

#endif

