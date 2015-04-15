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
 * Contributor(s): Blender Foundation (2015)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_asset.c
 *  \ingroup RNA
 */

#include "BLI_utildefines.h"
#include "BLI_fileops_types.h"
#include "BLI_path_util.h"

#include "DNA_space_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "BKE_asset.h"
#include "BKE_idprop.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_report.h"


/* Asset listing... */

/* Revisions. */
static int rna_AssetRevision_size_get(PointerRNA *ptr)
{
	FileDirEntryRevision *revision = ptr->data;
	return (int)revision->size;
}

static void rna_AssetRevision_size_set(PointerRNA *ptr, const int val)
{
	FileDirEntryRevision *revision = ptr->data;
	revision->size = (int64_t)val;
}

static int rna_AssetRevision_timestamp_get(PointerRNA *ptr)
{
	FileDirEntryRevision *revision = ptr->data;
	return (int)revision->time;
}

static void rna_AssetRevision_timestamp_set(PointerRNA *ptr, const int val)
{
	FileDirEntryRevision *revision = ptr->data;
	revision->time = (int64_t)val;
}

/* Variants. */
static FileDirEntryRevision *rna_AssetVariant_revisions_add(FileDirEntryVariant *variant/*, ReportList *reports,*/)
{
	FileDirEntryRevision *revision = MEM_callocN(sizeof(*revision), __func__);

	BLI_addtail(&variant->revisions, revision);
	variant->nbr_revisions++;

	return revision;
}

static PointerRNA rna_AssetVariant_active_revision_get(PointerRNA *ptr)
{
	FileDirEntryVariant *variant = ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_AssetRevision, BLI_findlink(&variant->revisions, variant->act_revision));
}

static void rna_AssetVariant_active_revision_set(PointerRNA *ptr, PointerRNA value)
{
	FileDirEntryVariant *variant = ptr->data;
	FileDirEntryRevision *revision = value.data;

	variant->act_revision = BLI_findindex(&variant->revisions, revision);
}

static void rna_AssetVariant_name_get(struct PointerRNA *ptr, char *value)
{
	FileDirEntryVariant *variant = ptr->data;
	if (variant->name) {
		strcpy(value, variant->name);
	}
	else {
		*value = '\0';
	}
}

static int rna_AssetVariant_name_length(struct PointerRNA *ptr)
{
	FileDirEntryVariant *variant = ptr->data;
	return variant->name ? strlen(variant->name) : 0;
}

static void rna_AssetVariant_name_set(struct PointerRNA *ptr, const char *value)
{
	FileDirEntryVariant *variant = ptr->data;
	MEM_SAFE_FREE(variant->name);
	if (value[0] != '\0') {
		variant->name = BLI_strdup(value);
	}
}

static void rna_AssetVariant_description_get(struct PointerRNA *ptr, char *value)
{
	FileDirEntryVariant *variant = ptr->data;
	if (variant->description) {
		strcpy(value, variant->description);
	}
	else {
		*value = '\0';
	}
}

static int rna_AssetVariant_description_length(struct PointerRNA *ptr)
{
	FileDirEntryVariant *variant = ptr->data;
	return variant->description ? strlen(variant->description) : 0;
}

static void rna_AssetVariant_description_set(struct PointerRNA *ptr, const char *value)
{
	FileDirEntryVariant *variant = ptr->data;
	MEM_SAFE_FREE(variant->description);
	if (value[0] != '\0') {
		variant->description = BLI_strdup(value);
	}
}

/* Entries. */
static PointerRNA rna_AssetEntry_active_variant_get(PointerRNA *ptr)
{
	FileDirEntry *entry = ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_AssetVariant, BLI_findlink(&entry->variants, entry->act_variant));
}

static void rna_AssetEntry_active_variant_set(PointerRNA *ptr, PointerRNA value)
{
	FileDirEntry *entry = ptr->data;
	FileDirEntryVariant *variant = value.data;

	entry->act_variant = BLI_findindex(&entry->variants, variant);
}

static FileDirEntryVariant *rna_AssetEntry_variants_add(FileDirEntry *entry/*, ReportList *reports,*/)
{
	FileDirEntryVariant *variant = MEM_callocN(sizeof(*variant), __func__);

	BLI_addtail(&entry->variants, variant);
	entry->nbr_variants++;

	return variant;
}

static void rna_AssetEntry_relpath_get(struct PointerRNA *ptr, char *value)
{
	FileDirEntry *entry = ptr->data;
	if (entry->relpath) {
		strcpy(value, entry->relpath);
	}
	else {
		*value = '\0';
	}
}

