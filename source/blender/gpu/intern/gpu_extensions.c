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
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_extensions.c
 *  \ingroup gpu
 */

/* my interface */
#include "GPU_extensions.h"

/* my library */
#include "GPU_draw.h"
#include "GPU_basic_shader.h"
#include "GPU_compatibility.h"

/* internal */
#include "intern/gpu_codegen.h"
#include "intern/gpu_extension_wrapper.h"

/* external */
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BKE_global.h"

#include "DNA_image_types.h"

#include "MEM_guardedalloc.h"

/* standard*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* Extensions support */

/* extensions used:
 * - texture border clamp: 1.3 core
 * - fragment shader: 2.0 core
 * - framebuffer object: ext specification
 * - multitexture 1.3 core
 * - arb non power of two: 2.0 core
 * - pixel buffer objects? 2.1 core
 * - arb draw buffers? 2.0 core
 */

/* Non-generated shaders */
extern char datatoc_gpu_shader_vsm_store_vert_glsl[];
extern char datatoc_gpu_shader_vsm_store_frag_glsl[];
extern char datatoc_gpu_shader_sep_gaussian_blur_vert_glsl[];
extern char datatoc_gpu_shader_sep_gaussian_blur_frag_glsl[];
unsigned int GPU_ext_config = 0;
unsigned int GPU_gl_type = 0;

typedef struct GPUShaders {
	GPUShader *vsm_store;
	GPUShader *sep_gaussian_blur;
} GPUShaders;

// XXX jwilkins: fix the "booleans" in this struct to actually be bools 
static struct GPUGlobal {
	GLint maxtexsize;
	GLint maxtextures;
	GLuint currentfb;
	GLboolean glslsupport;
	GLboolean framebuffersupport;
	int extdisabled;
	int colordepth;
	int npotdisabled; /* ATI 3xx-5xx (and more) chipsets support NPoT partially (== not enough) */
	GPUDeviceType device;
	GPUOSType os;
	GPUDriverType driver;
	GPUShaders shaders;
} GG = {1, 0};

/* GPU Types */

int GPU_type_matches(GPUDeviceType device, GPUOSType os, GPUDriverType driver)
{
	return (GG.device & device) && (GG.os & os) && (GG.driver & driver);
}

/* GPU Extensions */

static int gpu_extensions_init = 0;

void GPU_extensions_disable(void)
{
	GG.extdisabled = 1;
}

int GPU_max_texture_size(void)
{
	return GG.maxtexsize;
}

/*
Computes the maximum number of textures 'n' that
can be referenced by ActiveTexture(TEXTURE0+n-1)

This is for any use of ActiveTexture.

Individual limits, such as for the multitexture extension, gl_TexCoord,
vertex shaders, fragment shader, etc. will each have different limits.
*/
static GLint get_max_textures(void)
{
	GLint maxTextureUnits;
	GLint maxTextureCoords;
	GLint maxCombinedTextureImageUnits;

	/* There has to be at least one texture so count that here */
	maxTextureUnits = 1;

#if !defined(GLEW_ES_ONLY)
	if (GPU_PROFILE_COMPAT && (GLEW_VERSION_1_3 || GLEW_ARB_multitexture)) {
		/* Multitexture typically supports only 2 or 4 texture stages even on modern hardware. */
		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTextureUnits);
	}
#endif

	/* Set to zero here in case they do not get set later */
	maxTextureCoords             = 0;
	maxCombinedTextureImageUnits = 0;

	if (GLEW_VERSION_2_0 || GLEW_ES_VERSION_2_0 || GLEW_ARB_fragment_program) {
#if !defined(GLEW_ES_ONLY)
		if (GPU_PROFILE_COMPAT) {
			/* size of gl_TexCoord array in GLSL */
			glGetIntegerv(GL_MAX_TEXTURE_COORDS, &maxTextureCoords);
		}
#endif

		/* Number of textures accessible by vertex, fragment, and geometry shaders combined. */
		/* Individually the limits for each of those programmable units may be smaller. */
		glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxCombinedTextureImageUnits);
	}

	return MAX3(maxTextureUnits, maxTextureCoords, maxCombinedTextureImageUnits);
}

int GPU_max_textures(void)
{
	return GG.maxtextures;
}

