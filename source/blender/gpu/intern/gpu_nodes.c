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

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "DNA_color_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"

#include "BLI_blenlib.h"

#include "GPU_node.h"
#include "GPU_extensions.h"

#include "gpu_nodes.h"
#include "gpu_codegen.h"

#include <string.h>
#include <math.h>

/* Node Buffer Functions */

#if 0
static GLenum GPU_TYPE_FORMAT[4] = {GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA};
#endif

GPUNodeBuf *GPU_node_buf_create(int w, int h, int type)
{
	GPUNodeBuf *buf = MEM_callocN(sizeof(GPUNodeBuf), "GPUNodeBuf");
	buf->w = w;
	buf->h = h;
	buf->type = type;
	buf->users++;
	buf->mat[0][0] = buf->mat[1][1] = 1.0f;

	return buf;
}

void GPU_node_buf_free(GPUNodeBuf *buf)
{
	buf->users--;

	if (buf->users < 0)
		fprintf(stderr, "GPU_node_buf_free: negative refcount\n");
	
	if (buf->users == 0) {
#if 0
		if (buf->tex) {
			if (buf->nocache)
				GPU_texture_free(buf->tex);
			else
				GPU_texture_cache_free(buf->tex);
		}
#endif
		if (buf->source)
			buf->source->buf = NULL;
		MEM_freeN(buf);
	}
}

void GPU_node_buf_ref(GPUNodeBuf *buf)
{
	buf->users++;
}

