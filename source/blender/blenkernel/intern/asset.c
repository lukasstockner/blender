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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/asset.c
 *  \ingroup bke
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "BLF_translation.h"

#include "BLI_fileops_types.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_asset.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

/* Asset engine types (none intern!). */

ListBase asset_engines = {NULL, NULL};

void BKE_asset_engines_init(void)
{
	/* We just add a dummy engine, which 'is' our intern filelisting code from space_file! */
	AssetEngineType *aet = MEM_callocN(sizeof(*aet), __func__);

	BLI_strncpy(aet->idname, AE_FAKE_ENGINE_ID, sizeof(aet->idname));
	BLI_strncpy(aet->name, "None", sizeof(aet->name));

	BLI_addhead(&asset_engines, aet);
}

void BKE_asset_engines_exit(void)
{
	AssetEngineType *type, *next;

	for (type = asset_engines.first; type; type = next) {
		next = type->next;

		BLI_remlink(&asset_engines, type);

		if (type->ext.free) {
			type->ext.free(type->ext.data);
		}

		MEM_freeN(type);
	}
}

AssetEngineType *BKE_asset_engines_find(const char *idname)
{
	AssetEngineType *type;

	type = BLI_findstring(&asset_engines, idname, offsetof(AssetEngineType, idname));

	return type;
}

/* Asset engine instances. */

/* Create, Free */

AssetEngine *BKE_asset_engine_create(AssetEngineType *type)
{
	AssetEngine *engine;

	BLI_assert(type);

	engine = MEM_callocN(sizeof(AssetEngine), __func__);
	engine->type = type;

	return engine;
}

void BKE_asset_engine_free(AssetEngine *engine)
{
#ifdef WITH_PYTHON
	if (engine->py_instance) {
		BPY_DECREF_RNA_INVALIDATE(engine->py_instance);
	}
#endif

	MEM_freeN(engine);
}