void GPU_extensions_init(void)
{
	GLint r, g, b;
	const char *vendor, *renderer;
	int bdepth = -1;

	/* can't avoid calling this multiple times, see wm_window_add_ghostwindow */
	if (gpu_extensions_init) return;
	gpu_extensions_init= 1;

	GPU_wrap_extensions(&(GG.glslsupport), &(GG.framebuffersupport));
	GPU_codegen_init();

	GG.maxtextures = get_max_textures();

	GPU_CHECK_NO_ERROR();

//#if defined(WITH_GL_PROFILE_ES20) || defined(WITH_GL_PROFILE_CORE)
//	gpu_object_init_gles();
//#endif

	GPU_CHECK_NO_ERROR();

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &GG.maxtexsize);

	GPU_CHECK_NO_ERROR();

	if (GPU_PROFILE_CORE) {
#if defined(WITH_GL_PROFILE_CORE)
		//glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,   &r);
		//glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &g);
		//glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,  &b);
		r=g=b=8;// XXX jwilkins: workaround driver bug?  will probably need to forward this from ghost
#endif
	}
	else
	{
#if defined(WITH_GL_PROFILE_COMPAT) || defined(WITH_GL_PROFILE_ES20)
		glGetIntegerv(GL_RED_BITS,   &r);
		glGetIntegerv(GL_GREEN_BITS, &g);
		glGetIntegerv(GL_BLUE_BITS,  &b);
#endif
	}

	GPU_CHECK_NO_ERROR();

	GG.colordepth = r+g+b; /* assumes same depth for RGB */

	vendor = (const char *)glGetString(GL_VENDOR);
	renderer = (const char *)glGetString(GL_RENDERER);

	GPU_CHECK_NO_ERROR();

	if (strstr(vendor, "ATI")) {
		GG.device = GPU_DEVICE_ATI;
		GG.driver = GPU_DRIVER_OFFICIAL;
	}
	else if (strstr(vendor, "NVIDIA")) {
		GG.device = GPU_DEVICE_NVIDIA;
		GG.driver = GPU_DRIVER_OFFICIAL;
	}
	else if (strstr(vendor, "Intel") ||
	        /* src/mesa/drivers/dri/intel/intel_context.c */
	        strstr(renderer, "Mesa DRI Intel") ||
		strstr(renderer, "Mesa DRI Mobile Intel")) {
		GG.device = GPU_DEVICE_INTEL;
		GG.driver = GPU_DRIVER_OFFICIAL;
	}
	else if (strstr(renderer, "Mesa DRI R") || (strstr(renderer, "Gallium ") && strstr(renderer, " on ATI "))) {
		GG.device = GPU_DEVICE_ATI;
		GG.driver = GPU_DRIVER_OPENSOURCE;
	}
	else if (strstr(renderer, "Nouveau") || strstr(vendor, "nouveau")) {
		GG.device = GPU_DEVICE_NVIDIA;
		GG.driver = GPU_DRIVER_OPENSOURCE;
	}
	else if (strstr(vendor, "Mesa")) {
		GG.device = GPU_DEVICE_SOFTWARE;
		GG.driver = GPU_DRIVER_SOFTWARE;
	}
	else if (strstr(vendor, "Microsoft")) {
		GG.device = GPU_DEVICE_SOFTWARE;
		GG.driver = GPU_DRIVER_SOFTWARE;
	}
	else if (strstr(renderer, "Apple Software Renderer")) {
		GG.device = GPU_DEVICE_SOFTWARE;
		GG.driver = GPU_DRIVER_SOFTWARE;
	}
	else {
		GG.device = GPU_DEVICE_ANY;
		GG.driver = GPU_DRIVER_ANY;
	}

	if (GG.device == GPU_DEVICE_ATI) {
		/* ATI 9500 to X2300 cards support NPoT textures poorly
		 * Incomplete list http://dri.freedesktop.org/wiki/ATIRadeon
		 * New IDs from MESA's src/gallium/drivers/r300/r300_screen.c
		 */
		if (strstr(renderer, "R3")    || strstr(renderer, "RV3")      ||
		    strstr(renderer, "R4")    || strstr(renderer, "RV4")      ||
		    strstr(renderer, "RS4")   || strstr(renderer, "RC4")      ||
		    strstr(renderer, "R5")    || strstr(renderer, "RV5")      ||
		    strstr(renderer, "RS600") || strstr(renderer, "RS690")    ||
		    strstr(renderer, "RS740") || strstr(renderer, "X1")       ||
		    strstr(renderer, "X2")    || strstr(renderer, "Radeon 9") ||
		    strstr(renderer, "RADEON 9"))
		{
			GG.npotdisabled = 1;
		}
	}

#ifdef _WIN32
	GG.os = GPU_OS_WIN;
#elif defined(__APPLE__)
	GG.os = GPU_OS_MAC;
#else
	GG.os = GPU_OS_UNIX;
#endif

	GPU_basic_shaders_init();

	GPU_CHECK_NO_ERROR();
}

void GPU_extensions_exit(void)
{
	gpu_extensions_init = 0;
	GPU_codegen_exit();
	GPU_basic_shaders_exit();
}

int GPU_glsl_support(void)
{
	return !GG.extdisabled && GG.glslsupport;
}

int GPU_non_power_of_two_support(void)
{
	return GG.npotdisabled ? 0 : GLEW_ARB_texture_non_power_of_two;
}

int GPU_color_depth(void)
{
	return GG.colordepth;
}

int GPU_print_error(const char *str)
{
	GLenum errCode;
	if (G.debug & G_DEBUG) {
		if ((errCode = glGetError()) != GL_NO_ERROR) {
			fprintf(stderr, "%s opengl error: %s\n", str, gpuErrorString(errCode));
			return 1;
		}
	}
	return 0;
}
//#include FAKE_GL_MODE

static void GPU_print_framebuffer_error(GLenum status, char err_out[256])
{
	const char *err= "unknown";

	switch (status) {
		case GL_FRAMEBUFFER_COMPLETE:
			break;
		case GL_INVALID_OPERATION:
			err= "Invalid operation";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			err= "Incomplete attachment";
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			err= "Unsupported framebuffer format";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			err= "Missing attachment";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
			err= "Attached images must have same dimensions";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
			err= "Attached images must have same format";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			err= "Missing draw buffer";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			err= "Missing read buffer";
			break;
	}

	if (err_out) {
		BLI_snprintf(err_out, 256, "GPUFrameBuffer: framebuffer incomplete error %d '%s'",
			(int)status, err);
	}
	else {
		fprintf(stderr, "GPUFrameBuffer: framebuffer incomplete error %d '%s'\n",
			(int)status, err);
	}
}

/* GPUTexture */

struct GPUTexture {
	int w, h;				/* width/height */
	int number;				/* number for multitexture binding */
	int refcount;			/* reference count */
	GLenum target;			/* GL texture enum */
	GLuint bindcode;		/* opengl identifier for texture */
	int fromblender;		/* we got the texture from Blender */

	GPUFrameBuffer *fb;		/* GPUFramebuffer this texture is attached to */
	int depth;				/* is a depth texture? */

#if !defined(GLEW_NO_ES)
	// XXX jwilkins: for saving original data for shader export in ES
	GLuint  pixels_w, pixels_h;
	GLvoid* pixels;
#endif
};

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

