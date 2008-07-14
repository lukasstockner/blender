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

#include "DNA_listBase.h"
#include "DNA_image_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_image.h"
#include "BKE_global.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_blenlib.h"

#include "GPU_extensions.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Extensions support */

/* extensions used:
	- texture border clamp: 1.3 core
	- fragement shader: 2.0 core
	- framebuffer object: ext specification
	- multitexture 1.3 core
	- half float: arb extension
	- arb non power of two: 2.0 core
	- pixel buffer objects? 2.1 core
	- arb draw buffers? 2.0 core
*/

struct GPUGlobal {
	GLint maxtextures;
	GLuint currentfb;
	GLenum halfformat;
	struct GPUGlobalLimits {
		GLint alu_instructions; // 48+ (64)
		GLint tex_instructions; // 24+ (32)
		GLint instructions; // 72+ (96)
		GLint tex_indirections; // 4+ (4)
		GLint temp_variables; // 16+ (32)
		GLint max_attribs; // 10+ (varying) (10)
		GLint parameters; // 24+ (uniforms + constants) (32)
	} limits;

	int minimumsupport;
} GG = {1, 0, 0, {0, 0, 0, 0, 0, 0, 0}, 0};

void GPU_extensions_init()
{
	glewInit();

	/* glewIsSupported("GL_VERSION_2_0") */

	if (GLEW_ARB_multitexture)
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &GG.maxtextures);
	if (GLEW_ATI_texture_float || GLEW_ARB_texture_float) {
		GG.halfformat = (GLEW_ATI_texture_float)? GL_RGBA_FLOAT16_ATI: GL_RGBA16F_ARB;
	}
	if (GLEW_ARB_fragment_shader) {
		glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
			GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB, &GG.limits.alu_instructions);
		glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
			GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB, &GG.limits.tex_instructions);
		glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
			GL_MAX_PROGRAM_INSTRUCTIONS_ARB, &GG.limits.instructions);
		glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
			GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB, &GG.limits.tex_indirections);
		glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
			GL_MAX_PROGRAM_TEMPORARIES_ARB, &GG.limits.temp_variables);
		glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
			GL_MAX_PROGRAM_ATTRIBS_ARB, &GG.limits.max_attribs);
		glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
			GL_MAX_PROGRAM_PARAMETERS_ARB, &GG.limits.parameters);
	}

	GG.minimumsupport = 1;
	if (!GLEW_ARB_multitexture) GG.minimumsupport = 0;
	if (!GLEW_ARB_vertex_shader) GG.minimumsupport = 0;
	if (!GLEW_ARB_fragment_shader) GG.minimumsupport = 0;
}

int GPU_extensions_minimum_support()
{
	return GG.minimumsupport;
}

int GPU_print_error(char *str)
{
	GLenum errCode;

	if (G.f & G_DEBUG) {
		if ((errCode = glGetError()) != GL_NO_ERROR) {
    	    fprintf(stderr, "%s opengl error: %s\n", str, gluErrorString(errCode));
			return 1;
		}
	}

	return 0;
}

