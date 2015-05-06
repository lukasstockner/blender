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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/hair/hair_object_cachelib.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_cache_library_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_strands_types.h"

#include "BKE_anim.h"
#include "BKE_cache_library.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_strands.h"

#include "bmesh.h"

#include "hair_intern.h"

bool ED_hair_object_has_hair_cache_data(Object *ob)
{
	return BKE_cache_modifier_strands_key_get(ob, NULL, NULL, NULL, NULL, NULL, NULL);
}

bool ED_hair_object_init_cache_edit(Object *ob)
{
	StrandsKeyCacheModifier *skmd;
	DerivedMesh *dm;
	Strands *strands;
	float mat[4][4];
	
	if (!BKE_cache_modifier_strands_key_get(ob, &skmd, &dm, &strands, NULL, NULL, mat))
		return false;
	
	if (!skmd->edit) {
		BMesh *bm = BKE_cache_strands_to_bmesh(strands, skmd->key, mat, skmd->shapenr - 1, dm);
		
		skmd->edit = BKE_editstrands_create(bm, dm, mat);
	}
	
	return true;
}

bool ED_hair_object_apply_cache_edit(Object *ob)
{
	StrandsKeyCacheModifier *skmd;
	DerivedMesh *dm;
	Strands *strands;
	DupliObjectData *dobdata;
	const char *name;
	float mat[4][4];
	
	if (!BKE_cache_modifier_strands_key_get(ob, &skmd, &dm, &strands, &dobdata, &name, mat))
		return false;
	
	if (skmd->edit) {
		Strands *nstrands;
		
		nstrands = BKE_cache_strands_from_bmesh(skmd->edit, skmd->key, mat, dm);
		
		BKE_dupli_object_data_add_strands(dobdata, name, nstrands);
		
		BKE_editstrands_free(skmd->edit);
		MEM_freeN(skmd->edit);
		skmd->edit = NULL;
	}
	
	return true;
}