static void GPU_glTexSubImageEmpty(GLenum target, GLenum format, int x, int y, int w, int h)
{
	void *pixels = MEM_callocN(sizeof(char)*4*w*h, "GPUTextureEmptyPixels");

#if !defined(GLEW_ES_ONLY)
	if (target == GL_TEXTURE_1D)
	{
		glTexSubImage1D(target, 0, x, w, format, GL_UNSIGNED_BYTE, pixels);
	}
	else
#endif
	{
		glTexSubImage2D(target, 0, x, y, w, h, format, GL_UNSIGNED_BYTE, pixels);
	}

	MEM_freeN(pixels);
}

static GPUTexture *GPU_texture_create_nD(int w, int h, int n, float *fpixels, int depth, char err_out[256])
{
	GPUTexture *tex;
	GLenum type, format, internalformat;
	void *pixels = NULL;

	if (depth && !(GLEW_VERSION_1_4 || GLEW_ARB_depth_texture || GLEW_OES_depth_texture))
		return NULL;

	tex           =  (GPUTexture*)MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w        =  w;
	tex->number   = -1;
	tex->refcount =  1;
	tex->depth    = depth;
#if !defined(GLEW_NO_ES)
	tex->h        =  1; // XXX jwilkins: might be able to emulate a 1D texture
	tex->target   =  GL_TEXTURE_2D;

	tex->pixels_w = tex->w;
	tex->pixels_h = tex->h;
	tex->pixels   =  NULL;
#else
	tex->h        =  h;
	tex->target   =  (n == 1) ? GL_TEXTURE_1D : GL_TEXTURE_2D;
#endif

	tex->bindcode = 0; // XXX jwilkins: there is no reason the following call to glGenTextures should fail, and if it did, i doubt it would set the value of bindcode to 0
	glGenTextures(1, &tex->bindcode);

	if (!tex->bindcode) {
		if (err_out) {
			BLI_snprintf(err_out, 256, "GPUTexture: texture create failed: %s",
				gpuErrorString(glGetError()));
		}
		else {
			fprintf(stderr, "GPUTexture: texture create failed: %s\n",
				gpuErrorString(glGetError()));
		}
		GPU_texture_free(tex);
		return NULL;
	}

	if (!GPU_non_power_of_two_support()) {
		tex->w = power_of_2_max_i(tex->w);
		tex->h = power_of_2_max_i(tex->h);
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	if (depth) {
		type           = GL_UNSIGNED_BYTE;
		format         = GL_DEPTH_COMPONENT;
		internalformat = GL_DEPTH_COMPONENT;
	}
	else {
		type           = GL_UNSIGNED_BYTE;
		format         = GL_RGBA;
		internalformat = (GLEW_VERSION_1_1 || GLEW_OES_required_internalformat) ? GL_RGBA8 : GL_RGBA;

		if (fpixels)
			pixels = GPU_texture_convert_pixels(w*h, fpixels);
	}

#if !defined(GLEW_ES_ONLY)
	if (tex->target == GL_TEXTURE_1D)
	{
		glTexImage1D(tex->target, 0, internalformat, tex->w, 0, format, type, NULL);

		if (fpixels) {
			glTexSubImage1D(tex->target, 0, 0, w, format, type, pixels ? pixels : fpixels);

			if (tex->w > w)
				GPU_glTexSubImageEmpty(tex->target, format, w, 0, tex->w-w, 1);
		}
	}
	else
#endif
	{
		glTexImage2D(tex->target, 0, internalformat, tex->w, tex->h, 0, format, type, NULL);

		if (fpixels) {
			glTexSubImage2D(tex->target, 0, 0, 0, w, h,
				format, type, pixels ? pixels : fpixels);

			if (tex->w > w)
				GPU_glTexSubImageEmpty(tex->target, format, w, 0, tex->w-w, tex->h);

			if (tex->h > h)
				GPU_glTexSubImageEmpty(tex->target, format, 0, h, w, tex->h-h);
		}
	}

#if !defined(GLEW_NO_ES)
// XXX jwilkins: save copy of texture data since ES doesn't have GetTexImage (this won't consider possible TexSubImage updates...)

	if (!pixels)
		pixels = GPU_texture_convert_pixels(w*h, fpixels);

	tex->pixels = pixels;
#else
	if (pixels)
		MEM_freeN(pixels);
#endif

	if (depth) {
		glTexParameteri(tex->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(tex->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (GLEW_ARB_shadow || GLEW_EXT_shadow_samplers) {
			glTexParameteri(tex->target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(tex->target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
#if !defined(GLEW_ES_ONLY) // XXX jwilkins: this probably won't work out in ES without this parameter
			glTexParameteri(tex->target, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
#endif
		}
	}
	else {
		glTexParameteri(tex->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(tex->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

#if !defined(GLEW_ES_ONLY)
	if (tex->target != GL_TEXTURE_1D) {
		GLenum wrapmode; // XXX jwilkins: this could probably be a function
		if (GLEW_VERSION_1_3 || GLEW_ARB_texture_border_clamp) {
			wrapmode = (depth || tex->h == 1) ? GL_CLAMP_TO_EDGE : GL_CLAMP_TO_BORDER;
		}
		else {
			wrapmode = GL_CLAMP_TO_EDGE;
		}

		glTexParameteri(tex->target, GL_TEXTURE_WRAP_S, wrapmode);
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_T, wrapmode);

#if 0
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); 
#endif
	}
	else
#endif
	{
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	}

	return tex;
}


GPUTexture *GPU_texture_create_3D(int w, int h, int depth, int channels, float *fpixels)
{
	GPUTexture *tex;
	GLenum type, format, internalformat;
	float vfBorderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	if (!(GLEW_VERSION_1_2 || GLEW_ARB_texture3D || GLEW_EXT_texture3D || GLEW_OES_texture_3D))
		return NULL;

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = h;
	tex->depth = depth;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_3D;

	glGenTextures(1, &tex->bindcode);

	if (!tex->bindcode) {
		fprintf(stderr, "GPUTexture: texture create failed: %s\n",
			gpuErrorString(glGetError()));
		GPU_texture_free(tex);
		return NULL;
	}

	if (!GPU_non_power_of_two_support()) {
		tex->w = power_of_2_max_i(tex->w);
		tex->h = power_of_2_max_i(tex->h);
		tex->depth = power_of_2_max_i(tex->depth);
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	GPU_print_error("3D glBindTexture");

	type = GL_FLOAT;
	if (channels == 4) {
		format = GL_RGBA;
		internalformat = GL_RGBA;
	}
	else {
#if !defined(GLEW_ES_ONLY)
		format = GL_RED;
		internalformat = GL_INTENSITY;
#else
		format = GL_LUMINANCE; // XXX jwilkins: format and internalFormat must match for ES!! (need to check all others)
		internalformat = GL_LUMINANCE; // XXX jwilkins: not sure of this is an ok substitute
#endif
	}

	glTexImage3D(tex->target, 0, internalformat, tex->w, tex->h, tex->depth, 0, format, type, NULL);

	GPU_print_error("3D glTexImage3D");

	if (fpixels) {
		if (!GPU_non_power_of_two_support() && (w != tex->w || h != tex->h || depth != tex->depth)) {
			/* clear first to avoid unitialized pixels */
			float *zero= MEM_callocN(sizeof(float)*tex->w*tex->h*tex->depth, "zero");
			glTexSubImage3D(tex->target, 0, 0, 0, 0, tex->w, tex->h, tex->depth, format, type, zero);
			MEM_freeN(zero);
		}

		glTexSubImage3D(tex->target, 0, 0, 0, 0, w, h, depth, format, type, fpixels);
		GPU_print_error("3D glTexSubImage3D");
	}

#if !defined(GLEW_ES_ONLY)
	glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, vfBorderColor);
#else
	// XXX jwilkins: not really sure what could be done to replace the border color
#endif
	GPU_print_error("3D GL_TEXTURE_BORDER_COLOR");
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	GPU_print_error("3D GL_LINEAR");
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	GPU_print_error("3D GL_CLAMP_TO_BORDER");

	GPU_texture_unbind(tex);

	return tex;
}

GPUTexture *GPU_texture_from_blender(Image *ima, ImageUser *iuser, int isdata, double time, int mipmap)
{
	GPUTexture *tex;
	GLint w, h, border, lastbindcode, bindcode;
//#include REAL_GL_MODE
	lastbindcode = gpuGetTextureBinding2D();

	GPU_update_image_time(ima, time);
	/* this binds a texture, so that's why to restore it with lastbindcode */
	bindcode = GPU_verify_image(ima, iuser, 0, 0, mipmap, isdata);

	if (ima->gputexture) {
		ima->gputexture->bindcode = bindcode;
		gpuBindTexture(GL_TEXTURE_2D, lastbindcode);
		return ima->gputexture;
	}

	if (!bindcode) {
		gpuBindTexture(GL_TEXTURE_2D, lastbindcode);
		return NULL;
	}

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->bindcode = bindcode;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_2D;
	tex->fromblender = 1;

	ima->gputexture= tex;

#if !defined(GLEW_ES_ONLY)
	if (!glIsTexture(tex->bindcode))
	{
		 // XXX jwilkins: how common is this error? how to detect this in ES?
		GPU_print_error("Blender Texture");
	}
	else
#endif
	{
		gpuBindTexture(GL_TEXTURE_2D, tex->bindcode);

#if !defined(GLEW_ES_ONLY)
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_BORDER, &border);
#else
		// XXX jwilkins: zeros here probably won't go over very well with the caller, but some way is needed to reconstruct the size of the original image
		w      = 0;
		h      = 0;
		border = 0;
#endif

		tex->w = w - border;
		tex->h = h - border;

		gpuBindTexture(GL_TEXTURE_2D, lastbindcode);
	}

	return tex;
}

GPUTexture *GPU_texture_from_preview(PreviewImage *prv, int mipmap)
{
	GPUTexture *tex = prv->gputexture[0];
	GLint w, h, lastbindcode;
	GLuint bindcode = 0;
	
	lastbindcode = gpuGetTextureBinding2D();
	
	if (tex)
		bindcode = tex->bindcode;
	
	/* this binds a texture, so that's why to restore it */
	if (bindcode == 0) {
		GPU_create_gl_tex(&bindcode, prv->rect[0], NULL, prv->w[0], prv->h[0], mipmap, 0, NULL);
	}
	if (tex) {
		tex->bindcode = bindcode;
		gpuBindTexture(GL_TEXTURE_2D, lastbindcode);
		return tex;
	}

	/* error binding anything */
	if (!bindcode) {
		gpuBindTexture(GL_TEXTURE_2D, lastbindcode);
		return NULL;
	}
	
	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->bindcode = bindcode;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_2D;
	
	prv->gputexture[0]= tex;

#if !defined(GLEW_ES_ONLY)
	if (!glIsTexture(tex->bindcode))
	{
		 // XXX jwilkins: how common is this error? how to detect this in ES?
		GPU_print_error("Blender Texture");
	}
	else
#endif
	{
		gpuBindTexture(GL_TEXTURE_2D, tex->bindcode);

#if !defined(GLEW_ES_ONLY)
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
#else
		// XXX jwilkins: this probably won't work very well, but ES doesn't provide this (as far I as see this moment)
		w = 0;
		h = 0;
#endif
		tex->w = w;
		tex->h = h;
	}
	
	gpuBindTexture(GL_TEXTURE_2D, lastbindcode);
	
	return tex;

}

GPUTexture *GPU_texture_create_1D(int w, float *fpixels, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, 1, 1, fpixels, 0, err_out);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

GPUTexture *GPU_texture_create_2D(int w, int h, float *fpixels, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, fpixels, 0, err_out);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

GPUTexture *GPU_texture_create_depth(int w, int h, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, NULL, 1, err_out);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

/**
 * A shadow map for VSM needs two components (depth and depth^2)
 */
GPUTexture *GPU_texture_create_vsm_shadow_map(int size, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(size, size, 2, NULL, 0, err_out);

	if ((GLEW_ARB_texture_rg || (GLEW_EXT_texture_rg && GLEW_EXT_texture_storage)) && tex) {
		/* Now we tweak some of the settings */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, size, size, 0, GL_RG, GL_FLOAT, NULL);

		GPU_texture_unbind(tex);
	}

	return tex;
}

void GPU_texture_bind(GPUTexture *tex, int number)
{
	GLenum arbnumber;

	if (number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}

	if (number == -1)
		return;

	GPU_print_error("Pre Texture Bind");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0 + number);
	if (number != 0) glActiveTexture(arbnumber);
	glBindTexture(tex->target, tex->bindcode);
	glEnable(tex->target);
	if (number != 0) glActiveTexture(GL_TEXTURE0);

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

	if (tex->number == -1)
		return;
	
	GPU_print_error("Pre Texture Unbind");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0 + tex->number);
	if (tex->number != 0) glActiveTexture(arbnumber);
	glBindTexture(tex->target, 0);
	glDisable(tex->target);
	if (tex->number != 0) glActiveTexture(GL_TEXTURE0);

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

#if !defined(GLEW_NO_ES)
		if (tex->pixels)
			MEM_freeN(tex->pixels);
#endif

		MEM_freeN(tex);
	}
}

void GPU_texture_ref(GPUTexture *tex)
{
	tex->refcount++;
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

int GPU_texture_opengl_bindcode(const GPUTexture *tex)
{
	return tex->bindcode;
}

GPUFrameBuffer *GPU_texture_framebuffer(GPUTexture *tex)
{
	return tex->fb;
}

unsigned char* GPU_texture_dup_pixels(const GPUTexture *tex, size_t* count)
{
	unsigned char* texpixels;

#if !defined(GLEW_NO_ES)
	*count = tex->pixels_w * tex->pixels_h;
#else
	*count = tex->w * tex->h;
#endif

	texpixels = (unsigned char*)MEM_mallocN(4*(*count), "RGBApixels");

#if !defined(GLEW_NO_ES)
	memcpy(texpixels, tex->pixels, 4*(*count));
#else
	{
	GLint lastbindcode = gpuGetTextureBinding2D();
	gpuBindTexture(GL_TEXTURE_2D, GPU_texture_opengl_bindcode(tex));
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, texpixels); 
	gpuBindTexture(GL_TEXTURE_2D, lastbindcode); /* restore previous value */
	}
#endif

	return texpixels;
}

/* GPUFrameBuffer */

struct GPUFrameBuffer {
	GLuint object;
	GPUTexture *colortex;
	GPUTexture *depthtex;
};

GPUFrameBuffer *GPU_framebuffer_create(void)
{
	if (GG.framebuffersupport) {
		GPUFrameBuffer *fb;

		fb = MEM_callocN(sizeof(GPUFrameBuffer), "GPUFrameBuffer");
		gpu_glGenFramebuffers(1, &fb->object);

		if (!fb->object) {
			fprintf(
				stderr,
				"GPUFFrameBuffer: framebuffer gen failed. %s\n",
				gpuErrorString(glGetError()));

			GPU_framebuffer_free(fb);

			return NULL;
		}
		else {
			return fb;
		}
	}
	else {
		return NULL;
	}
}

int GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex, char err_out[256])
{
	GLenum status;
	GLenum attachment;
	GLenum error;

	if (tex->depth)
		attachment = GL_DEPTH_ATTACHMENT;
	else
		attachment = GL_COLOR_ATTACHMENT0;

	gpu_glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
	GG.currentfb = fb->object;

	gpu_glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, 
		tex->target, tex->bindcode, 0);

	error = glGetError();

	if (error == GL_INVALID_OPERATION) {
		GPU_framebuffer_restore();
		GPU_print_framebuffer_error(error, err_out);
		return 0;
	}

#if !defined(GLEW_ES_ONLY) // XXX jwilkins: i think ES20 can only access COLOR_ATTACHMENT0 anyway
	if (tex->depth) {
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE); // XXX jwilkins: this is an invalid value!
	}
	else {
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
	}