static void GPU_print_framebuffer_error(GLenum status)
{
	fprintf(stderr, "GPUFrameBuffer: framebuffer incomplete error %d\n",
		(int)status);

	switch(status) {
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			fprintf(stderr, "Incomplete attachment.\n");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			fprintf(stderr, "Unsupported framebuffer format.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			fprintf(stderr, "Missing attachment.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			fprintf(stderr, "Attached images must have same dimensions.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			 fprintf(stderr, "Attached images must have same format.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			fprintf(stderr, "Missing draw buffer.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			fprintf(stderr, "Missing read buffer.\n");
			break;
		default:
			fprintf(stderr, "Unknown.\n");
			break;
	}
}

/* GPUTexture */

struct GPUTexture {
	int w, h;				/* width/height */
	int realw, realh;		/* the width/height as intended */
	int number;				/* number for multitexture binding */
	int refcount;			/* reference count */
	GLenum target;			/* GL_TEXTURE_* */
	GLuint bindcode;		/* opengl identifier for texture */
	GLuint framebuffer;		/* opengl identifier for framebuffer object */
	GLenum wrapmode;		/* clamp/repeat/.. */
	GLenum internalformat;	/* 8bit, half float, .. */
	int fromblender;		/* we got the texture from Blender */

	GPUFrameBuffer *fb;		/* GPUFramebuffer this texture is attached to */
	int depth;				/* is a depth texture? */
};

#define FTOCHAR(val) val<=0.0f?0: (val>=1.0f?255: (char)(255.0f*val))
static unsigned char *GPU_texture_convert_pixels(int length, float *fpixels)
{
	unsigned char *pixels, *p;
	float *fp;
	int a, len;

	len = 4*length;
	fp = fpixels;
	p = pixels = MEM_callocN(sizeof(unsigned char)*len, "GPUTexturePixels");

	for (a=0; a<len; a++, p++, fp++)
		*p = FTOCHAR((*fp));

	return pixels;
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

static void GPU_glTexSubImageEmpty(GLenum target, GLenum type, int x, int y, int w, int h)
{
	void *pixels = MEM_callocN(sizeof(char)*4*w*h, "GPUTextureEmptyPixels");

	if (target == GL_TEXTURE_1D)
		glTexSubImage1D(target, 0, x, w, GL_RGBA, type, pixels);
	else
		glTexSubImage2D(target, 0, x, y, w, h, GL_RGBA, type, pixels);
	
	MEM_freeN(pixels);
}

static GPUTexture *GPU_texture_create_nD(int w, int h, int n, float *fpixels, int halffloat, int depth)
{
	GPUTexture *tex;
	GLenum type, format;
	void *pixels = NULL;

	if(depth && !GLEW_ARB_depth_texture)
		return NULL;

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = tex->realw = w;
	tex->h = tex->realh = h;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = (n == 1)? GL_TEXTURE_1D: GL_TEXTURE_2D;
	tex->depth = depth;

	glGenTextures(1, &tex->bindcode);

	if (!tex->bindcode) {
		fprintf(stderr, "GPUTexture: texture create failed: %d\n",
			(int)glGetError());
		GPU_texture_free(tex);
		return NULL;
	}

	if (!GLEW_ARB_texture_non_power_of_two) {
		tex->w = larger_pow2(tex->realw);
		tex->h = larger_pow2(tex->realh);
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	if(depth) {
		type = GL_UNSIGNED_BYTE;
		format = GL_DEPTH_COMPONENT;
		tex->internalformat = GL_DEPTH_COMPONENT;
	}
	else if (halffloat && (GLEW_ARB_texture_float || GLEW_ATI_texture_float)) {
		type = GL_FLOAT;
		format = GL_RGBA;
		tex->internalformat = GG.halfformat;
		/* GL_EXT_float_buffer: GL_FLOAT_RGBA16_NV */
	}
	else {
		type = GL_UNSIGNED_BYTE;
		format = GL_RGBA;
		tex->internalformat = GL_RGBA8;

		if (fpixels)
			pixels = GPU_texture_convert_pixels(w*h, fpixels);
	}

	if (tex->target == GL_TEXTURE_1D) {
		glTexImage1D(tex->target, 0, tex->internalformat, tex->w, 0, format, type, 0);
		if (fpixels) {
			glTexSubImage1D(tex->target, 0, 0, tex->realw, format, type,
				pixels? pixels: fpixels);

			if (tex->w > tex->realw)
				GPU_glTexSubImageEmpty(tex->target, type, tex->realw, 0,
					tex->w-tex->realw, 1);
		}
	}
	else {
		glTexImage2D(tex->target, 0, tex->internalformat, tex->w, tex->h, 0,
			format, type, 0);
		if (fpixels) {
			glTexSubImage2D(tex->target, 0, 0, 0, tex->realw, tex->realh,
				format, type, pixels? pixels: fpixels);

			if (tex->w > tex->realw)
				GPU_glTexSubImageEmpty(tex->target, type, tex->realw, 0,
					tex->w-tex->realw, tex->h);
			if (tex->h > tex->realh)
				GPU_glTexSubImageEmpty(tex->target, type, 0, tex->realh,
					tex->realw, tex->h-tex->realh);
		}
	}

	if (pixels)
		MEM_freeN(pixels);

	if(depth) {
		glTexParameteri(tex->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(tex->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else {
		glTexParameteri(tex->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(tex->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (tex->target != GL_TEXTURE_1D) {
		/* CLAMP_TO_BORDER is an OpenGL 1.3 core feature */
		if(depth)
			tex->wrapmode = GL_CLAMP_TO_EDGE;
		else
			tex->wrapmode = GL_CLAMP_TO_BORDER;
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_S, tex->wrapmode);
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_T, tex->wrapmode);

#if 0
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); 
#endif
	}
	else {
		tex->wrapmode = GL_CLAMP_TO_EDGE;
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_S, tex->wrapmode);
	}

	return tex;
}

static int is_pow2_limit(int num)
{
	if (U.glreslimit != 0 && num > U.glreslimit) return 0;
	return ((num)&(num-1))==0;
}

static int smaller_pow2_limit(int num)
{
	if (U.glreslimit != 0 && num > U.glreslimit)
		return U.glreslimit;
	return smaller_pow2(num);
}

static void gpu_blender_texture_create(Image *ima, ImageUser *iuser)
{
	ImBuf *ibuf;
	char *rect, *scalerect;
	int x, y;

	/* TODO: better integrate with textured mode */
	ibuf = BKE_image_get_ibuf(ima, iuser);

	if(!ibuf)
		return;
	
	if(!ibuf->rect && ibuf->rect_float)
		IMB_rect_from_float(ibuf);

	if(!ibuf->rect)
		return;
	
	GPU_print_error("Pre Blender Texture Create");

	x = ibuf->x;
	y = ibuf->y;
	rect = (char*)ibuf->rect;
	
	if (!is_pow2_limit(x) || !is_pow2_limit(y)) {
		x= smaller_pow2_limit(x);
		y= smaller_pow2_limit(y);
		
		scalerect= MEM_mallocN(x*y*sizeof(*scalerect)*4, "scalerect");
		gluScaleImage(GL_RGBA, ibuf->x, ibuf->y, GL_UNSIGNED_BYTE, rect, x, y, GL_UNSIGNED_BYTE, scalerect);
		rect= scalerect;
	}

	glGenTextures(1, (GLuint *)&ima->bindcode);
	glBindTexture(GL_TEXTURE_2D, ima->bindcode);

	if(!(G.f & G_TEXTUREPAINT)) {
		gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, x, y, GL_RGBA, GL_UNSIGNED_BYTE, rect);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else {
		glTexImage2D(GL_TEXTURE_2D, 0,  GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, rect);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if(rect != (char*)ibuf->rect)
		MEM_freeN(rect);

	GPU_print_error("Post Blender Texture Create");
}

GPUTexture *GPU_texture_from_blender(Image *ima, ImageUser *iuser)
{
	GPUTexture *tex;
	GLint w, h, border, lastbindcode;
		
	if(ima->gputexture)
		return ima->gputexture;

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastbindcode);
	
	if(!ima->bindcode)
		gpu_blender_texture_create(ima, iuser);

	if(!ima->bindcode) {
		glBindTexture(GL_TEXTURE_2D, lastbindcode);
		return NULL;
	}

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->bindcode = ima->bindcode;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_2D;
	tex->wrapmode = GL_REPEAT;
	tex->fromblender = 1;
	tex->internalformat = GL_RGBA;

	ima->gputexture= tex;

	if (!glIsTexture(tex->bindcode)) {
		GPU_print_error("Blender Texture");

		tex->w = tex->realw = 64;
		tex->h = tex->realh = 64;
	}
	else {
		glBindTexture(GL_TEXTURE_2D, tex->bindcode);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_BORDER, &border);

		tex->w = tex->realw = w - border;
		tex->h = tex->realh = h - border;
	}

	glBindTexture(GL_TEXTURE_2D, lastbindcode);

	return tex;
}

GPUTexture *GPU_texture_create_1D(int w, float *fpixels, int halffloat)
{
	GPUTexture *tex = GPU_texture_create_nD(w, 1, 1, fpixels, halffloat, 0);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

GPUTexture *GPU_texture_create_2D(int w, int h, float *fpixels, int halffloat)
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, fpixels, halffloat, 0);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

GPUTexture *GPU_texture_create_depth(int w, int h)
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, NULL, 0, 1);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

void GPU_texture_bind(GPUTexture *tex, int number)
{
	GLenum arbnumber;

	if (number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}

	if(number == -1)
		return;

	GPU_print_error("Pre Texture Bind");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + number);
	if (number != 0) glActiveTextureARB(arbnumber);
	glBindTexture(tex->target, tex->bindcode);
	glEnable(tex->target);
	if (number != 0) glActiveTextureARB(GL_TEXTURE0_ARB);

	tex->number = number;

	GPU_print_error("Post Texture Bind");
}

void GPU_texture_unbind(GPUTexture *tex)
{
	GLenum arbnumber;

	if (tex->number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}

	if(tex->number == -1)
		return;
	
	GPU_print_error("Pre Texture Unbind");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + tex->number);
	if (tex->number != 0) glActiveTextureARB(arbnumber);
	glBindTexture(tex->target, 0);
	glDisable(tex->target);
	if (tex->number != 0) glActiveTextureARB(GL_TEXTURE0_ARB);

	tex->number = -1;

	GPU_print_error("Post Texture Unbind");
}

void GPU_texture_free(GPUTexture *tex)
{
	tex->refcount--;

	if (tex->refcount < 0)
		fprintf(stderr, "GPUTexture: negative refcount\n");
	
	if (tex->refcount == 0) {
		if (tex->fb)
			GPU_framebuffer_texture_detach(tex->fb, tex);
		if (tex->bindcode && !tex->fromblender)
			glDeleteTextures(1, &tex->bindcode);

		MEM_freeN(tex);
	}
}

void GPU_texture_ref(GPUTexture *tex)
{
	tex->refcount++;
}

int GPU_texture_width(GPUTexture *tex)
{
	return tex->realw;
}

int GPU_texture_height(GPUTexture *tex)
{
	return tex->realh;
}

int GPU_texture_target(GPUTexture *tex)
{
	return tex->target;
}

int GPU_texture_opengl_width(GPUTexture *tex)
{
	return tex->w;
}

int GPU_texture_opengl_height(GPUTexture *tex)
{
	return tex->h;
}

void GPU_texture_coord_2f(GPUTexture *tex, float s, float t)
{
	if (GLEW_ARB_multitexture) {
		GLenum arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + tex->number);
		float scalex, scaley;

		scalex = (float)tex->realw/(float)tex->w;
		scaley = (float)tex->realh/(float)tex->h;

		glMultiTexCoord2fARB(arbnumber, scalex*s, scaley*t);
	}
	else
		glTexCoord2f(s, t);
}

#if 0
void GPU_texture_blender_wrap_swap(GPUTexture *tex)
{
	if (tex->fromblender) {
		GLenum arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + tex->number);

		if (tex->number != 0) glActiveTextureARB(arbnumber);

		if (tex->wrapmode == GL_REPEAT)
			tex->wrapmode = GL_CLAMP_TO_BORDER;
		else
			tex->wrapmode = GL_REPEAT;
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_S, tex->wrapmode);
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_T, tex->wrapmode);

		if (tex->number != 0) glActiveTextureARB(GL_TEXTURE0_ARB);
	}
}
#endif

