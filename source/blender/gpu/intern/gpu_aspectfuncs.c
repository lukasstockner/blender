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

/** \file blender/gpu/intern/gpu_aspectfuncs.c
*  \ingroup gpu
*/

#define GPU_ASPECT_INTERN
#include "intern/gpu_aspectfuncs.h"

#include "GPU_basic_shader.h"
#include "GPU_font_shader.h"

#include "BLI_utildefines.h"



static bool font_end(void* UNUSED(param), const void* UNUSED(object))
{
	GPU_font_shader_unbind();

	return true;
}

static void font_commit(void* UNUSED(param))
{
	GPU_font_shader_bind();
}

GPUaspectfuncs GPU_ASPECTFUNCS_FONT = {
	NULL,        /* begin   */
	font_end,    /* end     */
	font_commit, /* commit  */
	NULL,        /* enable  */
	NULL,        /* disable */
};



static bool basic_end(void* UNUSED(param), const void* UNUSED(object))
{
	GPU_basic_shader_unbind();

	return true;
}

static void basic_commit(void* UNUSED(param))
{
	GPU_basic_shader_bind();
}

static void basic_enable(void* UNUSED(param), uint32_t options)
{
	GPU_basic_shader_enable(options);
}

static void basic_disable(void* UNUSED(param), uint32_t options)
{
	GPU_basic_shader_disable(options);
}

GPUaspectfuncs GPU_ASPECTFUNCS_BASIC = {
	NULL,         /* begin   */
	basic_end,    /* end     */
	basic_commit, /* commit  */
	basic_enable, /* enable  */
	basic_disable /* disable */
};



void gpu_initialize_aspect_funcs(void)
{
	GPU_gen_aspects(1, &GPU_ASPECT_FONT);
	GPU_gen_aspects(1, &GPU_ASPECT_BASIC);

	GPU_aspect_funcs(GPU_ASPECT_FONT,  &GPU_ASPECTFUNCS_FONT);
	GPU_aspect_funcs(GPU_ASPECT_BASIC, &GPU_ASPECTFUNCS_BASIC);
}



void gpu_shutdown_aspect_funcs(void)
{
	GPU_delete_aspects(1, &GPU_ASPECT_FONT);
	GPU_delete_aspects(1, &GPU_ASPECT_BASIC);
}