#endif

	status = gpu_glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status != GL_FRAMEBUFFER_COMPLETE) {
		GPU_framebuffer_restore();
		GPU_print_framebuffer_error(status, err_out);
		return 0;
	}

	if (tex->depth)
		fb->depthtex = tex;
	else
		fb->colortex = tex;

	tex->fb= fb;

	return 1;
}

void GPU_framebuffer_texture_detach(GPUFrameBuffer *fb, GPUTexture *tex)
{
	GLenum attachment;

	if (!tex->fb)
		return;

	if (GG.currentfb != tex->fb->object) {
		gpu_glBindFramebuffer(GL_FRAMEBUFFER, tex->fb->object);
		GG.currentfb = tex->fb->object;
	}

	if (tex->depth) {
		fb->depthtex = NULL;
		attachment = GL_DEPTH_ATTACHMENT;
	}
	else {
		fb->colortex = NULL;
		attachment = GL_COLOR_ATTACHMENT0;
	}

	gpu_glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
		tex->target, 0, 0);

	tex->fb = NULL;
}

static GLint save_viewport[4];

void GPU_framebuffer_texture_bind(GPUFrameBuffer *UNUSED(fb), GPUTexture *tex, int w, int h)
{
	glDisable(GL_SCISSOR_TEST);

	/* bind framebuffer */
	gpu_glBindFramebuffer(GL_FRAMEBUFFER, tex->fb->object);

	/* push matrices and set default viewport and matrix */
	glGetIntegerv(GL_VIEWPORT, save_viewport);
	gpuViewport(0, 0, w, h);
	GG.currentfb = tex->fb->object;

	gpuMatrixMode(GL_PROJECTION);
	gpuPushMatrix();
	// gpuLoadIdentity(); // XXX: Why Delete?
	gpuMatrixMode(GL_MODELVIEW);
	gpuPushMatrix();
	// gpuLoadIdentity(); // XXX: Why Delete?
}

