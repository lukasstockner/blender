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

/* my interface */
#include "intern/gpu_blender_aspect_intern.h"

/* my library */
#include "GPU_basic.h"
#include "GPU_font.h"
#include "GPU_pixels.h"
#include "GPU_raster.h"
#include "GPU_sprite.h"

/* external */
#include "BLI_utildefines.h"



uint32_t GPU_ASPECT_FONT   = 0;
uint32_t GPU_ASPECT_BASIC  = 0;
uint32_t GPU_ASPECT_PIXELS = 0;
uint32_t GPU_ASPECT_RASTER = 0;
uint32_t GPU_ASPECT_SPRITE = 0;



static bool font_end(void* UNUSED(param), const void* UNUSED(object))
{
	GPU_font_unbind();

	return true;
}

static void font_commit(void* UNUSED(param))
{
	GPU_font_bind();
}

GPUaspectimpl GPU_ASPECTIMPL_FONT = {
	NULL,        /* begin   */
	font_end,    /* end     */
	font_commit, /* commit  */
	NULL,        /* enable  */
	NULL,        /* disable */
};



static bool pixels_end(void* UNUSED(param), const void* UNUSED(object))
{
	GPU_pixels_unbind();

	return true;
}

static void pixels_commit(void* UNUSED(param))
{
	GPU_pixels_bind();
}

GPUaspectimpl GPU_ASPECTIMPL_PIXELS = {
	NULL,          /* begin   */
	pixels_end,    /* end     */
	pixels_commit, /* commit  */
	NULL,          /* enable  */
	NULL,          /* disable */
};



static bool basic_end(void* UNUSED(param), const void* UNUSED(object))
{
	GPU_basic_unbind();

	return true;
}

static void basic_commit(void* UNUSED(param))
{
	GPU_basic_bind();
}

static void basic_enable(void* UNUSED(param), uint32_t options)
{
	GPU_basic_enable(options);
}

static void basic_disable(void* UNUSED(param), uint32_t options)
{
	GPU_basic_disable(options);
}

GPUaspectimpl GPU_ASPECTIMPL_BASIC = {
	NULL,         /* begin   */
	basic_end,    /* end     */
	basic_commit, /* commit  */
	basic_enable, /* enable  */
	basic_disable /* disable */
};



static bool raster_end(void* UNUSED(param), const void* UNUSED(object))
{
	GPU_raster_unbind();

	return true;
}

static void raster_commit(void* UNUSED(param))
{
	GPU_raster_bind();
}

static void raster_enable(void* UNUSED(param), uint32_t options)
{
	GPU_raster_enable(options);
}

static void raster_disable(void* UNUSED(param), uint32_t options)
{
	GPU_raster_disable(options);
}

GPUaspectimpl GPU_ASPECTIMPL_RASTER = {
	NULL,          /* begin   */
	raster_end,    /* end     */
	raster_commit, /* commit  */
	raster_enable, /* enable  */
	raster_disable /* disable */
};



static bool sprite_end(void* UNUSED(param), const void* UNUSED(object))
{
	GPU_sprite_unbind();

	return true;
}

static void sprite_commit(void* UNUSED(param))
{
	GPU_sprite_bind();
}

static void sprite_enable(void* UNUSED(param), uint32_t options)
{
	GPU_sprite_enable(options);
}

static void sprite_disable(void* UNUSED(param), uint32_t options)
{
	GPU_sprite_disable(options);
}

GPUaspectimpl GPU_ASPECTIMPL_SPRITE = {
	NULL,          /* begin   */
	sprite_end,    /* end     */
	sprite_commit, /* commit  */
	sprite_enable, /* enable  */
	sprite_disable /* disable */
};



void gpu_initialize_aspect_impl(void)
{
	GPU_gen_aspects(1, &GPU_ASPECT_FONT);
	GPU_gen_aspects(1, &GPU_ASPECT_BASIC);
	GPU_gen_aspects(1, &GPU_ASPECT_PIXELS);
	GPU_gen_aspects(1, &GPU_ASPECT_RASTER);
	GPU_gen_aspects(1, &GPU_ASPECT_SPRITE);

	GPU_aspect_impl(GPU_ASPECT_FONT,   &GPU_ASPECTIMPL_FONT);
	GPU_aspect_impl(GPU_ASPECT_BASIC,  &GPU_ASPECTIMPL_BASIC);
	GPU_aspect_impl(GPU_ASPECT_PIXELS, &GPU_ASPECTIMPL_PIXELS);
	GPU_aspect_impl(GPU_ASPECT_RASTER, &GPU_ASPECTIMPL_RASTER);
	GPU_aspect_impl(GPU_ASPECT_SPRITE, &GPU_ASPECTIMPL_SPRITE);
}



void gpu_shutdown_aspect_impl(void)
{
	GPU_delete_aspects(1, &GPU_ASPECT_FONT);
	GPU_delete_aspects(1, &GPU_ASPECT_BASIC);
	GPU_delete_aspects(1, &GPU_ASPECT_PIXELS);
	GPU_delete_aspects(1, &GPU_ASPECT_RASTER);
	GPU_delete_aspects(1, &GPU_ASPECT_SPRITE);
}