#if 0
void GPU_node_buf_write(GPUNodeBuf *buf, GPUFrameBuffer *fb, int type, unsigned char *pixels, float *fpixels)
{
	GPUTexture *tex = buf->tex;
	int w = buf->w;
	int h = buf->h;

	/* todo:
		- ATI should use glDrawPixels
		- NVidia and other should use glTexSubImage2d */

	GPU_framebuffer_texture_bind(fb, buf->tex);
	if (!pixels || (GPU_texture_width(tex) != GPU_texture_opengl_width(tex)) ||
	    (GPU_texture_height(tex) != GPU_texture_opengl_height(tex))) {
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	if (pixels || fpixels) {
		glRasterPos2i(0, 0);

		if (fpixels)
			glDrawPixels(w, h, GPU_TYPE_FORMAT[type-1], GL_FLOAT, fpixels);
		else
			glDrawPixels(w, h, GPU_TYPE_FORMAT[type-1], GL_UNSIGNED_BYTE, pixels);
	}
	GPU_framebuffer_texture_unbind(fb, buf->tex);
}

void GPU_node_buf_read(GPUNodeBuf *buf, GPUFrameBuffer *fb, int type, float *pixels)
{
	GPU_framebuffer_texture_bind(fb, buf->tex);
	glRasterPos2i(0, 0);
	glReadPixels(0, 0, buf->w, buf->h, GPU_TYPE_FORMAT[type-1], GL_FLOAT, pixels);
	GPU_framebuffer_texture_unbind(fb, buf->tex);
}

GPUTexture *GPU_node_buf_texture(GPUNodeBuf *buf)
{
	return buf->tex;
}

void GPU_node_buf_translation(GPUNodeBuf *buf, int xof, int yof)
{
	buf->xof = xof;
	buf->yof = yof;
}

void GPU_node_buf_rotation(GPUNodeBuf *buf, float rotation)
{
	float rad= (M_PI*rotation)/180.0f;
	float s= sin(rad);
	float c= cos(rad);

	buf->mat[0][0] = c;
	buf->mat[0][1] = s;
	buf->mat[1][0] = -s;
	buf->mat[1][1] = c;
}

void node_buf_update_transform(GPUNodeBuf *buf, int w, int h)
{
	float scalex = (float)w/(float)buf->w;
	float scaley = (float)h/(float)buf->h;

	buf->paintprex = -0.5f;
	buf->paintprey = -0.5f;
	buf->paintpostx = (0.5f - (w/2 - buf->w/2 + buf->xof)/(float)w)*scalex;
	buf->paintposty = (0.5f - (h/2 - buf->h/2 + buf->yof)/(float)h)*scaley;

	buf->paintmat[0][0] = buf->mat[0][0]*scalex;
	buf->paintmat[1][0] = buf->mat[1][0]*scalex*buf->w/(float)buf->h;
	buf->paintmat[0][1] = buf->mat[0][1]*scaley*buf->h/(float)buf->w;
	buf->paintmat[1][1] = buf->mat[1][1]*scaley;
}

void GPU_node_buffers_texcoord(ListBase *nodes, int w, int h, float x, float y)
{
	GPUNode *node;
	GPUInput *input;
	GPUNodeBuf *buf;
	float s, t, u, v;

	/* we transform the verts manually, seems easier and faster than using
	   the texture matrix in vertex or fragment shaders */
	for (node=nodes->first; node; node=node->next) {
		for (input=node->inputs.first; input; input=input->next) {
			if (input->buf && input->tex && input->bindtex) {
				buf= input->buf;

				node_buf_update_transform(buf, w, h);

				u = x + buf->paintprex;
				v = y + buf->paintprey;
				s= buf->paintmat[0][0]*u + buf->paintmat[0][1]*v + buf->paintpostx;
				t= buf->paintmat[1][0]*u + buf->paintmat[1][1]*v + buf->paintposty;
				GPU_texture_coord_2f(input->tex, s, t);
			}
		}
	}
}

int GPU_node_buf_width(GPUNodeBuf *buf)
{
	return buf->w;
}

int GPU_node_buf_height(GPUNodeBuf *buf)
{
	return buf->h;
}
#endif

/* Node Functions */

GPUNode *GPU_node_begin(char *name, int w, int h)
{
	GPUNode *node = MEM_callocN(sizeof(GPUNode), "GPUNode");

	node->name = name;
	node->w = w;
	node->h = h;

#if 0
	node->res.alu_instructions = 0; //64;
	node->res.tex_instructions = 0; //32;
	node->res.constants = 0; //32;
#endif

	return node;
}

#if 0
void GPU_node_resources(GPUNode *node, int alu, int tex, int constants)
{
	node->res.alu_instructions = alu;
	node->res.tex_instructions = tex;
	node->res.constants = constants;
}
#endif

void GPU_node_end(GPUNode *node)
{
	/* empty */
}

void GPU_node_input_array(GPUNode *node, int type, char *name, void *ptr1, void *ptr2, void *ptr3)
{
	GPUInput *input = MEM_callocN(sizeof(GPUInput), "GPUInput");

	input->name = name;
	input->node = node;
	
	if (type == GPU_ATTRIB) {
		input->type = *((int*)ptr1);
		input->samp = GPU_S_ATTRIB;

		input->attribtype = *((int*)ptr2);
		BLI_strncpy(input->attribname, (char*)ptr3, sizeof(input->attribname));
	}
	else if ((type == GPU_TEX1D) || (type == GPU_TEX2D)) {
		if(ptr1 && ptr2) {
			int length = *((int*)ptr1);
			float *pixels = ((float*)ptr2);

			input->type = GPU_VEC4;
			input->samp = GPU_TEX_RAND;

			if (type == GPU_TEX1D) {
				input->tex = GPU_texture_create_1D(length, pixels, 1);
				input->textarget = GL_TEXTURE_1D;
			}
			else {
				input->tex = GPU_texture_create_2D(length, length, pixels, 1);
				input->textarget = GL_TEXTURE_2D;
			}
		}
		else {
			input->type = GPU_VEC4;
			input->samp = GPU_TEX_RAND;
			input->ima = (Image*)ptr1;
			input->textarget = GL_TEXTURE_2D;
		}

#if 0
		node->res.uniforms += 4;
#endif
	}
	else if ((type == GPU_RAND1) || (type == GPU_RAND4)) {
		GPUNodeBuf *buf = ((GPUNodeBuf*)ptr1);

		input->type = (type == GPU_RAND1)? GPU_FLOAT: GPU_VEC4;
		input->samp = GPU_TEX_RAND;
		input->textarget = GL_TEXTURE_2D;

		input->buf = buf;
		GPU_node_buf_ref(buf);

#if 0
		node->res.uniforms += 4;
#endif
	}
	else {
		float *vec = ((float*)ptr1);
		GPUNodeBuf *buf = ((GPUNodeBuf*)ptr2);
		int length = type;

		input->type = type;

		if (buf) {
			input->samp = GPU_TEX_PIXEL;
			input->textarget = GL_TEXTURE_2D;
			input->buf = buf;
			GPU_node_buf_ref(buf);
#if 0
			node->res.uniforms += 4;
#endif
		}
		else {
			if (ptr3) {
				int arraysize = *((int*)ptr3);
				input->samp = GPU_ARR_UNIFORM;
				input->arraysize = arraysize;
				memcpy(input->vec, vec, length*arraysize*sizeof(float));
#if 0
				node->res.uniforms += length*arraysize;
#endif
			}
			else {
				input->samp = GPU_VEC_UNIFORM;
				memcpy(input->vec, vec, length*sizeof(float));
#if 0
				node->res.uniforms += length;
#endif
			}
		}
	}

	BLI_addtail(&node->inputs, input);
}

void GPU_node_input(GPUNode *node, int type, char *name, void *ptr1, void *ptr2)
{
	GPU_node_input_array(node, type, name, ptr1, ptr2, NULL);
}

void GPU_node_output(GPUNode *node, int type, char *name, GPUNodeBuf **buf)
{
	GPUOutput *output = MEM_callocN(sizeof(GPUOutput), "GPUOutput");

	output->type = type;
	output->name = name;
	output->node = node;

	if (buf) {
		*buf = output->buf = GPU_node_buf_create(node->w, node->h, type);
		output->buf->source = output;

		/* note: the caller owns the reference to the buffer, GPUOutput
		   merely points to it, and if the node is destroyed it will
		   set that pointer to NULL */
	}

	BLI_addtail(&node->outputs, output);
}

void GPU_node_free(GPUNode *node)
{
	GPUInput *input;
	GPUOutput *output;

	for (input=node->inputs.first; input; input=input->next) {
		if (input->buf) {
			GPU_node_buf_free(input->buf);
		}
		else if (input->tex)
			GPU_texture_free(input->tex);
	}

	for (output=node->outputs.first; output; output=output->next)
		if (output->buf) {
			output->buf->source = NULL;
			GPU_node_buf_free(output->buf);
		}

	BLI_freelistN(&node->inputs);
	BLI_freelistN(&node->outputs);
	MEM_freeN(node);
}

void GPU_nodes_free(ListBase *nodes)
{
	GPUNode *node;

	while (nodes->first) {
		node = nodes->first;
		BLI_remlink(nodes, node);
		GPU_node_free(node);
	}
}