void GPU_framebuffer_texture_unbind(GPUFrameBuffer *UNUSED(fb), GPUTexture *UNUSED(tex))
{
	/* restore matrix */
	gpuMatrixMode(GL_PROJECTION);
	gpuPopMatrix();
	gpuMatrixMode(GL_MODELVIEW);
	gpuPopMatrix();

	/* restore attributes */
	gpuViewport(save_viewport[0], save_viewport[1], save_viewport[2], save_viewport[3]);
	glEnable(GL_SCISSOR_TEST);
}

void GPU_framebuffer_free(GPUFrameBuffer *fb)
{
	if (fb->depthtex)
		GPU_framebuffer_texture_detach(fb, fb->depthtex);
	if (fb->colortex)
		GPU_framebuffer_texture_detach(fb, fb->colortex);

	if (fb->object) {
		gpu_glDeleteFramebuffers(1, &fb->object);

		if (GG.currentfb == fb->object) {
			gpu_glBindFramebuffer(GL_FRAMEBUFFER, 0);
			GG.currentfb = 0;
		}
	}

	MEM_freeN(fb);
}

void GPU_framebuffer_restore(void)
{
	if (GG.currentfb != 0) {
		gpu_glBindFramebuffer(GL_FRAMEBUFFER, 0);
		GG.currentfb = 0;
	}
}