static int rna_AssetEntry_relpath_length(struct PointerRNA *ptr)
{
	FileDirEntry *entry = ptr->data;
	return entry->relpath ? strlen(entry->relpath) : 0;
}

static void rna_AssetEntry_relpath_set(struct PointerRNA *ptr, const char *value)
{
	FileDirEntry *entry = ptr->data;
	if (entry->relpath) {
		MEM_freeN(entry->relpath);
	}
	entry->relpath = BLI_strdup(value);
}

static void rna_AssetEntry_name_get(struct PointerRNA *ptr, char *value)
{
	FileDirEntry *entry = ptr->data;
	if (entry->name) {
		strcpy(value, entry->name);
	}
	else {
		*value = '\0';
	}
}

static int rna_AssetEntry_name_length(struct PointerRNA *ptr)
{
	FileDirEntry *entry = ptr->data;
	return entry->name ? strlen(entry->name) : 0;
}

static void rna_AssetEntry_name_set(struct PointerRNA *ptr, const char *value)
{
	FileDirEntry *entry = ptr->data;
	MEM_SAFE_FREE(entry->name);
	if (value[0] != '\0') {
		entry->name = BLI_strdup(value);
	}
}

static void rna_AssetEntry_description_get(struct PointerRNA *ptr, char *value)
{
	FileDirEntry *entry = ptr->data;
	if (entry->description) {
		strcpy(value, entry->description);
	}
	else {
		*value = '\0';
	}
}

static int rna_AssetEntry_description_length(struct PointerRNA *ptr)
{
	FileDirEntry *entry = ptr->data;
	return entry->description ? strlen(entry->description) : 0;
}

static void rna_AssetEntry_description_set(struct PointerRNA *ptr, const char *value)
{
	FileDirEntry *entry = ptr->data;
	MEM_SAFE_FREE(entry->description);
	if (value[0] != '\0') {
		entry->description = BLI_strdup(value);
	}
}

/* Entries Array. */
static PointerRNA rna_AssetList_active_entry_get(PointerRNA *ptr)
{
	FileDirEntryArr *arr = ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_AssetEntry, arr->entries.first /* BLI_findlink(&arr->entries, 0) */);
}

static void rna_AssetList_active_entry_set(PointerRNA *ptr, PointerRNA value)
{
	FileDirEntryArr *arr = ptr->data;
	FileDirEntry *entry = value.data;

	BLI_remlink_safe(&arr->entries, entry);
	BLI_addhead(&arr->entries, entry);
}

static int rna_AssetList_active_entry_index_get(PointerRNA *UNUSED(ptr))
{
	return 0;
}

static FileDirEntry *rna_AssetList_entries_add(FileDirEntryArr *dirlist)
{
	FileDirEntry *entry = MEM_callocN(sizeof(*entry), __func__);

	BLI_addtail(&dirlist->entries, entry);

	return entry;
}

static void rna_AssetList_entries_remove(FileDirEntryArr *dirlist, ReportList *reports, PointerRNA *ptr)
{
	FileDirEntry *entry = ptr->data;

	if (!BLI_remlink_safe(&dirlist->entries, entry)) {
		BKE_report(reports, RPT_ERROR, "Trying to remove an entry for a list which does not contain it!");
		return;
	}

	BKE_filedir_entry_free(entry);
}

static void rna_AssetList_entries_clear(FileDirEntryArr *dirlist)
{
	BKE_filedir_entryarr_clear(dirlist);
}

/* AssetEngine callbacks. */

static int rna_ae_status(AssetEngine *engine, const int id)
{
	extern FunctionRNA rna_AssetEngine_status_func;
	PointerRNA ptr;
	PropertyRNA *parm;
	ParameterList list;
	FunctionRNA *func;

	void *ret;
	int ret_status;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_AssetEngine_status_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "job_id", &id);
	engine->type->ext.call(NULL, &ptr, func, &list);

	parm = RNA_function_find_parameter(NULL, func, "status_return");
	RNA_parameter_get(&list, parm, &ret);
	ret_status = *(int *)ret;

	RNA_parameter_list_free(&list);

	return ret_status;
}

static float rna_ae_progress(AssetEngine *engine, const int id)
{
	extern FunctionRNA rna_AssetEngine_progress_func;
	PointerRNA ptr;
	PropertyRNA *parm;
	ParameterList list;
	FunctionRNA *func;

	void *ret;
	float ret_progress;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_AssetEngine_progress_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "job_id", &id);
	engine->type->ext.call(NULL, &ptr, func, &list);

	parm = RNA_function_find_parameter(NULL, func, "progress_return");
	RNA_parameter_get(&list, parm, &ret);
	ret_progress = *(float *)ret;

	RNA_parameter_list_free(&list);

	return ret_progress;
}

