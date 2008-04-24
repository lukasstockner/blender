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

#if 0
#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "DNA_color_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"

#include "BLI_blenlib.h"

#include "GPU_node.h"
#include "GPU_extensions.h"

#include "gpu_codegen.h"
#include "gpu_nodes.h"
#include "gpu_shaders.h"

#include <string.h>
#include <math.h>

struct GPUComposite {
	ListBase nodes;
	GPUFrameBuffer *fb;
} _composite = {{NULL, NULL}, NULL};

/* Functions */

static void comp_paint_framebuffer(ListBase *nodes, GPUTexture *tex, int w, int h)
{
	GPU_framebuffer_texture_bind(_composite.fb, tex);

	glClearColor(0.1, 0.1, 0.1, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glBegin(GL_QUADS);
	GPU_node_buffers_texcoord(nodes, w, h, 0.0f, 0.0f);
	glVertex2f(0.0f, 0.0f);
	GPU_node_buffers_texcoord(nodes, w, h, 1.0f, 0.0f);
	glVertex2f(1.0f, 0.0f);
	GPU_node_buffers_texcoord(nodes, w, h, 1.0f, 1.0f);
	glVertex2f(1.0f, 1.0f);
	GPU_node_buffers_texcoord(nodes, w, h, 0.0f, 1.0f);
	glVertex2f(0.0f, 1.0f);
	glEnd();

	GPU_framebuffer_texture_unbind(_composite.fb, tex);
}

static void comp_execute_pass(GPUPass *pass)
{
	GPUOutput *output = pass->output;
	GPUNode *node = output->node;
	GPUTexture *tex;

	/* TODO: move this past compile? */
	tex = GPU_texture_cache_create(node->w, node->h, 1, _composite.fb);
	output->buf->tex = tex;
	if (!tex)
		return;

	GPU_pass_bind(pass);
	comp_paint_framebuffer(&pass->nodes, tex, node->w, node->h);
	GPU_pass_unbind(pass);
}

void GPU_comp_begin()
{
	if (!_composite.fb)
		_composite.fb = GPU_framebuffer_create();
}

void GPU_comp_end()
{
	GPU_nodes_free(&_composite.nodes);

	GPU_texture_cache_free_untagged();
	GPU_texture_cache_untag();

	GPU_framebuffer_restore();
}

void GPU_comp_free()
{
	GPU_texture_cache_untag();
	GPU_texture_cache_free_untagged();

	GPU_nodes_free(&_composite.nodes);

	if (_composite.fb) {
		GPU_framebuffer_free(_composite.fb);
		_composite.fb= NULL;
	}

	memset(&_composite, 0, sizeof(_composite));
}

void GPU_comp_flush_buf(GPUNodeBuf *buf)
{
	ListBase passes;
	GPUPass *pass;

	if (buf->source) {
		passes = GPU_generate_passes(&_composite.nodes, buf->source, 0);

		while (passes.first) {
			pass = passes.first;

			comp_execute_pass(pass);
			GPU_pass_free(&passes, pass);
		}
	}
}

GPUNodeBuf *GPU_comp_buf_write(int w, int h, int type, unsigned char *pixels, float *fpixels)
{
	GPUNodeBuf *buf = GPU_node_buf_create(w, h, type);

	/* TODO: it is unnecessary to create attach these to a framebuffer,
	   but otherwise can't do glDrawPixels/glReadPixels, need to replace
	   those with something else (ideally PBO's for speed) */
	buf->tex = GPU_texture_cache_create(w, h, (fpixels)? 1: 0, _composite.fb);

	if (!buf->tex) {
		GPU_node_buf_free(buf);
		return NULL;
	}
	else {
		GPU_node_buf_write(buf, _composite.fb, type, pixels, fpixels);
		return buf;
	}
}

void GPU_comp_buf_read(GPUNodeBuf *buf, int type, float *pixels)
{
	GPU_node_buf_read(buf, _composite.fb, type, pixels);
}

/* Node Implementations */

void GPU_comp_image(int w, int h, GPUNodeBuf *image, GPUNodeBuf **outbuf)
{
	GPUNode *node;
	
	node = GPU_node_begin("image", w, h);
	GPU_node_resources(node, 0, 1, 0);

	GPU_node_input(node, GPU_RAND4, "img,st", image, NULL);
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);

	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_normal(int w, int h, GPUCompSocket dir, GPUNodeBuf **norbuf, GPUNodeBuf **dotbuf)
{
	GPUNode *node;
	
	node = GPU_node_begin("normal", w, h);
	GPU_node_resources(node, 1, 0, 0);

	GPU_node_input(node, GPU_VEC3, "dir", dir.vec, dir.buf);

	GPU_node_output(node, GPU_VEC3, "nor", norbuf);
	GPU_node_output(node, GPU_FLOAT, "dot", dotbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_curves_rgb(int w, int h, GPUCompSocket col, float *pixels, int length, GPUCompSocket fac, GPUNodeBuf **outbuf)
{
	GPUNode *node;
	
	node = GPU_node_begin("curves_rgb", w, h);
	GPU_node_resources(node, 32, 3, 2);

	GPU_node_input(node, GPU_VEC4, "col", col.vec, col.buf);
	GPU_node_input(node, GPU_TEX1D, "curvemap", &length, pixels);
	GPU_node_input(node, GPU_FLOAT,"fac", fac.vec, fac.buf);

	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_hue_sat(int w, int h, GPUCompSocket col, float *hue, float *sat, float *value, GPUCompSocket fac, GPUNodeBuf **outbuf)
{
	/* this node is in 4 steps, for two reasons:
		- the sum of the instructions in the conversion functions exceeds
		  the instruction limits on some cards
		- allows to reuse code */

	GPUNodeBuf *buf1, *buf2, *buf3;
	GPUNode *node;

	node = GPU_node_begin("rgb_to_hsv", w, h);
	GPU_node_input(node, GPU_VEC4, "rgb", col.vec, col.buf);
	GPU_node_output(node, GPU_VEC4, "outcol", &buf1);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	node = GPU_node_begin("hue_sat", w, h);
	GPU_node_input(node, GPU_VEC4, "hsv", NULL, buf1);
	GPU_node_input(node, GPU_FLOAT, "hue", hue, NULL);
	GPU_node_input(node, GPU_FLOAT, "sat", sat, NULL);
	GPU_node_input(node, GPU_FLOAT, "value", value, NULL);
	GPU_node_output(node, GPU_VEC4, "outcol", &buf2);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	node = GPU_node_begin("hsv_to_rgb", w, h);
	GPU_node_input(node, GPU_VEC4, "hsv", NULL, buf2);
	GPU_node_output(node, GPU_VEC4, "outcol", &buf3);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	node = GPU_node_begin("blend_mix", w, h);
	GPU_node_resources(node, 4, 0, 0);
	GPU_node_input(node, GPU_VEC4, "col1", col.vec, col.buf);
	GPU_node_input(node, GPU_VEC4, "col2", NULL, buf3);
	GPU_node_input(node, GPU_FLOAT, "fac", fac.vec, fac.buf);
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	GPU_node_buf_free(buf1);
	GPU_node_buf_free(buf2);
	GPU_node_buf_free(buf3);
}

void GPU_comp_blend(int w, int h, GPUCompSocket col1, GPUCompSocket col2, int blendmode, GPUCompSocket fac, GPUNodeBuf **outbuf)
{
	GPUNode *node;
	static char *blend_shader_names[16] = {"blend_mix", "blend_add",
		"blend_mult", "blend_sub", "blend_screen", "blend_div", "blend_diff",
		"blend_dark", "blend_light", "blend_overlay", "blend_dodge",
		"blend_burn", "blend_hue", "blend_sat", "blend_val", "blend_color"};

	if ((blendmode >= 16) || (blend_shaders[blendmode] == NULL))
		blendmode = 0;

	node = GPU_node_begin(blend_shader_names[blendmode], w, h);

	GPU_node_input(node, GPU_VEC4, "col1", col1.vec, col1.buf);
	GPU_node_input(node, GPU_VEC4, "col2", col2.vec, col2.buf);
	GPU_node_input(node, GPU_FLOAT, "fac", fac.vec, fac.buf);

	//GPU_node_code(blend_shaders[blendmode]);

	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);

	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_filter(int w, int h, GPUCompSocket col, int edge, float *mat, GPUCompSocket fac, GPUNodeBuf **outbuf)
{
	float dx = 0.5f/w, dy = 0.5f/h; /* why 0.5? */
	GPUNodeBuf *buf;
	GPUNode *node;

	node = GPU_node_begin(edge? "filter_edge": "filter", w, h);

	GPU_node_input(node, GPU_RAND4, "tex", col.buf, NULL);
	GPU_node_input(node, GPU_FLOAT, "dx", &dx, NULL);
	GPU_node_input(node, GPU_FLOAT, "dy", &dy, NULL);
	GPU_node_input(node, GPU_MAT3, "fmat", mat, NULL);

	//GPU_node_code(edge? filter_edge_shader: filter_shader);

	GPU_node_output(node, GPU_VEC4, "outcol", &buf);

	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	node = GPU_node_begin("blend_mix", w, h);
	GPU_node_input(node, GPU_VEC4, "col1", col.vec, col.buf);
	GPU_node_input(node, GPU_VEC4, "col2", NULL, buf);
	GPU_node_input(node, GPU_FLOAT, "fac", fac.vec, fac.buf);
	//GPU_node_code(blend_mix_shader);
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	GPU_node_buf_free(buf);
}

void GPU_comp_valtorgb(int w, int h, GPUCompSocket fac, float *pixels, int length, GPUNodeBuf **outbuf, GPUNodeBuf **alphabuf)
{
	GPUNode *node;

	/* generate code */
	node = GPU_node_begin("valtorgb", w, h);
	GPU_node_input(node, GPU_FLOAT, "fac", fac.vec, fac.buf);
	GPU_node_input(node, GPU_TEX1D, "colormap", &length, pixels);
	//GPU_node_code("	outcol = texture1D(colormap, fac);\n");
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	if (alphabuf) {
		node = GPU_node_begin("valtorgb_alpha", w, h);
		GPU_node_input(node, GPU_FLOAT, "fac", fac.vec, fac.buf);
		GPU_node_input(node, GPU_TEX1D, "colormap", &length, pixels);
		//GPU_node_code("	outalpha = texture1D(colormap, fac).a;\n");
		GPU_node_output(node, GPU_FLOAT, "outalpha", outbuf);
		GPU_node_end(node);
		BLI_addtail(&_composite.nodes, node);
	}
}

void GPU_comp_rgbtobw(int w, int h, GPUCompSocket col, GPUNodeBuf **outbuf)
{
	GPUNode *node;

	node = GPU_node_begin("rgb_to_bw", w, h);

	GPU_node_input(node, GPU_VEC4, "col", col.vec, col.buf);
	//GPU_node_code("	outcol = col.r*0.35 + col.g*0.45 + col.b*0.2;\n");
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);

	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_seprgba(int w, int h, GPUCompSocket col, GPUNodeBuf **outr, GPUNodeBuf **outg, GPUNodeBuf **outb, GPUNodeBuf **outa)
{
	GPUNode *node;

	if (outr) {
		node = GPU_node_begin("sep_rgba_r", w, h);
		GPU_node_input(node, GPU_VEC4, "col", col.vec, col.buf);
		//GPU_node_code("	r = col.r;\n");
		GPU_node_output(node, GPU_FLOAT, "r", outr);
		GPU_node_end(node);
		BLI_addtail(&_composite.nodes, node);
	}
	if (outg) {
		node = GPU_node_begin("sep_rgba_g", w, h);
		GPU_node_input(node, GPU_VEC4, "col", col.vec, col.buf);
		//GPU_node_code("	g = col.g;\n");
		GPU_node_output(node, GPU_FLOAT, "g", outg);
		GPU_node_end(node);
		BLI_addtail(&_composite.nodes, node);
	}
	if (outb) {
		node = GPU_node_begin("sep_rgba_b", w, h);
		GPU_node_input(node, GPU_VEC4, "col", col.vec, col.buf);
		//GPU_node_code("	b = col.b;\n");
		GPU_node_output(node, GPU_FLOAT, "b", outb);
		GPU_node_end(node);
		BLI_addtail(&_composite.nodes, node);
	}
	if (outa) {
		node = GPU_node_begin("sep_rgba_a", w, h);
		GPU_node_input(node, GPU_VEC4, "col", col.vec, col.buf);
		//GPU_node_code("	a = col.a;\n");
		GPU_node_output(node, GPU_FLOAT, "a", outa);
		GPU_node_end(node);
		BLI_addtail(&_composite.nodes, node);
	}
}

void GPU_comp_sephsva(int w, int h, GPUCompSocket col, GPUNodeBuf **outh, GPUNodeBuf **outs, GPUNodeBuf **outv, GPUNodeBuf **outa)
{
	GPUNodeBuf *buf;
	GPUNode *node;

	node = GPU_node_begin("rgb_to_hsv", w, h);
	GPU_node_input(node, GPU_VEC4, "rgb", col.vec, col.buf);
	//GPU_node_code(rgb_to_hsv_shader);
	GPU_node_output(node, GPU_VEC4, "outcol", &buf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	col.buf = buf;
	GPU_comp_seprgba(w, h, col, outh, outs, outv, outa);

	GPU_node_buf_free(buf);
}

void GPU_comp_setalpha(int w, int h, GPUCompSocket col, GPUCompSocket alpha, GPUNodeBuf **outbuf)
{
	GPUNodeBuf *buf;
	GPUNode *node;

	node = GPU_node_begin("set_alpha", w, h);
	GPU_node_input(node, GPU_VEC4, "rgb", col.vec, col.buf);
	GPU_node_input(node, GPU_FLOAT, "alpha", alpha.vec, alpha.buf);
	//GPU_node_code("	outcol = vec4(rgb.rgb, alpha);\n");
	GPU_node_output(node, GPU_VEC4, "outcol", &buf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_alphaover(int w, int h, GPUCompSocket src, GPUCompSocket over, int key, GPUCompSocket fac, GPUNodeBuf **outbuf)
{
	GPUNode *node = GPU_node_begin(key? "alpha_over_key": "alpha_over_premul", w, h);
	GPU_node_input(node, GPU_VEC4, "src", src.vec, src.buf);
	GPU_node_input(node, GPU_VEC4, "over", over.vec, over.buf);
	GPU_node_input(node, GPU_FLOAT, "fac", fac.vec, fac.buf);
	//GPU_node_code(key? alpha_over_key_shader: alpha_over_premul_shader);
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_zcombine(int w, int h, GPUCompSocket col1, GPUCompSocket z1, GPUCompSocket col2, GPUCompSocket z2, GPUNodeBuf **outbuf)
{
	GPUNode *node;
	
	node = GPU_node_begin("zcombine", w, h);
	GPU_node_input(node, GPU_VEC4, "col1", col1.vec, col1.buf);
	GPU_node_input(node, GPU_FLOAT, "z1", z1.vec, z1.buf);
	GPU_node_input(node, GPU_VEC4, "col2", col2.vec, col2.buf);
	GPU_node_input(node, GPU_FLOAT, "z2", z2.vec, z2.buf);
	/*GPU_node_code(
	"	if (z1 < z2)\n"
	"		outcol = col1;\n"
	"	else\n"
	"		outcol = col2;\n");*/
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_mapvalue(int w, int h, GPUCompSocket val, float loc, float size, float min, float max, int domin, int domax, GPUNodeBuf **outbuf)
{
	float dominf = domin, domaxf = domax;
	GPUNode *node;
	
	node = GPU_node_begin("map_value", w, h);

	GPU_node_input(node, GPU_FLOAT, "val", val.vec, val.buf);
	GPU_node_input(node, GPU_FLOAT, "loc", &loc, NULL);
	GPU_node_input(node, GPU_FLOAT, "size", &size, NULL);
	GPU_node_input(node, GPU_FLOAT, "mni", &min, NULL);
	GPU_node_input(node, GPU_FLOAT, "mxa", &max, NULL);
	GPU_node_input(node, GPU_FLOAT, "domin", &dominf, NULL);
	GPU_node_input(node, GPU_FLOAT, "domax", &domaxf, NULL);
	/*GPU_node_code(
	"	val = (val + loc)*size;\n"
	"	if (bool(domin) && (val < mni)) val = mni;\n"
	"	if (bool(domax) && (val > mxa)) val = mxa;\n"
	"	outval = val;\n");*/
	GPU_node_output(node, GPU_FLOAT, "outval", outbuf);

	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_blur(int w, int h, GPUCompSocket col, float *gaussx, int radx, float *gaussy, int rady, GPUCompSocket fac, GPUNodeBuf **outbuf)
{
#if 0
	int pixelsleft = radx, maxpixels = 16, numpixels, curpixel = 0;
	GPUNodeBuf *buf1 = col.buf, *buf2;
	float *offsets;

	while (pixelsleft > 0) {
		GPUNode *node = GPU_node_begin("blursep", w, h);
		numpixels = (pixelsleft > maxpixels)? maxpixels: pixelsleft;

		GPU_node_input(node, GPU_RAND4, "tex", buf1, NULL);
		GPU_node_input(node, GPU_RAND4, "blendtex", col.buf, NULL);
		GPU_node_input_array(GPU_FLOAT, "weights", gaussx+curpixel, NULL, &numpixels);
		GPU_node_input_array(GPU_VEC2, "offsets", offsets+curpixel, NULL, &numpixels);
		GPU_node_code(blursep_shader);
		GPU_node_output(node, GPU_VEC4, "outcol", &buf2);

		curpixel += numpixels;
		GPU_node_end(node);
		BLI_addtail(&_composite.nodes, node);

		if (buf1 != col.buf) {
			GPU_node_buf_free(buf1);
			buf1 = NULL;
		}

		buf1 = buf2;
		buf2 = NULL;
	}

	if (!buf1) {
		*outbuf = col.buf;
		GPU_node_buf_ref(col.buf);
	}
	else
		*outbuf = buf1; /* take over reference from buf1 */

	float dx = 1.0f/w, dy = 1.0f/h;

	GPUNode *node = GPU_node_begin("blurh", w, h);
	GPU_node_input(node, GPU_RAND4, "tex", col.buf, NULL);
	GPU_node_input(node, GPU_TEX1D, "gausstab", &radx, gaussx);
	GPU_node_input(node, GPU_FLOAT, "dx", &dx, NULL);
	GPU_node_input(node, GPU_FLOAT, "dy", &dy, NULL);
	GPU_node_code(blur_hor_shader);
	GPU_node_output(node, GPU_VEC4, "outcol", &buf1);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	GPUNode *node = GPU_node_begin("blurv", w, h);
	GPU_node_input(node, GPU_RAND4, "tex", buf1, NULL);
	GPU_node_input(node, GPU_TEX1D, "gausstab", &rady, gaussy);
	GPU_node_input(node, GPU_FLOAT, "dx", &dx, NULL);
	GPU_node_input(node, GPU_FLOAT, "dy", &dy, NULL);
	GPU_node_code(blur_ver_shader);
	GPU_node_output(node, GPU_VEC4, "outcol", &buf2);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	GPUNode *node = GPU_node_begin("blend_mix", w, h);
	GPU_node_input(node, GPU_VEC4, "col1", col.vec, col.buf);
	GPU_node_input(node, GPU_VEC4, "col2", NULL, buf2);
	GPU_node_input(node, GPU_FLOAT, "fac", fac.vec, fac.buf);
	GPU_node_code(blend_mix_shader);
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	GPU_node_buf_free(buf1);
	GPU_node_buf_free(buf2);
#endif
}

void GPU_comp_combrgba(int w, int h, GPUCompSocket r, GPUCompSocket g, GPUCompSocket b, GPUCompSocket a, GPUNodeBuf **outbuf)
{
	GPUNode *node;
	
	node = GPU_node_begin("comb_rgba", w, h);
	GPU_node_input(node, GPU_FLOAT, "r", r.vec, r.buf);
	GPU_node_input(node, GPU_FLOAT, "g", g.vec, g.buf);
	GPU_node_input(node, GPU_FLOAT, "b", b.vec, b.buf);
	GPU_node_input(node, GPU_FLOAT, "a", a.vec, a.buf);
	//GPU_node_code("	outcol = vec4(r, g, b, a);\n");
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_dilateerode(int w, int h, GPUCompSocket val, int iterations, GPUNodeBuf **outbuf)
{
	GPUNodeBuf *buf1 = NULL, *buf2 = NULL;
	float dx = 0.5f/w, dy = 0.5f/h; /* why 0.5f? */
	int i, dilate = 1;
	GPUNode *node;

	if (iterations == 0) {
		node = GPU_node_begin("empty_dilateerode", w, h);
		GPU_node_input(node, GPU_FLOAT, "val", NULL, val.buf);
		//GPU_node_code("	outval = val;\n");
		GPU_node_output(node, GPU_FLOAT, "outval", outbuf);
		GPU_node_end(node);
		BLI_addtail(&_composite.nodes, node);
	}
	else {
		if (iterations < 0) {
			iterations = -iterations;
			dilate = 0;
		}

		buf1 = val.buf;

		for (i = 0; i < iterations; i++) {
			node = GPU_node_begin(dilate? "dilate": "erode", w, h);
			GPU_node_input(node, GPU_RAND1, "tex", buf1, NULL);
			GPU_node_input(node, GPU_FLOAT, "dx", &dx, NULL);
			GPU_node_input(node, GPU_FLOAT, "dy", &dy, NULL);
			//GPU_node_code(dilate? dilate_shader: erode_shader);
			GPU_node_output(node, GPU_FLOAT, "outval", &buf2);
			GPU_node_end(node);
			BLI_addtail(&_composite.nodes, node);

			if (buf1 != val.buf) {
				GPU_node_buf_free(buf1);
				buf1 = NULL;
			}

			buf1 = buf2;
			buf2 = NULL;
		}

		*outbuf = buf1; /* take over reference from buf1 */
	}
}

void GPU_comp_rotate(int w, int h, GPUCompSocket col, float rotation, GPUNodeBuf **outbuf)
{
	GPUNode *node;

	GPU_node_buf_rotation(col.buf, rotation);

	node = GPU_node_begin("rotate", w, h);
	GPU_node_input(node, GPU_VEC4, "col", col.vec, col.buf);
	//GPU_node_code("	outcol = col;\n");
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}

void GPU_comp_gammacross(int w, int h, GPUCompSocket col1, GPUCompSocket col2, float *pixels, float *invpixels, int length, GPUCompSocket fac, GPUNodeBuf **outbuf)
{
	GPUNodeBuf *buf1, *buf2, *buf;
	GPUCompSocket sock1, sock2;
	GPUNode *node;

	memset(&sock1, 0, sizeof(sock1));
	memset(&sock2, 0, sizeof(sock2));

	node = GPU_node_begin("gammacorrect", w, h);
	GPU_node_input(node, GPU_VEC4, "col", col1.vec, col1.buf);
	GPU_node_input(node, GPU_TEX1D, "gamma", &length, invpixels);
	//GPU_node_code(gamma_shader);
	GPU_node_output(node, GPU_VEC4, "outcol", &buf1);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	node = GPU_node_begin("gammacorrect", w, h);
	GPU_node_input(node, GPU_VEC4, "col", col2.vec, col2.buf);
	GPU_node_input(node, GPU_TEX1D, "gamma", &length, invpixels);
	//GPU_node_code(gamma_shader);
	GPU_node_output(node, GPU_VEC4, "outcol", &buf2);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	sock1.buf = buf1;
	sock2.buf = buf2;
	GPU_comp_blend(w, h, sock1, sock2, 0, fac, &buf);

	GPU_node_buf_free(buf1);
	GPU_node_buf_free(buf2);

	node = GPU_node_begin("gammacorrect", w, h);
	GPU_node_input(node, GPU_VEC4, "col", NULL, buf);
	GPU_node_input(node, GPU_TEX1D, "gamma", &length, pixels);
	//GPU_node_code(gamma_shader);
	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);
	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);

	GPU_node_buf_free(buf);
}

static int is_pow2(int n)
{
	return ((n)&(n-1))==0;
}

static int smaller_pow2(int n)
{
	while (!is_pow2(n))
		n= n&(n-1);

	return n;
}

static int larger_pow2(int n)
{
	if (is_pow2(n))
		return n;
	
	return smaller_pow2(n)*2;
}

void GPU_comp_wipe(int w, int h, GPUCompSocket col1, GPUCompSocket col2, float angle, int type, int forward, float fac, GPUNodeBuf **outbuf)
{
	float x1, y1, x2, y2, d1, d2, scale[2];
	float c = cos(angle);
	float s = sin(angle);
	float width = 0.1;
	GPUNode *node;

	/* compute distance to a line segment with length 1. no idea why the sequencer
	   effect does this so complicated */
	if(forward) {
		x1 = fac;
		y1 = fac;
	} else {
		x1 = 1.0 - fac;
		y1 = 1.0 - fac;
	}

	x2 = x1 + c;
	y2 = y1 + s;

	d1 = (x2 - x1);
	d2 = (y2 - y1);

	/* ugly hack for non rectangular textures */
	if (GLEW_ARB_texture_non_power_of_two) {
		scale[0] = 1.0;
		scale[1] = 1.0;
	}
	else {
		scale[0] = larger_pow2(w)/(float)w;
		scale[1] = larger_pow2(h)/(float)h;
	}

#if 0
	float t1,t2,alpha;

	if(width == 0)
		return side;
	
	if(width < dist)
		return side;
	
	t1 = dist / width;  //percentange of width that is
	t2 = 1 / width;  //amount of alpha per % point
	
	if(side == 1)
		alpha = (t1*t2*100) + (1-perc); // add point's alpha contrib to current position in wipe
	else
		alpha = (1-perc) - (t1*t2*100);
	
	if(dir == 0)
		alpha = 1-alpha;
	return alpha;
}

	printf("%f %f  %f %f\n", x1, y1, d1, d2);
#endif

	node = GPU_node_begin("wipe", w, h);

	GPU_node_input(node, GPU_VEC4, "col1", col1.vec, col1.buf);
	GPU_node_input(node, GPU_VEC4, "col2", col2.vec, col2.buf);
	GPU_node_input(node, GPU_FLOAT, "d1", &d1, NULL);
	GPU_node_input(node, GPU_FLOAT, "d2", &d2, NULL);
	GPU_node_input(node, GPU_FLOAT, "x1", &x1, NULL);
	GPU_node_input(node, GPU_FLOAT, "y1", &y1, NULL);
	GPU_node_input(node, GPU_VEC2, "scale", scale, NULL);
	GPU_node_input(node, GPU_FLOAT, "width", &width, NULL);
	GPU_node_input(node, GPU_FLOAT, "fac", &fac, NULL);

	/*GPU_node_code(
	"	vec2 st = gl_TexCoord[0].st*scale;\n"
	"	float dist = d1*(y1 - st.t) - d2*(x1 - st.s);\n"
	"	if (abs(dist) > width) {\n"
	"		if (dist > 0.0)\n"
	"			outcol = col1;\n"
	"		else\n"
	"			outcol = col2;\n"
	"	}\n"
	"	else {\n"
	"		float t1 = dist/width;\n"
	"		float t2 = dist/width;\n"
	"		float alpha;\n"
	"	\n"
	"		if (dist > 0.0)\n"
	"			alpha = t1*t2*100.0 + (1.0-fac);\n"
	"		else\n"
	"			alpha = (1.0-fac) - t1*t2*100.0;\n"

	"		outcol = mix(col1, col2, alpha);\n"
	"	}\n");*/

	GPU_node_output(node, GPU_VEC4, "outcol", outbuf);

	GPU_node_end(node);
	BLI_addtail(&_composite.nodes, node);
}
#endif

