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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_blender_aspect.c
 *  \ingroup gpu
 */

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#include "GPU_aspect.h"

#include "intern/gpu_private.h"



uint32_t GPU_ASPECT_BASIC   = 0;
uint32_t GPU_ASPECT_CODEGEN = 0;
uint32_t GPU_ASPECT_FONT    = 0;
uint32_t GPU_ASPECT_PIXELS  = 0;
uint32_t GPU_ASPECT_RASTER  = 0;
uint32_t GPU_ASPECT_SPRITE  = 0;



static bool font_end(const void* UNUSED(object), void* UNUSED(param))
{
	gpu_font_unbind();

	return true;
}

static bool font_commit(const void* UNUSED(object))
{
	gpu_font_bind();

	return true;
}

GPUaspectimpl GPU_ASPECTIMPL_FONT = {
	NULL,        /* render_begin  */
	font_end,    /* render_end    */
	font_commit, /* render_commit */
	NULL,        /* select_begin  */
	NULL,        /* select_end    */
	NULL,        /* select_commit */
	NULL,        /* enable        */
	NULL,        /* disable       */
};



static bool pixels_end(const void* UNUSED(object), void* UNUSED(param))
{
	gpu_pixels_unbind();

	return true;
}

static bool pixels_commit(const void* UNUSED(object))
{
	gpu_pixels_bind();

	return true;
}

GPUaspectimpl GPU_ASPECTIMPL_PIXELS = {
	NULL,          /* render_begin  */
	pixels_end,    /* render_end    */
	pixels_commit, /* render_commit */
	NULL,          /* select_begin  */
	NULL,          /* select_end    */
	NULL,          /* select_commit */
	NULL,          /* enable        */
	NULL,          /* disable       */
};



static bool basic_end(const void* UNUSED(object), void* UNUSED(param))
{
	gpu_basic_unbind();

	return true;
}

static bool basic_commit(const void* UNUSED(object))
{
	gpu_basic_bind();

	return true;
}

static void basic_enable(const void* UNUSED(object), uint32_t options)
{
	gpu_basic_enable(options);
}

static void basic_disable(const void* UNUSED(object), uint32_t options)
{
	gpu_basic_disable(options);
}

GPUaspectimpl GPU_ASPECTIMPL_BASIC = {
	NULL,                      /* render_begin  */
	basic_end,                 /* render_end    */
	basic_commit,              /* render_commit */
	gpu_default_select_begin,  /* select_begin  */
	gpu_default_select_end,    /* select_end    */
	gpu_default_select_commit, /* select_commit */
	basic_enable,              /* enable        */
	basic_disable              /* disable       */
};



GPUaspectimpl GPU_ASPECTIMPL_CODEGEN = {
	NULL, /* render_begin  */
	NULL, /* render_end    */
	NULL, /* render_commit */
	NULL, /* select_begin  */
	NULL, /* select_end    */
	NULL, /* select_commit */
	NULL, /* enable        */
	NULL, /* disable       */
};



static bool raster_end(const void* UNUSED(object), void* UNUSED(param))
{
	gpu_raster_unbind();

	return true;
}

static bool raster_commit(const void* UNUSED(object))
{
	gpu_raster_bind();

	return true;
}

static void raster_enable(const void* UNUSED(object), uint32_t options)
{
	gpu_raster_enable(options);
}

static void raster_disable(const void* UNUSED(object), uint32_t options)
{
	gpu_raster_disable(options);
}

GPUaspectimpl GPU_ASPECTIMPL_RASTER = {
	NULL,                      /* render_begin  */
	raster_end,                /* render_end    */
	raster_commit,             /* render_commit */
	gpu_default_select_begin,  /* select_begin  */
	gpu_default_select_end,    /* select_end    */
	gpu_default_select_commit, /* select_commit */
	raster_enable,             /* enable        */
	raster_disable             /* disable       */
};



static bool sprite_end(const void* UNUSED(object), void* UNUSED(param))
{
	gpu_sprite_unbind();

	return true;
}

static bool sprite_commit(const void* UNUSED(object))
{
	gpu_sprite_bind();

	return true;
}

static void sprite_enable(const void* UNUSED(object), uint32_t options)
{
	gpu_sprite_enable(options);
}

static void sprite_disable(const void* UNUSED(object), uint32_t options)
{
	gpu_sprite_disable(options);
}

GPUaspectimpl GPU_ASPECTIMPL_SPRITE = {
	NULL,                      /* begin         */
	sprite_end,                /* end           */
	sprite_commit,             /* commit        */
	gpu_default_select_begin,  /* select_begin  */
	gpu_default_select_end,    /* select_end    */
	gpu_default_select_commit, /* select_commit */
	sprite_enable,             /* enable        */
	sprite_disable             /* disable       */
};



void gpu_blender_aspect_init(void)
{
	GPU_gen_aspects(1, &GPU_ASPECT_BASIC);
	GPU_gen_aspects(1, &GPU_ASPECT_CODEGEN);
	GPU_gen_aspects(1, &GPU_ASPECT_FONT);
	GPU_gen_aspects(1, &GPU_ASPECT_PIXELS);
	GPU_gen_aspects(1, &GPU_ASPECT_RASTER);
	GPU_gen_aspects(1, &GPU_ASPECT_SPRITE);

	GPU_aspect_impl(GPU_ASPECT_BASIC,  &GPU_ASPECTIMPL_BASIC);
	GPU_aspect_impl(GPU_ASPECT_CODEGEN,  &GPU_ASPECTIMPL_CODEGEN);
	GPU_aspect_impl(GPU_ASPECT_FONT,   &GPU_ASPECTIMPL_FONT);
	GPU_aspect_impl(GPU_ASPECT_PIXELS, &GPU_ASPECTIMPL_PIXELS);
	GPU_aspect_impl(GPU_ASPECT_RASTER, &GPU_ASPECTIMPL_RASTER);
	GPU_aspect_impl(GPU_ASPECT_SPRITE, &GPU_ASPECTIMPL_SPRITE);
}



void gpu_blender_aspect_exit(void)
{
	GPU_delete_aspects(1, &GPU_ASPECT_BASIC);
	GPU_delete_aspects(1, &GPU_ASPECT_CODEGEN);
	GPU_delete_aspects(1, &GPU_ASPECT_FONT);
	GPU_delete_aspects(1, &GPU_ASPECT_PIXELS);
	GPU_delete_aspects(1, &GPU_ASPECT_RASTER);
	GPU_delete_aspects(1, &GPU_ASPECT_SPRITE);
}