static void rna_ae_kill(AssetEngine *engine, const int id)
{
	extern FunctionRNA rna_AssetEngine_kill_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_AssetEngine_kill_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "job_id", &id);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static int rna_ae_list_dir(AssetEngine *engine, const int id, FileDirEntryArr *entries_r)
{
	extern FunctionRNA rna_AssetEngine_list_dir_func;
	PointerRNA ptr;
	PropertyRNA *parm;
	ParameterList list;
	FunctionRNA *func;

	void *ret;
	int ret_job_id;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_AssetEngine_list_dir_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "job_id", &id);
	RNA_parameter_set_lookup(&list, "entries", &entries_r);
	engine->type->ext.call(NULL, &ptr, func, &list);

	parm = RNA_function_find_parameter(NULL, func, "job_id_return");
	RNA_parameter_get(&list, parm, &ret);
	ret_job_id = *(int *)ret;

	RNA_parameter_list_free(&list);

	return ret_job_id;
}

static bool rna_ae_load_pre(AssetEngine *engine, AssetUUIDList *uuids, struct FileDirEntryArr *entries_r)
{
	extern FunctionRNA rna_AssetEngine_load_pre_func;
	PointerRNA ptr;
	PropertyRNA *parm;
	ParameterList list;
	FunctionRNA *func;

	void *ret;
	bool ret_success;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_AssetEngine_load_pre_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "uuids", &uuids);
	RNA_parameter_set_lookup(&list, "entries", &entries_r);
	engine->type->ext.call(NULL, &ptr, func, &list);

	parm = RNA_function_find_parameter(NULL, func, "success_return");
	RNA_parameter_get(&list, parm, &ret);
	ret_success = (bool)*(int *)ret;

	RNA_parameter_list_free(&list);

	return ret_success;
}

static bool rna_ae_sort_filter(
        AssetEngine *engine, const bool use_sort, const bool use_filter,
        FileSelectParams *params, FileDirEntryArr *entries_r)
{
	extern FunctionRNA rna_AssetEngine_sort_filter_func;
	PointerRNA ptr;
	PropertyRNA *parm;
	ParameterList list;
	FunctionRNA *func;

	void *ret;
	bool ret_changed;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_AssetEngine_sort_filter_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "use_sort", &use_sort);
	RNA_parameter_set_lookup(&list, "use_filter", &use_filter);
	RNA_parameter_set_lookup(&list, "params", &params);
	RNA_parameter_set_lookup(&list, "entries", &entries_r);
	engine->type->ext.call(NULL, &ptr, func, &list);

	parm = RNA_function_find_parameter(NULL, func, "changed_return");
	RNA_parameter_get(&list, parm, &ret);
	ret_changed= *(bool *)ret;

	RNA_parameter_list_free(&list);

	return ret_changed;
}

static bool rna_ae_entries_block_get(
        AssetEngine *engine, const int start_index, const int end_index, FileDirEntryArr *entries_r)
{
	extern FunctionRNA rna_AssetEngine_entries_block_get_func;
	PointerRNA ptr;
	PropertyRNA *parm;
	ParameterList list;
	FunctionRNA *func;

	void *ret;
	bool ret_success;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_AssetEngine_entries_block_get_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "start_index", &start_index);
	RNA_parameter_set_lookup(&list, "end_index", &end_index);
	RNA_parameter_set_lookup(&list, "entries", &entries_r);
	engine->type->ext.call(NULL, &ptr, func, &list);

	parm = RNA_function_find_parameter(NULL, func, "success_return");
	RNA_parameter_get(&list, parm, &ret);
	ret_success= *(bool *)ret;

	RNA_parameter_list_free(&list);

	return ret_success;
}

static bool rna_ae_entries_uuid_get(
        AssetEngine *engine, AssetUUIDList *uuids, FileDirEntryArr *entries_r)
{
	extern FunctionRNA rna_AssetEngine_entries_uuid_get_func;
	PointerRNA ptr;
	PropertyRNA *parm;
	ParameterList list;
	FunctionRNA *func;

	void *ret;
	bool ret_success;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_AssetEngine_entries_uuid_get_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "uuids", &uuids);
	RNA_parameter_set_lookup(&list, "entries", &entries_r);
	engine->type->ext.call(NULL, &ptr, func, &list);

	parm = RNA_function_find_parameter(NULL, func, "success_return");
	RNA_parameter_get(&list, parm, &ret);
	ret_success= *(bool *)ret;

	RNA_parameter_list_free(&list);

	return ret_success;
}