void GPU_framebuffer_blur(GPUFrameBuffer *fb, GPUTexture *tex, GPUFrameBuffer *blurfb, GPUTexture *blurtex)
{
	float scaleh[2] = {1.0f/GPU_texture_opengl_width(blurtex), 0.0f};
	float scalev[2] = {0.0f, 1.0f/GPU_texture_opengl_height(tex)};

	GPUShader *blur_shader = GPU_shader_get_builtin_shader(GPU_SHADER_SEP_GAUSSIAN_BLUR);
	int scale_uniform, texture_source_uniform;

	if (!blur_shader)
		return;

	scale_uniform = GPU_shader_get_uniform(blur_shader, "ScaleU");
	texture_source_uniform = GPU_shader_get_uniform(blur_shader, "textureSource");
		
	/* Blurring horizontally */

	/* We do the bind ourselves rather than using GPU_framebuffer_texture_bind() to avoid
	 * pushing unnecessary matrices onto the OpenGL stack. */
	gpu_glBindFramebuffer(GL_FRAMEBUFFER, blurfb->object);

	GPU_shader_bind(blur_shader);
	GPU_shader_uniform_vector(blur_shader, scale_uniform, 2, 1, (float *)scaleh);
	GPU_shader_uniform_texture(blur_shader, texture_source_uniform, tex);
	gpuViewport(0, 0, GPU_texture_opengl_width(blurtex), GPU_texture_opengl_height(blurtex));

	/* Peparing to draw quad */
	gpuMatrixMode(GL_TEXTURE);
	gpuLoadIdentity();
	gpuMatrixMode(GL_PROJECTION);
	gpuLoadIdentity();
	gpuMatrixMode(GL_MODELVIEW); /* make sure last current matrix is modelview */
	gpuLoadIdentity();

	GPU_texture_bind(tex, 0);

	// SSS Enable
	GPU_aspect_enable(GPU_ASPECT_BASIC, GPU_BASIC_TEXTURE_2D);

	gpuImmediateFormat_T2_V2();

	/* Drawing quad */
	gpuBegin(GL_TRIANGLE_FAN);
	gpuTexCoord2f(0, 0); gpuVertex2f(1, 1);
	gpuTexCoord2f(1, 0); gpuVertex2f(-1, 1);
	gpuTexCoord2f(1, 1); gpuVertex2f(-1, -1);
	gpuTexCoord2f(0, 1); gpuVertex2f(1, -1);
	gpuEnd();
		
	/* Blurring vertically */

	gpu_glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
	gpuViewport(0, 0, GPU_texture_opengl_width(tex), GPU_texture_opengl_height(tex));
	GPU_shader_uniform_vector(blur_shader, scale_uniform, 2, 1, (float *)scalev);
	GPU_shader_uniform_texture(blur_shader, texture_source_uniform, blurtex);
	GPU_texture_bind(blurtex, 0);

	gpuBegin(GL_TRIANGLE_FAN);
	gpuTexCoord2f(0, 0); gpuVertex2f(1, 1);
	gpuTexCoord2f(1, 0); gpuVertex2f(-1, 1);
	gpuTexCoord2f(1, 1); gpuVertex2f(-1, -1);
	gpuTexCoord2f(0, 1); gpuVertex2f(1, -1);
	gpuEnd();

	gpuImmediateUnformat();

	// SSS Disable
	GPU_aspect_disable(GPU_ASPECT_BASIC, GPU_BASIC_TEXTURE_2D);

	GPU_shader_unbind();
}

