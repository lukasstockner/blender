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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/workflow_shaders.c
 *  \ingroup bke
 */

#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BKE_library.h"
#include "BKE_workflow_shaders.h"


void BKE_workflow_shader_free(GPUWorkflowShader *shader)
{

}

struct GPUWorkflowShader *BKE_workflow_shader_add(struct Main *bmain, const char *name)
{
	GPUWorkflowShader *wfshader;

	wfshader = BKE_libblock_alloc(bmain, ID_GPUWS, name);

	/* enable fake user by default */
	wfshader->id.flag |= LIB_FAKEUSER;

	/* initially active only for object mode */
	wfshader->objectmode |= OB_MODE_OBJECT;

	return wfshader;
}