/* AssetEngine registration */

static void rna_AssetEngine_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	AssetEngineType *aet = RNA_struct_blender_type_get(type);

	if (!aet) {
		return;
	}
	
	RNA_struct_free_extension(type, &aet->ext);
	BLI_freelinkN(&asset_engines, aet);
	RNA_struct_free(&BLENDER_RNA, type);
}

static StructRNA *rna_AssetEngine_register(Main *bmain, ReportList *reports, void *data, const char *identifier,
                                           StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	AssetEngineType *aet, dummyaet = {NULL};
	AssetEngine dummyengine = {NULL};
	PointerRNA dummyptr;
	int have_function[8];

	/* setup dummy engine & engine type to store static properties in */
	dummyengine.type = &dummyaet;
	RNA_pointer_create(NULL, &RNA_AssetEngine, &dummyengine, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0) {
		return NULL;
	}

	if (strlen(identifier) >= sizeof(dummyaet.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering asset engine class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummyaet.idname));
		return NULL;
	}

	/* Check if we have registered this engine type before, and remove it. */
	aet = BLI_rfindstring(&asset_engines, dummyaet.idname, offsetof(AssetEngineType, idname));
	if (aet && aet->ext.srna) {
		rna_AssetEngine_unregister(bmain, aet->ext.srna);
	}
	
	/* Create a new engine type. */
	aet = MEM_callocN(sizeof(AssetEngineType), __func__);
	memcpy(aet, &dummyaet, sizeof(*aet));

	aet->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, aet->idname, &RNA_AssetEngine);
	aet->ext.data = data;
	aet->ext.call = call;
	aet->ext.free = free;
	RNA_struct_blender_type_set(aet->ext.srna, aet);

	aet->status = (have_function[0]) ? rna_ae_status : NULL;
	aet->progress = (have_function[1]) ? rna_ae_progress : NULL;
	aet->kill = (have_function[2]) ? rna_ae_kill : NULL;

	aet->list_dir = (have_function[3]) ? rna_ae_list_dir : NULL;

	aet->load_pre = (have_function[4]) ? rna_ae_load_pre : NULL;

	aet->sort_filter = (have_function[5]) ? rna_ae_sort_filter : NULL;
	aet->entries_block_get = (have_function[6]) ? rna_ae_entries_block_get : NULL;
	aet->entries_uuid_get = (have_function[7]) ? rna_ae_entries_uuid_get : NULL;

	BLI_addtail(&asset_engines, aet);

	return aet->ext.srna;
}

static void **rna_AssetEngine_instance(PointerRNA *ptr)
{
	AssetEngine *engine = ptr->data;
	return &engine->py_instance;
}

static StructRNA *rna_AssetEngine_refine(PointerRNA *ptr)
{
	AssetEngine *engine = ptr->data;
	return (engine->type && engine->type->ext.srna) ? engine->type->ext.srna : &RNA_AssetEngine;
}

static IDProperty *rna_AssetEngine_idprops(PointerRNA *ptr, bool create)
{
	AssetEngine *ae = (AssetEngine *)ptr->data;
	if (create && !ae->properties) {
		IDPropertyTemplate val = {0};
		ae->properties = IDP_New(IDP_GROUP, &val, "RNA_AssetEngine IDproperties group");
	}

	return ae->properties;
}

#else /* RNA_RUNTIME */

/* Much lighter version of asset/variant/revision identifier. */
static void rna_def_asset_uuid(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	int null_uuid[4] = {0};

	srna = RNA_def_struct(brna, "AssetUUID", NULL);
	RNA_def_struct_sdna(srna, "AssetUUID");
	RNA_def_struct_ui_text(srna, "Asset UUID", "A unique identifier of an asset (asset engine dependent!)");

	prop = RNA_def_int_vector(srna, "uuid_asset", 4, null_uuid, INT_MIN, INT_MAX,
	                          "Asset UUID", "Unique identifier of this asset", INT_MIN, INT_MAX);
//	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
//	RNA_def_property_int_funcs(prop, "rna_AssetUUID_uuid_asset_get", NULL, NULL);

	prop = RNA_def_int_vector(srna, "uuid_variant", 4, null_uuid, INT_MIN, INT_MAX,
	                          "Variant UUID", "Unique identifier of this asset's variant", INT_MIN, INT_MAX);
//	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
//	RNA_def_property_int_funcs(prop, "rna_AssetUUID_uuid_variant_get", NULL, NULL);

	prop = RNA_def_int_vector(srna, "uuid_revision", 4, null_uuid, INT_MIN, INT_MAX,
	                          "Revision UUID", "Unique identifier of this asset's revision", INT_MIN, INT_MAX);
//	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
//	RNA_def_property_int_funcs(prop, "rna_AssetUUID_uuid_revision_get", NULL, NULL);
}