int GPU_texture_is_half_float(GPUTexture *tex)
{
	return (tex->internalformat == GG.halfformat);
}

GPUFrameBuffer *GPU_texture_framebuffer(GPUTexture *tex)
{
	return tex->fb;
}

/* GPUFrameBuffer */

struct GPUFrameBuffer {
	GLuint object;
	GPUTexture *colortex;
	GPUTexture *depthtex;
};

GPUFrameBuffer *GPU_framebuffer_create()
{
	GPUFrameBuffer *fb;

	if (!GLEW_EXT_framebuffer_object)
		return NULL;
	
	fb= MEM_callocN(sizeof(GPUFrameBuffer), "GPUFrameBuffer");
	glGenFramebuffersEXT(1, &fb->object);

	if (!fb->object) {
		fprintf(stderr, "GPUFFrameBuffer: framebuffer gen failed. %d\n",
			(int)glGetError());
		GPU_framebuffer_free(fb);
		return NULL;
	}

	return fb;
}

int GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex)
{
	GLenum status;
	GLenum attachment;

	if(tex->depth)
		attachment = GL_DEPTH_ATTACHMENT_EXT;
	else
		attachment = GL_COLOR_ATTACHMENT0_EXT;

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb->object);
	GG.currentfb = fb->object;

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment, 
		tex->target, tex->bindcode, 0);

	if(tex->depth) {
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}
	else {
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glReadBuffer(GL_NONE);
	}

	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

	if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
		GPU_framebuffer_restore();
		GPU_print_framebuffer_error(status);
		return 0;
	}

	if(tex->depth)
		fb->depthtex = tex;
	else
		fb->colortex = tex;

	tex->fb= fb;

	return 1;
}