/* GPUOffScreen */

struct GPUOffScreen {
	GPUFrameBuffer *fb;
	GPUTexture *color;
	GPUTexture *depth;

	/* requested width/height, may be smaller than actual texture size due
	 * to missing non-power of two support, so we compensate for that */
	int w, h;
};

GPUOffScreen *GPU_offscreen_create(int width, int height, char err_out[256])
{
	GPUOffScreen *ofs;

	ofs= MEM_callocN(sizeof(GPUOffScreen), "GPUOffScreen");
	ofs->w= width;
	ofs->h= height;

	ofs->fb = GPU_framebuffer_create();
	if (!ofs->fb) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	ofs->depth = GPU_texture_create_depth(width, height, err_out);
	if (!ofs->depth) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	if (!GPU_framebuffer_texture_attach(ofs->fb, ofs->depth, err_out)) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	ofs->color = GPU_texture_create_2D(width, height, NULL, err_out);
	if (!ofs->color) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	if (!GPU_framebuffer_texture_attach(ofs->fb, ofs->color, err_out)) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	GPU_framebuffer_restore();

	return ofs;
}

void GPU_offscreen_free(GPUOffScreen *ofs)
{
	if (ofs->fb)
		GPU_framebuffer_free(ofs->fb);
	if (ofs->color)
		GPU_texture_free(ofs->color);
	if (ofs->depth)
		GPU_texture_free(ofs->depth);
	
	MEM_freeN(ofs);
}

void GPU_offscreen_bind(GPUOffScreen *ofs)
{
	glDisable(GL_SCISSOR_TEST);
	GPU_framebuffer_texture_bind(ofs->fb, ofs->color, ofs->w, ofs->h);
}

void GPU_offscreen_unbind(GPUOffScreen *ofs)
{
	GPU_framebuffer_texture_unbind(ofs->fb, ofs->color);
	GPU_framebuffer_restore();
	glEnable(GL_SCISSOR_TEST);
}

void GPU_offscreen_read_pixels(GPUOffScreen *ofs, int type, void *pixels)
{
	glReadPixels(0, 0, ofs->w, ofs->h, GL_RGBA, type, pixels);
}

int GPU_offscreen_width(GPUOffScreen *ofs)
{
	return ofs->w;
}

int GPU_offscreen_height(GPUOffScreen *ofs)
{
	return ofs->h;
}

static void shader_print_errors(const char *task, char *log, const char *code)
{
	const char *c, *pos, *end = code + strlen(code);
	int line = 1;

	fprintf(stderr, "GPUShader: %s error:\n", task);

	if (G.debug & G_DEBUG) {
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
static const char *gpu_shader_standard_extensions(void)
{
	/* need this extensions for high quality bump mapping */
	if (GPU_bicubic_bump_support()) {
		return "#version 130\n"
		       "#extension GL_ARB_texture_query_lod: enable\n"
		       "#define BUMP_BICUBIC\n";
	}

	return "";
}

static const char *gpu_shader_standard_defines(void)
{
	/* some useful defines to detect GPU type */
	if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY))
		return "#define GPU_ATI\n";
	else if(GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY))
		return "#define GPU_NVIDIA\n";
	else if(GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY))
		return "#define GPU_INTEL\n";
	
	return "";
}

GPUShader *GPU_shader_create(const char *vertexcode, const char *fragcode, const char *libcode, const char *defines)
{
	GLint status;
	char log[5000];
	GLsizei length = 0;
	GPUShader *shader;

	if (!GG.glslsupport) {
		return NULL;
	}

	shader = MEM_callocN(sizeof(GPUShader), "GPUShader");

	if (vertexcode)
		shader->vertex = gpu_glCreateShader(GL_VERTEX_SHADER);
	if (fragcode)
		shader->fragment = gpu_glCreateShader(GL_FRAGMENT_SHADER);
	shader->object = gpu_glCreateProgram();

	if (!shader->object ||
	    (vertexcode && !shader->vertex) ||
	    (fragcode && !shader->fragment))
	{
		fprintf(stderr, "GPUShader, object creation failed.\n");
		GPU_shader_free(shader);
		return NULL;
	}

	if (vertexcode) {
		const char *source[4];
		int num_source = 0;

		source[num_source++] = gpu_shader_standard_extensions();
		source[num_source++] = gpu_shader_standard_defines();

		if (defines) source[num_source++] = defines;
		if (vertexcode) source[num_source++] = vertexcode;

		gpu_glAttachShader(shader->object, shader->vertex);
		gpu_glShaderSource(shader->vertex, num_source, source, NULL);

		gpu_glCompileShader(shader->vertex);
		gpu_glGetShaderiv(shader->vertex, GL_COMPILE_STATUS, &status);

		if (!status) {
			gpu_glGetShaderInfoLog(shader->vertex, sizeof(log), &length, log);
			shader_print_errors("compile", log, vertexcode);

			GPU_shader_free(shader);
			return NULL;
		}
	}

	if (fragcode) {
		const char *source[5];
		int num_source = 0;

		source[num_source++] = gpu_shader_standard_extensions();
		source[num_source++] = gpu_shader_standard_defines();

		if (defines) source[num_source++] = defines;
		if (libcode) source[num_source++] = libcode;
		if (fragcode) source[num_source++] = fragcode;

		gpu_glAttachShader(shader->object, shader->fragment);
		gpu_glShaderSource(shader->fragment, num_source, source, NULL);

		gpu_glCompileShader(shader->fragment);
		gpu_glGetShaderiv(shader->fragment, GL_COMPILE_STATUS, &status);

		if (!status) {
			gpu_glGetShaderInfoLog(shader->fragment, sizeof(log), &length, log);
			shader_print_errors("compile", log, fragcode);

			GPU_shader_free(shader);
			return NULL;
		}
	}

#if 0
	if (lib && lib->lib)
		gpuAttachShader(shader->object, lib->lib);
#endif

	gpu_glLinkProgram(shader->object);
	gpu_glGetProgramiv(shader->object, GL_LINK_STATUS, &status);
	if (!status) {
		gpu_glGetProgramInfoLog(shader->object, sizeof(log), &length, log);
		if (fragcode) shader_print_errors("linking", log, fragcode);
		else if (vertexcode) shader_print_errors("linking", log, vertexcode);
		else if (libcode) shader_print_errors("linking", log, libcode);

		GPU_shader_free(shader);
		return NULL;
	}
	return shader;
}