static void rna_def_asset_uuid_list(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "AssetUUIDList", NULL);
	RNA_def_struct_sdna(srna, "AssetUUIDList");
	RNA_def_struct_ui_text(srna, "Asset UUIDs List", "Collection of assets uuids");

	prop = RNA_def_property(srna, "uuids", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "uuids", "nbr_uuids");
	RNA_def_property_struct_type(prop, "AssetUUID");
	RNA_def_property_ui_text(prop, "UUIDs", "Collection of asset UUIDs");

	rna_def_asset_uuid(brna);
}

static void rna_def_asset_revision(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
//	FunctionRNA *func;

	int null_uuid[4] = {0};

	srna = RNA_def_struct(brna, "AssetRevision", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntryRevision");
	RNA_def_struct_ui_text(srna, "Asset Entry Revision", "A revision of a single asset item");
//	RNA_def_struct_ui_icon(srna, ICON_NONE);  /* XXX TODO */

	prop = RNA_def_int_vector(srna, "uuid", 4, null_uuid, INT_MIN, INT_MAX, "Revision UUID",
	                          "Unique identifier of this revision (actual content depends on asset engine)",
	                          INT_MIN, INT_MAX);
//	prop = RNA_def_property(srna, "uuid", PROP_STRING, PROP_BYTESTRING);
//	RNA_def_property_ui_text(prop, "Revision UUID",
//	                         "Unique identifier of this revision (actual content depends on asset engine)");

	prop = RNA_def_int(srna, "size", 0, -1, INT_MAX, "Size",
	                   "Size (in bytes, special value '-1' means 'no size')", -1, INT_MAX);
	RNA_def_property_int_funcs(prop, "rna_AssetRevision_size_get", "rna_AssetRevision_size_set", NULL);

	prop = RNA_def_int(srna, "timestamp", 0, 0, INT_MAX, "Timestamp", "In seconds since the epoch", 0, INT_MAX);
	RNA_def_property_int_funcs(prop, "rna_AssetRevision_timestamp_get", "rna_AssetRevision_timestamp_set", NULL);
}

/* assetvariant.revisions */
static void rna_def_asset_revisions(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "AssetRevisions");
	srna = RNA_def_struct(brna, "AssetRevisions", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntryVariant");
	RNA_def_struct_ui_text(srna, "Asset Entry Revisions", "Collection of asset entry's revisions");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "AssetRevision");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_AssetVariant_active_revision_get",
	                               "rna_AssetVariant_active_revision_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Revision", "Active (selected) revision of the asset");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "act_revision");
	RNA_def_property_ui_text(prop, "Active Index", "Index of asset's revision curently active (selected)");

	/* Add Revision */
	func = RNA_def_function(srna, "add", "rna_AssetVariant_revisions_add");
	RNA_def_function_ui_description(func, "Add a new revision to the entry's variant");
//	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* return arg */
	parm = RNA_def_pointer(func, "revision", "AssetRevision", "New Revision", "New asset entry variant revision");
	RNA_def_function_return(func, parm);
}

static void rna_def_asset_variant(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
//	FunctionRNA *func;

	int null_uuid[4] = {0};

	srna = RNA_def_struct(brna, "AssetVariant", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntryVariant");
	RNA_def_struct_ui_text(srna, "Asset Entry Variant",
	                       "A variant of a single asset item (e.g. high-poly, low-poly, etc.)");
//	RNA_def_struct_ui_icon(srna, ICON_NONE);  /* XXX TODO */

	prop = RNA_def_int_vector(srna, "uuid", 4, null_uuid, INT_MIN, INT_MAX, "Variant UUID",
	                          "Unique identifier of this revision (actual content depends on asset engine)",
	                          INT_MIN, INT_MAX);
//	prop = RNA_def_property(srna, "uuid", PROP_STRING, PROP_BYTESTRING);
//	RNA_def_property_ui_text(prop, "Variant UUID",
//	                         "Unique identifier of this variant (actual content depends on asset engine)");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_AssetVariant_name_get", "rna_AssetVariant_name_length",
	                              "rna_AssetVariant_name_set");
	RNA_def_property_ui_text(prop, "Name", "");

	prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_AssetVariant_description_get", "rna_AssetVariant_description_length",
	                              "rna_AssetVariant_description_set");
	RNA_def_property_ui_text(prop, "Description", "");

	prop = RNA_def_property(srna, "revisions", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "AssetRevision");
	RNA_def_property_ui_text(prop, "Revisions", "Collection of asset variant's revisions");
	rna_def_asset_revision(brna);
	rna_def_asset_revisions(brna, prop);
}