void GPU_framebuffer_texture_detach(GPUFrameBuffer *fb, GPUTexture *tex)
{
	GLenum attachment;

	if(!tex->fb)
		return;

	if(GG.currentfb != tex->fb->object) {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tex->fb->object);
		GG.currentfb = tex->fb->object;
	}

	if(tex->depth) {
		fb->depthtex = NULL;
		attachment = GL_DEPTH_ATTACHMENT_EXT;
	}
	else {
		fb->colortex = NULL;
		attachment = GL_COLOR_ATTACHMENT0_EXT;
	}

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment,
		tex->target, 0, 0);

	tex->fb = NULL;
}

void GPU_framebuffer_texture_bind(GPUFrameBuffer *fb, GPUTexture *tex)
{
	/* push attributes */
	glPushAttrib(GL_ENABLE_BIT);
	glPushAttrib(GL_VIEWPORT_BIT);
	glDisable(GL_SCISSOR_TEST);

	/* bind framebuffer */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tex->fb->object);

	/* push matrices and set default viewport and matrix */
	glViewport(0, 0, tex->w, tex->h);
	GG.currentfb = tex->fb->object;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
}

void GPU_framebuffer_texture_unbind(GPUFrameBuffer *fb, GPUTexture *tex)
{
	/* restore matrix */
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	/* restore attributes */
	glPopAttrib();
	glPopAttrib();
	glEnable(GL_SCISSOR_TEST);
}

