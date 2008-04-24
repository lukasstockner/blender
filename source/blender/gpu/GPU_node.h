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

#ifndef GPU_COMPOSITOR_H
#define GPU_COMPOSITOR_H

struct GPUNode;
struct GPUNodeBuf;
struct GPUTexture;

typedef struct GPUNode GPUNode;
typedef struct GPUNodeBuf GPUNodeBuf;

#if 0
typedef struct GPUCompSocket {
	float vec[4];
	GPUNodeBuf *buf;
} GPUCompSocket;
#endif

typedef enum GPUType {
	GPU_NONE = 0,
	GPU_FLOAT = 1,
	GPU_VEC2 = 2,
	GPU_VEC3 = 3,
	GPU_VEC4 = 4,
	GPU_MAT3 = 9,
	GPU_MAT4 = 16,
	GPU_TEX1D = 1001,
	GPU_TEX2D = 1002,
	GPU_ATTRIB = 2001
} GPUType;

typedef struct GPUNodeStack {
	GPUType type;
	char *name;
	float vec[4];
	struct GPUNodeBuf *buf;
	int hasinput;
} GPUNodeStack;

/* GPU Node Buffer
   - type may be 1/2/3/4 to indicate the number of channels
   - pixels can be NULL for creating a comp buf (not reading)
   - comp buf created based on a texture always has type=4 */

GPUNodeBuf *GPU_node_buf_from_texture(struct GPUTexture *tex);
void GPU_node_buf_free(GPUNodeBuf *buf);

void GPU_node_buf_draw(GPUNodeBuf *buf);
void GPU_node_buf_ref(GPUNodeBuf *buf);
struct GPUTexture *GPU_node_buf_texture(GPUNodeBuf *buf);
void GPU_node_buf_translation(GPUNodeBuf *buf, int xof, int yof);
void GPU_node_buf_rotation(GPUNodeBuf *buf, float rotation);
int GPU_node_buf_width(GPUNodeBuf *buf);
int GPU_node_buf_height(GPUNodeBuf *buf);

#if 0
/* GPU Compositor:
	- call GPU_comp_begin
	- call GPU_comp_* functions to add nodes in the queue, make sure to free
	  the returned buffers with GPU_node_buf_free
	- call GPU_comp_flush for one or more buffers
	- get the GPUTexture from a GPUNodeBuf with, ref it, and use for display

	- note that nodes are not immediately executed, and the contents of the
	  returned GPUNodeBuf's may therefore be empty until GPU_comp_flush_buf is
	  called.

	- on program exit, or to clear all GPU buffers, call GPU_comp_free */

void GPU_comp_begin();
void GPU_comp_flush_buf(GPUNodeBuf *buf);
void GPU_comp_end();
void GPU_comp_free();

GPUNodeBuf *GPU_comp_buf_write(int w, int h, int type,
	unsigned char *pixels, float *fpixels);
void GPU_comp_buf_read(GPUNodeBuf *buf, int type, float *pixels);

void GPU_comp_image(int w, int h, GPUNodeBuf *image, GPUNodeBuf **outbuf);
void GPU_comp_normal(int w, int h, GPUCompSocket dir, GPUNodeBuf **norbuf, GPUNodeBuf **dotbuf);
void GPU_comp_curves_rgb(int w, int h, GPUCompSocket col, float *pixels, int length, GPUCompSocket fac, GPUNodeBuf **outbuf);
void GPU_comp_hue_sat(int w, int h, GPUCompSocket col, float *hue, float *sat, float *value, GPUCompSocket fac, GPUNodeBuf **outbuf);
void GPU_comp_blend(int w, int h, GPUCompSocket col1, GPUCompSocket col2, int blendmode, GPUCompSocket fac, GPUNodeBuf **outbuf);
void GPU_comp_filter(int w, int h, GPUCompSocket col, int filter, float *mat, GPUCompSocket fac, GPUNodeBuf **outbuf);
void GPU_comp_valtorgb(int w, int h, GPUCompSocket fac, float *pixels, int length, GPUNodeBuf **outbuf, GPUNodeBuf **alphabuf);
void GPU_comp_rgbtobw(int w, int h, GPUCompSocket col, GPUNodeBuf **outbuf);
void GPU_comp_alphaover(int w, int h, GPUCompSocket src, GPUCompSocket over, int key, GPUCompSocket fac, GPUNodeBuf **outbuf);
void GPU_comp_zcombine(int w, int h, GPUCompSocket col1, GPUCompSocket z1, GPUCompSocket col2, GPUCompSocket z2, GPUNodeBuf **outbuf);
void GPU_comp_mapvalue(int w, int h, GPUCompSocket val, float loc, float size, float min, float max, int domin, int domax, GPUNodeBuf **outbuf);
void GPU_comp_blur(int w, int h, GPUCompSocket col, float *gaussx, int radx, float *gaussy, int rady, GPUCompSocket fac, GPUNodeBuf **outbuf);
void GPU_comp_setalpha(int w, int h, GPUCompSocket col, GPUCompSocket alpha, GPUNodeBuf **outbuf);
void GPU_comp_sephsva(int w, int h, GPUCompSocket col, GPUNodeBuf **outh, GPUNodeBuf **outs, GPUNodeBuf **outv, GPUNodeBuf **outa);
void GPU_comp_seprgba(int w, int h, GPUCompSocket col, GPUNodeBuf **outr, GPUNodeBuf **outg, GPUNodeBuf **outb, GPUNodeBuf **outa);
void GPU_comp_combrgba(int w, int h, GPUCompSocket r, GPUCompSocket g, GPUCompSocket b, GPUCompSocket a, GPUNodeBuf **outbuf);
void GPU_comp_dilateerode(int w, int h, GPUCompSocket val, int iterations, GPUNodeBuf **outbuf);
void GPU_comp_rotate(int w, int h, GPUCompSocket col, float rotation, GPUNodeBuf **outbuf);
void GPU_comp_wipe(int w, int h, GPUCompSocket col1, GPUCompSocket col2, float angle, int type, int forward, float fac, GPUNodeBuf **outbuf);
void GPU_comp_gammacross(int w, int h, GPUCompSocket col1, GPUCompSocket col2, float *pixels, float *invpixels, int length, GPUCompSocket fac, GPUNodeBuf **outbuf);
#endif

#endif