/* assetentry.variants */
static void rna_def_asset_variants(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "AssetVariants");
	srna = RNA_def_struct(brna, "AssetVariants", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntry");
	RNA_def_struct_ui_text(srna, "Asset Entry Variants", "Collection of asset entry's variants");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "AssetVariant");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_AssetEntry_active_variant_get",
	                               "rna_AssetEntry_active_variant_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Variant", "Active (selected) variant of the asset");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "act_variant");
	RNA_def_property_ui_text(prop, "Active Index", "Index of asset's variant curently active (selected)");

	/* Add Variant */
	func = RNA_def_function(srna, "add", "rna_AssetEntry_variants_add");
	RNA_def_function_ui_description(func, "Add a new variant to the entry");
//	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* return arg */
	parm = RNA_def_pointer(func, "variant", "AssetVariant", "New Variant", "New asset entry variant");
	RNA_def_function_return(func, parm);
}

static void rna_def_asset_entry(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
//	FunctionRNA *func;

	static EnumPropertyItem asset_revision_types[] = {
	    {FILE_TYPE_BLENDER, "BLENDER", 0, "Blender File", ""},
//	    {FILE_TYPE_BLENDER_BACKUP, "", 0, "", ""},
	    {FILE_TYPE_IMAGE, "IMAGE", 0, "Image", ""},
	    {FILE_TYPE_MOVIE, "MOVIE", 0, "Movie", ""},
	    {FILE_TYPE_PYSCRIPT, "PYSCRIPT", 0, "Python Script", ""},
	    {FILE_TYPE_FTFONT, "FONT", 0, "Font", ""},
	    {FILE_TYPE_SOUND, "SOUND", 0, "Sound", ""},
	    {FILE_TYPE_TEXT, "TEXT", 0, "Text", ""},
//	    {FILE_TYPE_MOVIE_ICON, "", 0, "", ""},
//	    {FILE_TYPE_FOLDER, "", 0, "", ""},
//	    {FILE_TYPE_BTX, "", 0, "", ""},
//	    {FILE_TYPE_COLLADA, "", 0, "", ""},
//	    {FILE_TYPE_OPERATOR, "", 0, "", ""},
//	    {FILE_TYPE_APPLICATIONBUNDLE, "", 0, "", ""},
	    {FILE_TYPE_DIR, "DIR", 0, "Directory", "An entry that can be used as 'root' path too"},
	    {FILE_TYPE_BLENDERLIB, "BLENLIB", 0, "Blender Library", "An entry that is part of a .blend file"},
	    {0, NULL, 0, NULL, NULL}
	};

	int null_uuid[4] = {0};

	srna = RNA_def_struct(brna, "AssetEntry", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntry");
	RNA_def_struct_ui_text(srna, "Asset Entry", "A single asset item (quite similar to a file path)");
//	RNA_def_struct_ui_icon(srna, ICON_NONE);  /* XXX TODO */

	prop = RNA_def_int_vector(srna, "uuid", 4, null_uuid, INT_MIN, INT_MAX, "Variant UUID",
	                          "Unique identifier of this entry (actual content depends on asset engine)",
	                          INT_MIN, INT_MAX);
//	prop = RNA_def_property(srna, "uuid", PROP_STRING, PROP_BYTESTRING);
//	RNA_def_property_ui_text(prop, "Entry UUID",
//	                         "Unique identifier of this revision (actual content depends on asset engine)");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_AssetEntry_name_get", "rna_AssetEntry_name_length",
	                              "rna_AssetEntry_name_set");
	RNA_def_property_ui_text(prop, "Name", "");

	prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_AssetEntry_description_get", "rna_AssetEntry_description_length",
	                              "rna_AssetEntry_description_set");
	RNA_def_property_ui_text(prop, "Description", "");

	prop = RNA_def_property(srna, "relpath", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_AssetEntry_relpath_get", "rna_AssetEntry_relpath_length",
	                              "rna_AssetEntry_relpath_set");
	RNA_def_property_ui_text(prop, "Relative Path", "Relative AssetList's root_path");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "typeflag");
	RNA_def_property_enum_items(prop, asset_revision_types);

	prop = RNA_def_property(srna, "blender_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blentype");
	RNA_def_property_enum_items(prop, id_type_items);

	prop = RNA_def_property(srna, "variants", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "AssetVariant");
	RNA_def_property_ui_text(prop, "Variants", "Collection of asset variants");
	rna_def_asset_variant(brna);
	rna_def_asset_variants(brna, prop);

	/* TODO: image (i.e. preview)? */

	/* TODO tags, status */
}