void GPU_framebuffer_free(GPUFrameBuffer *fb)
{
	if(fb->depthtex)
		GPU_framebuffer_texture_detach(fb, fb->depthtex);
	if(fb->colortex)
		GPU_framebuffer_texture_detach(fb, fb->colortex);

	if(fb->object) {
		glDeleteFramebuffersEXT(1, &fb->object);

		if (GG.currentfb == fb->object) {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			GG.currentfb = 0;
		}
	}

	MEM_freeN(fb);
}

void GPU_framebuffer_restore()
{
	if (GG.currentfb != 0) {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		GG.currentfb = 0;
	}
}

/* GPUShader */

struct GPUShader {
	GLhandleARB object;		/* handle for full shader */
	GLhandleARB vertex;		/* handle for vertex shader */
	GLhandleARB fragment;	/* handle for fragment shader */
	GLhandleARB lib;		/* handle for libment shader */
	int totattrib;			/* total number of attributes */
};

static void shader_print_errors(char *task, char *log, const char *code)
{
	const char *c, *pos, *end = code + strlen(code);
	int line = 1;

	fprintf(stderr, "GPUShader: %s error:\n", task);

	if(G.f & G_DEBUG) {
		c = code;
		while ((c < end) && (pos = strchr(c, '\n'))) {
			fprintf(stderr, "%2d  ", line);
			fwrite(c, (pos+1)-c, 1, stderr);
			c = pos+1;
			line++;
		}

		fprintf(stderr, "%s", c);
	}

	fprintf(stderr, "%s\n", log);
}

