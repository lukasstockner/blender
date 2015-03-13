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

#include "IMB_imbuf.h"

#include "DNA_space_types.h"

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
	engine->refcount = 1;

	return engine;
}

/** Shalow copy only (i.e. memory is 100% shared, just increases refcount). */
AssetEngine *BKE_asset_engine_copy(AssetEngine *engine)
{
	engine->refcount++;
	return engine;
}

void BKE_asset_engine_free(AssetEngine *engine)
{
	if (engine->refcount-- == 1) {
#ifdef WITH_PYTHON
		if (engine->py_instance) {
			BPY_DECREF_RNA_INVALIDATE(engine->py_instance);
		}
#endif

		MEM_freeN(engine);
	}
}


/* API helpers. */

void BKE_asset_engine_load_pre(AssetEngine *engine, FileDirEntryArr *r_entries)
{
	if (engine->type->load_pre) {
		AssetUUIDList *uuids = MEM_mallocN(sizeof(*uuids), __func__);
		FileDirEntry *en;
		const int nbr_entries = r_entries->nbr_entries;
		int i;

		uuids->uuids = MEM_mallocN(sizeof(*uuids->uuids) * nbr_entries, __func__);
		uuids->nbr_uuids = nbr_entries;

		for (i = 0, en = r_entries->entries.first; en; i++, en = en->next) {
			FileDirEntryVariant *var = BLI_findlink(&en->variants, en->act_variant);
			AssetUUID *uuid = &uuids->uuids[i];

			memcpy(uuid->uuid_asset, en->uuid, sizeof(uuid->uuid_asset));

			BLI_assert(var);
			memcpy(uuid->uuid_variant, var->uuid, sizeof(uuid->uuid_variant));

			memcpy(uuid->uuid_revision, en->entry->uuid, sizeof(uuid->uuid_revision));
		}

		BKE_filedir_entryarr_clear(r_entries);

		if (!engine->type->load_pre(engine, uuids, r_entries)) {
			/* If load_pre returns false (i.e. fails), clear all paths! */
			/* TODO: report!!! */
			BKE_filedir_entryarr_clear(r_entries);
		}

		MEM_freeN(uuids);
	}
}


/* FileDirxxx handling. */

void BKE_filedir_variant_free(FileDirEntryVariant *var)
{
	if (var->name) {
		MEM_freeN(var->name);
	}
	if (var->description) {
		MEM_freeN(var->description);
	}
	BLI_freelistN(&var->revisions);
}

void BKE_filedir_entry_free(FileDirEntry *entry)
{
	if (entry->name) {
		MEM_freeN(entry->name);
	}
	if (entry->description) {
		MEM_freeN(entry->description);
	}
	if (entry->relpath) {
		MEM_freeN(entry->relpath);
	}
	if (entry->image) {
		IMB_freeImBuf(entry->image);
	}
	/* For now, consider FileDirEntryRevision::poin as not owned here, so no need to do anything about it */

	if (!BLI_listbase_is_empty(&entry->variants)) {
		FileDirEntryVariant *var;

		for (var = entry->variants.first; var; var = var->next) {
			BKE_filedir_variant_free(var);
		}

		BLI_freelistN(&entry->variants);
	}
	else if (entry->entry){
		MEM_freeN(entry->entry);
	}

	/* TODO: tags! */
}

void BKE_filedir_entry_clear(FileDirEntry *entry)
{
	BKE_filedir_entry_free(entry);
	memset(entry, 0, sizeof(*entry));
}

/** Perform and return a full (deep) duplicate of given entry. */
FileDirEntry *BKE_filedir_entry_copy(FileDirEntry *entry)
{
	FileDirEntry *entry_new = MEM_dupallocN(entry);

	if (entry->name) {
		entry_new->name = MEM_dupallocN(entry->name);
	}
	if (entry->description) {
		entry_new->description = MEM_dupallocN(entry->description);
	}
	if (entry->relpath) {
		entry_new->relpath = MEM_dupallocN(entry->relpath);
	}
	if (entry->image) {
		entry_new->image = IMB_dupImBuf(entry->image);
	}
	/* For now, consider FileDirEntryRevision::poin as not owned here, so no need to do anything about it */

	entry_new->entry = NULL;
	if (!BLI_listbase_is_empty(&entry->variants)) {
		FileDirEntryVariant *var;
		int act_var;

		BLI_listbase_clear(&entry_new->variants);
		for (act_var = 0, var = entry->variants.first; var; act_var++, var = var->next) {
			FileDirEntryVariant *var_new = MEM_dupallocN(var);
			FileDirEntryRevision *rev;
			const bool is_act_var = (act_var == entry->act_variant);
			int act_rev;

			if (var->name) {
				var_new->name = MEM_dupallocN(var->name);
			}
			if (var->description) {
				var_new->description = MEM_dupallocN(var->description);
			}

			BLI_listbase_clear(&var_new->revisions);
			for (act_rev = 0, rev = var->revisions.first; rev; act_rev++, rev = rev->next) {
				FileDirEntryRevision *rev_new = MEM_dupallocN(rev);
				const bool is_act_rev = (act_rev == var->act_revision);

				BLI_addtail(&var_new->revisions, rev_new);

				if (is_act_var && is_act_rev) {
					entry_new->entry = rev_new;
				}
			}

			BLI_addtail(&entry_new->variants, var_new);
		}

	}
	else if (entry->entry){
		entry_new->entry = MEM_dupallocN(entry->entry);
	}

	BLI_assert(entry_new->entry != NULL);

	/* TODO: tags! */

	return entry_new;
}

void BKE_filedir_entryarr_clear(FileDirEntryArr *array)
{
	FileDirEntry *entry;

	for (entry = array->entries.first; entry; entry = entry->next) {
		BKE_filedir_entry_free(entry);
	}
	BLI_freelistN(&array->entries);
    array->nbr_entries = 0;
}

bool BKE_filedir_entry_is_selected(FileDirEntry *entry, FileCheckType check)
{
	switch (check) {
		case CHECK_DIRS:
			return ((entry->typeflag & FILE_TYPE_DIR) != 0) && (entry->selflag & FILE_SEL_SELECTED);
		case CHECK_FILES:
			return ((entry->typeflag & FILE_TYPE_DIR) == 0) && (entry->selflag & FILE_SEL_SELECTED);
		case CHECK_ALL:
		default:
			return (entry->selflag & FILE_SEL_SELECTED) != 0;
	}
}