/* assetlist.entries */
static void rna_def_asset_entries(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm, *prop;

	RNA_def_property_srna(cprop, "AssetEntries");
	srna = RNA_def_struct(brna, "AssetEntries", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntryArr");
	RNA_def_struct_ui_text(srna, "Asset List entries", "Collection of asset entries");

	/* Currently, 'active' entry (i.e. the one passed to single-file arg of operators) is always the
	 * first of the list... */
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "AssetEntry");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_AssetList_active_entry_get",
	                               "rna_AssetList_active_entry_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Entry", "Active (selected) entry of the list");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_AssetList_active_entry_index_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Index", "Index of entry curently active (selected)");

	/* Add Entry */
	func = RNA_def_function(srna, "add", "rna_AssetList_entries_add");
	RNA_def_function_ui_description(func, "Add a new asset entry to the list");
	/* return arg */
	parm = RNA_def_pointer(func, "entry", "AssetEntry", "New Entry", "New asset entry");
	RNA_def_function_return(func, parm);

	/* Remove Entry */
	func = RNA_def_function(srna, "remove", "rna_AssetList_entries_remove");
	RNA_def_function_ui_description(func, "Remove the given entry from the list (entry is freeded)");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "entry", "AssetEntry", "Entry", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

	/* Remove All Entries */
	func = RNA_def_function(srna, "clear", "rna_AssetList_entries_clear");
	RNA_def_function_ui_description(func, "Remove all entries from the list");
}