GPUShader *GPU_shader_create(const char *vertexcode, const char *fragcode, GPUShader *lib)
{
	GLint status;
	GLcharARB log[5000];
	GLsizei length = 0;
	GPUShader *shader;

	if (!GLEW_ARB_vertex_shader || !GLEW_ARB_fragment_shader)
		return NULL;

	shader = MEM_callocN(sizeof(GPUShader), "GPUShader");

	if(vertexcode)
		shader->vertex = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	if(fragcode)
		shader->fragment = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	shader->object = glCreateProgramObjectARB();

	if (!shader->object ||
		(vertexcode && !shader->vertex) ||
		(fragcode && !shader->fragment)) {
		fprintf(stderr, "GPUShader, object creation failed.\n");
		GPU_shader_free(shader);
		return NULL;
	}

	if(lib && lib->lib)
		glAttachObjectARB(shader->object, lib->lib);

	if(vertexcode) {
		glAttachObjectARB(shader->object, shader->vertex);
		glShaderSourceARB(shader->vertex, 1, (const char**)&vertexcode, NULL);

		glCompileShaderARB(shader->vertex);
		glGetObjectParameterivARB(shader->vertex, GL_OBJECT_COMPILE_STATUS_ARB, &status);

		if (!status) {
			glValidateProgramARB(shader->vertex);
			glGetInfoLogARB(shader->vertex, sizeof(log), &length, log);
			shader_print_errors("compile", log, vertexcode);

			GPU_shader_free(shader);
			return NULL;
		}
	}

	if(fragcode) {
		glAttachObjectARB(shader->object, shader->fragment);
		glShaderSourceARB(shader->fragment, 1, (const char**)&fragcode, NULL);

		glCompileShaderARB(shader->fragment);
		glGetObjectParameterivARB(shader->fragment, GL_OBJECT_COMPILE_STATUS_ARB, &status);

		if (!status) {
			glValidateProgramARB(shader->fragment);
			glGetInfoLogARB(shader->fragment, sizeof(log), &length, log);
			shader_print_errors("compile", log, fragcode);

			GPU_shader_free(shader);
			return NULL;
		}
	}

	glLinkProgramARB(shader->object);
	glGetObjectParameterivARB(shader->object, GL_OBJECT_LINK_STATUS_ARB, &status);
	if (!status) {
		glGetInfoLogARB(shader->object, sizeof(log), &length, log);
		shader_print_errors("linking", log, fragcode);

		GPU_shader_free(shader);
		return NULL;
	}

	return shader;
}

GPUShader *GPU_shader_create_lib(const char *code)
{
	GLint status;
	GLcharARB log[5000];
	GLsizei length = 0;
	GPUShader *shader;

	if (!GLEW_ARB_vertex_shader || !GLEW_ARB_fragment_shader)
		return NULL;

	shader = MEM_callocN(sizeof(GPUShader), "GPUShader");

	shader->lib = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

	if (!shader->lib) {
		fprintf(stderr, "GPUShader, object creation failed.\n");
		GPU_shader_free(shader);
		return NULL;
	}

	glShaderSourceARB(shader->lib, 1, (const char**)&code, NULL);

	glCompileShaderARB(shader->lib);
	glGetObjectParameterivARB(shader->lib, GL_OBJECT_COMPILE_STATUS_ARB, &status);

	if (!status) {
		glValidateProgramARB(shader->lib);
		glGetInfoLogARB(shader->lib, sizeof(log), &length, log);
		shader_print_errors("compile", log, code);

		GPU_shader_free(shader);
		return NULL;
	}

	return shader;
}


void GPU_shader_bind(GPUShader *shader)
{
	GPU_print_error("Pre Shader Bind");
	glUseProgramObjectARB(shader->object);
	GPU_print_error("Post Shader Bind");
}

void GPU_shader_unbind()
{
	GPU_print_error("Pre Shader Unbind");
	glUseProgramObjectARB(0);
	GPU_print_error("Post Shader Unbind");
}

void GPU_shader_free(GPUShader *shader)
{
	if (shader->lib)
		glDeleteObjectARB(shader->lib);
	if (shader->vertex)
		glDeleteObjectARB(shader->vertex);
	if (shader->fragment)
		glDeleteObjectARB(shader->fragment);
	if (shader->object)
		glDeleteObjectARB(shader->object);
	MEM_freeN(shader);
}


void GPU_shader_uniform_vector(GPUShader *shader, char *name, int length, int arraysize, float *value)
{
	GLint location = glGetUniformLocationARB(shader->object, name);

	if(location == -1)
		return;

	GPU_print_error("Pre Uniform Vector");

	/*if (length == 1) printf("%s %f\n", name, value[0]);
	else if (length == 2) printf("%s %f %f\n", name, value[0], value[1]);
	else if (length == 3) { printf("%s ", name); printvecf("", value); }
	else if (length == 4) { printf("%s ", name); printquat("", value); }
	else if (length == 9) { printf("%s ", name); printmatrix3("", (float(*)[3])value); }
	else if (length == 16) { printf("%s ", name); printmatrix4("", (float(*)[4])value); }*/

	if (length == 1) glUniform1fvARB(location, arraysize, value);
	else if (length == 2) glUniform2fvARB(location, arraysize, value);
	else if (length == 3) glUniform3fvARB(location, arraysize, value);
	else if (length == 4) glUniform4fvARB(location, arraysize, value);
	else if (length == 9) glUniformMatrix3fvARB(location, arraysize, 0, value);
	else if (length == 16) glUniformMatrix4fvARB(location, arraysize, 0, value);

	GPU_print_error("Post Uniform Vector");
}