#if 0
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
		glGetInfoLogARB(shader->lib, sizeof(log), &length, log);
		shader_print_errors("compile", log, code);

		GPU_shader_free(shader);
		return NULL;
	}

	return shader;
}
#endif

void GPU_shader_bind(GPUShader *shader)
{
	GPU_print_error("Pre Shader Bind");
	gpu_glUseProgram(shader->object);
	GPU_print_error("Post Shader Bind");
}

void GPU_shader_unbind(void)
{
	GPU_print_error("Pre Shader Unbind");
	gpu_glUseProgram(0);
	GPU_print_error("Post Shader Unbind");
}

void GPU_shader_free(GPUShader *shader)
{
	if (shader->lib)
		gpu_glDeleteShader(shader->lib);
	if (shader->vertex)
		gpu_glDeleteShader(shader->vertex);
	if (shader->fragment)
		gpu_glDeleteShader(shader->fragment);
	if (shader->object)
		gpu_glDeleteProgram(shader->object);
	MEM_freeN(shader);

}

int GPU_shader_get_attrib(GPUShader *shader, const char *name)
{
	return gpu_glGetAttribLocation(shader->object, name);
}

int GPU_shader_get_uniform(GPUShader *shader, const char *name)
{
	return gpu_glGetUniformLocation(shader->object, name);
}

void GPU_shader_uniform_vector(GPUShader *UNUSED(shader), int location, int length, int arraysize, float *value)
{
	if (location == -1)
		return;

	GPU_print_error("Pre Uniform Vector");

	if (length == 1) gpu_glUniform1fv(location, arraysize, value);
	else if (length == 2) gpu_glUniform2fv(location, arraysize, value);
	else if (length == 3) gpu_glUniform3fv(location, arraysize, value);
	else if (length == 4) gpu_glUniform4fv(location, arraysize, value);
	else if (length == 9) gpu_glUniformMatrix3fv(location, arraysize, 0, value);
	else if (length == 16) gpu_glUniformMatrix4fv(location, arraysize, 0, value);

	GPU_print_error("Post Uniform Vector");
}

void GPU_shader_uniform_int(GPUShader *UNUSED(shader), int location, int value)
{
	if (location == -1)
		return;

	GPU_print_error("Pre Uniform Int");
	gpu_glUniform1i(location, value);
	GPU_print_error("Post Uniform Int");
}

void GPU_shader_uniform_texture(GPUShader *UNUSED(shader), int location, GPUTexture *tex)
{
	GLenum arbnumber;

	if (tex->number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}
		
	if (tex->number == -1)
		return;

	if (location == -1)
		return;

	GPU_print_error("Pre Uniform Texture");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0 + tex->number);

	if (tex->number != 0) 
		glActiveTexture(arbnumber);
	glBindTexture(tex->target, tex->bindcode);
	gpu_glUniform1i(location, tex->number);
	glEnable(tex->target);
	if (tex->number != 0) 
		glActiveTexture(GL_TEXTURE0);

	GPU_print_error("Post Uniform Texture");
}

int GPU_shader_get_attribute(GPUShader *shader, const char *name)
{
	int index;
	
	GPU_print_error("Pre Get Attribute");
	index = gpu_glGetAttribLocation(shader->object, name);
	GPU_print_error("Post Get Attribute");

	return index;
}

GPUShader *GPU_shader_get_builtin_shader(GPUBuiltinShader shader)
{
	GPUShader *retval = NULL;

	switch (shader) {
		case GPU_SHADER_VSM_STORE:
			if (!GG.shaders.vsm_store)
				GG.shaders.vsm_store = GPU_shader_create(datatoc_gpu_shader_vsm_store_vert_glsl, datatoc_gpu_shader_vsm_store_frag_glsl, NULL, NULL);
			retval = GG.shaders.vsm_store;
			break;
		case GPU_SHADER_SEP_GAUSSIAN_BLUR:
			if (!GG.shaders.sep_gaussian_blur)
				GG.shaders.sep_gaussian_blur = GPU_shader_create(datatoc_gpu_shader_sep_gaussian_blur_vert_glsl, datatoc_gpu_shader_sep_gaussian_blur_frag_glsl, NULL, NULL);
			retval = GG.shaders.sep_gaussian_blur;
			break;
	}

	if (retval == NULL)
		printf("Unable to create a GPUShader for builtin shader: %d\n", shader);

	return retval;
}

void GPU_shader_free_builtin_shaders(void)
{
	if (GG.shaders.vsm_store) {
		MEM_freeN(GG.shaders.vsm_store);
		GG.shaders.vsm_store = NULL;
	}

	if (GG.shaders.sep_gaussian_blur) {
		MEM_freeN(GG.shaders.sep_gaussian_blur);
		GG.shaders.sep_gaussian_blur = NULL;
	}
}

#if 0
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
#endif