static void rna_def_asset_list(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
//	FunctionRNA *func;

	srna = RNA_def_struct(brna, "AssetList", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntryArr");
	RNA_def_struct_ui_text(srna, "Asset List", "List of assets (quite similar to a file list)");
//	RNA_def_struct_ui_icon(srna, ICON_NONE);  /* XXX TODO */

	prop = RNA_def_property(srna, "entries", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "AssetEntry");
	RNA_def_property_ui_text(prop, "Entries", "Collection of asset entries");
	rna_def_asset_entry(brna);
	rna_def_asset_entries(brna, prop);

	prop = RNA_def_int(srna, "nbr_entries", 0, 0, INT_MAX, "Entries Number",
	                   "Total number of available entries/assets, *not the length of 'entries'!*", 0, INT_MAX);
	RNA_def_property_int_sdna(prop, NULL, "nbr_entries");

	prop = RNA_def_int(srna, "nbr_entries_filtered", 0, 0, INT_MAX, "Filtered Entries Number",
	                   "Total number of visible entries/assets, *not the length of 'entries'!*", 0, INT_MAX);
	RNA_def_property_int_sdna(prop, NULL, "nbr_entries_filtered");

	prop = RNA_def_property(srna, "root_path", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "root");
	RNA_def_property_ui_text(prop, "Root Path", "Root directory from which all asset entries come from");
}

static void rna_def_asset_engine(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop, *parm;
	FunctionRNA *func;

	static EnumPropertyItem asset_engine_status_types[] = {
	    {AE_STATUS_VALID, "VALID", 0, "Valid", ""},
	    {AE_STATUS_RUNNING, "RUNNING", 0, "Running", ""},
	    {0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "AssetEngine", NULL);
	RNA_def_struct_sdna(srna, "AssetEngine");
	RNA_def_struct_ui_text(srna, "Asset Engine", "An assets manager");
	RNA_def_struct_refine_func(srna, "rna_AssetEngine_refine");
	RNA_def_struct_register_funcs(srna, "rna_AssetEngine_register", "rna_AssetEngine_unregister",
	                              "rna_AssetEngine_instance");
	RNA_def_struct_idprops_func(srna, "rna_AssetEngine_idprops");

	/* Status callback */
	func = RNA_def_function(srna, "status", NULL);
	RNA_def_function_ui_description(func, "Get status of whole engine, or a given job");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_int(func, "job_id", 0, 0, INT_MAX, "", "Job ID (zero to get engine status itself)", 0, INT_MAX);
	parm = RNA_def_enum(func, "status_return", asset_engine_status_types, 0, "", "Status of given job or whole engine");
	RNA_def_property_flag(parm, PROP_ENUM_FLAG);
	RNA_def_function_output(func, parm);

	/* Progress callback */
	func = RNA_def_function(srna, "progress", NULL);
	RNA_def_function_ui_description(func, "Get progress of a given job, or all running ones (between 0.0 and 1.0)");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_int(func, "job_id", 0, 0, INT_MAX, "",
	            "Job ID (zero to get average progress of all running jobs)", 0, INT_MAX);
	parm = RNA_def_float(func, "progress_return", 0.0f, 0.0f, 1.0f, "", "Progress", 0.0f, 1.0f);
	RNA_def_function_output(func, parm);

	/* Kill job callback */
	func = RNA_def_function(srna, "kill", NULL);
	RNA_def_function_ui_description(func, "Unconditionnaly stop a given job, or all running ones");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_int(func, "job_id", 0, 0, INT_MAX, "", "Job ID (zero to kill all)", 0, INT_MAX);

	/* Main listing callback */
	func = RNA_def_function(srna, "list_dir", NULL);
	RNA_def_function_ui_description(func, "Start/update the list of available entries (assets)");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_int(func, "job_id", 0, 0, INT_MAX, "", "Job ID (zero to start a new one)", 0, INT_MAX);
	RNA_def_pointer(func, "entries", "AssetList", "", "List of asset entries proposed to user by the asset engine");
	parm = RNA_def_int(func, "job_id_return", 0, 0, INT_MAX, "", "Job ID", 0, INT_MAX);
	RNA_def_function_output(func, parm);

	/* Pre-load callback */
	func = RNA_def_function(srna, "load_pre", NULL);
	RNA_def_function_ui_description(func, "Pre-process given assets to make them loadable by Blender");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_pointer(func, "uuids", "AssetUUIDList", "", "Identifiers of assets to 'make real'");
	RNA_def_pointer(func, "entries", "AssetList", "", "List of actual, existing paths that Blender can load");
	parm = RNA_def_boolean(func, "success_return", 0, "", "Success");
	RNA_def_function_output(func, parm);

	/* Sorting/filtering callback */
	func = RNA_def_function(srna, "sort_filter", NULL);
	RNA_def_function_ui_description(func, "Sort and/or filter the assets (on engine's side)");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_boolean(func, "use_sort", 0, "", "Whether to (re-)sort assets");
	RNA_def_boolean(func, "use_filter", 0, "", "Whether to (re-)filter assets");
	parm = RNA_def_pointer(func, "params", "FileSelectParams", "",
	                       "Generic filtering/sorting parameters from FileBrowser");
	RNA_def_pointer(func, "entries", "AssetList", "", "List of asset entries proposed to user by the asset engine");
	parm = RNA_def_boolean(func, "changed_return", 0, "", "Whether list of available entries was changed");
	RNA_def_function_output(func, parm);

	/* Block of entries by-index getter callback */
	func = RNA_def_function(srna, "entries_block_get", NULL);
	RNA_def_function_ui_description(func, "Get a block of entries/assets by its (sorted/filtered) start/end index");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_int(func, "start_index", 0, 0, INT_MAX, "", "Index of first entry (asset) to get (included)", 0, INT_MAX);
	RNA_def_int(func, "end_index", 0, 0, INT_MAX, "", "Index of last entry (asset) to get (excluded)", 0, INT_MAX);
	RNA_def_pointer(func, "entries", "AssetList", "", "List of asset entries proposed to user by the asset engine");
	parm = RNA_def_boolean(func, "success_return", 0, "", "Success");
	RNA_def_function_output(func, parm);

	/* Set of entries by-uuids getter callback */
	func = RNA_def_function(srna, "entries_uuid_get", NULL);
	RNA_def_function_ui_description(func, "Get a set of entries/assets by their uuids start/end index");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_pointer(func, "uuids", "AssetUUIDList", "", "Identifiers of assets to 'make real'");
	RNA_def_pointer(func, "entries", "AssetList", "", "List of asset entries matching given uuids");
	parm = RNA_def_boolean(func, "success_return", 0, "", "Success");
	RNA_def_function_output(func, parm);

	RNA_define_verify_sdna(false);

	/* registration */

	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->name");
	RNA_def_property_flag(prop, PROP_REGISTER);

	RNA_define_verify_sdna(true);
}

void RNA_def_asset(BlenderRNA *brna)
{
	rna_def_asset_engine(brna);
	rna_def_asset_uuid_list(brna);
	rna_def_asset_list(brna);
}

#endif /* RNA_RUNTIME */