void GPU_shader_uniform_texture(GPUShader *shader, char *name, GPUTexture *tex)
{
	GLint location;
	GLenum arbnumber;

	if (tex->number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}
		
	if(tex->number == -1)
		return;

	GPU_print_error("Pre Uniform Texture");

	location = glGetUniformLocationARB(shader->object, name);
	arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + tex->number);

	if (tex->number != 0) glActiveTextureARB(arbnumber);
	glBindTexture(tex->target, tex->bindcode);
	glUniform1iARB(location, tex->number);
	glEnable(tex->target);
	if (tex->number != 0) glActiveTextureARB(GL_TEXTURE0_ARB);

	GPU_print_error("Post Uniform Texture");
}

int GPU_shader_get_attribute(GPUShader *shader, char *name)
{
	int index;
	
	GPU_print_error("Pre Get Attribute");

	index = glGetAttribLocationARB(shader->object, name);

	GPU_print_error("Post Get Attribute");

	return index;
}

/* GPUPixelBuffer */

typedef struct GPUPixelBuffer {
	GLuint bindcode[2];
	GLuint current;
	int datasize;
	int numbuffers;
	int halffloat;
} GPUPixelBuffer;

void GPU_pixelbuffer_free(GPUPixelBuffer *pb)
{
	if (pb->bindcode[0])
		glDeleteBuffersARB(pb->numbuffers, pb->bindcode);
	MEM_freeN(pb);
}

GPUPixelBuffer *gpu_pixelbuffer_create(int x, int y, int halffloat, int numbuffers)
{
	GPUPixelBuffer *pb;

	if (!GLEW_ARB_multitexture || !GLEW_EXT_pixel_buffer_object)
		return NULL;
	
	pb = MEM_callocN(sizeof(GPUPixelBuffer), "GPUPBO");
	pb->datasize = x*y*4*((halffloat)? 16: 8);
	pb->numbuffers = numbuffers;
	pb->halffloat = halffloat;

   	glGenBuffersARB(pb->numbuffers, pb->bindcode);

	if (!pb->bindcode[0]) {
		fprintf(stderr, "GPUPixelBuffer allocation failed\n");
		GPU_pixelbuffer_free(pb);
		return NULL;
	}

	return pb;
}

void GPU_pixelbuffer_texture(GPUTexture *tex, GPUPixelBuffer *pb)
{
	void *pixels;
	int i;

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, tex->bindcode);
 
 	for (i = 0; i < pb->numbuffers; i++) {
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, pb->bindcode[pb->current]);
		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, pb->datasize, NULL,
			GL_STREAM_DRAW_ARB);
    
		pixels = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);
		/*memcpy(pixels, _oImage.data(), pb->datasize);*/
    
		if (!glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT)) {
			fprintf(stderr, "Could not unmap opengl PBO\n");
			break;
		}
	}

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
}

static int pixelbuffer_map_into_gpu(GLuint bindcode)
{
	void *pixels;

    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, bindcode);
	pixels = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);

	/* do stuff in pixels */

    if (!glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT)) {
		fprintf(stderr, "Could not unmap opengl PBO\n");
		return 0;
    }
	
	return 1;
}

static void pixelbuffer_copy_to_texture(GPUTexture *tex, GPUPixelBuffer *pb, GLuint bindcode)
{
	GLenum type = (pb->halffloat)? GL_HALF_FLOAT_NV: GL_UNSIGNED_BYTE;
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, tex->bindcode);
    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, bindcode);

    glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, tex->w, tex->h,
                    GL_RGBA, type, NULL);

	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
}

void GPU_pixelbuffer_async_to_gpu(GPUTexture *tex, GPUPixelBuffer *pb)
{
	int newbuffer;

	if (pb->numbuffers == 1) {
		pixelbuffer_copy_to_texture(tex, pb, pb->bindcode[0]);
		pixelbuffer_map_into_gpu(pb->bindcode[0]);
	}
	else {
		pb->current = (pb->current+1)%pb->numbuffers;
		newbuffer = (pb->current+1)%pb->numbuffers;

		pixelbuffer_map_into_gpu(pb->bindcode[newbuffer]);
		pixelbuffer_copy_to_texture(tex, pb, pb->bindcode[pb->current]);
    }
}

