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

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_report.h"


/* Asset listing... */

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
	if (variant->name) {
		MEM_freeN(variant->name);
	}
	variant->name = BLI_strdup(value);
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
	if (variant->description) {
		MEM_freeN(variant->description);
	}
	variant->description = BLI_strdup(value);
}

static FileDirEntryVariant *rna_AssetEntry_variants_add(FileDirEntry *entry/*, ReportList *reports,*/)
{
	FileDirEntryVariant *variant = MEM_callocN(sizeof(*variant), __func__);

	BLI_addtail(&entry->variants, variant);
	entry->nbr_variants++;

	return variant;
}

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

static FileDirEntry *rna_AssetList_entries_add(FileDirEntryArr *dirlist/*, ReportList *reports,*/)
{
	FileDirEntry *entry = MEM_callocN(sizeof(*entry), __func__);

	BLI_addtail(&dirlist->entries, entry);
	dirlist->nbr_entries++;

	return entry;
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
	int have_function[4];

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

#else /* RNA_RUNTIME */

static void rna_def_asset_revision(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
//	FunctionRNA *func;

	srna = RNA_def_struct(brna, "AssetRevision", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntryRevision");
	RNA_def_struct_ui_text(srna, "Asset Entry Revision", "A revision of a single asset item");
//	RNA_def_struct_ui_icon(srna, ICON_NONE);  /* XXX TODO */

	prop = RNA_def_property(srna, "uuid", PROP_STRING, PROP_BYTESTRING);
	RNA_def_property_ui_text(prop, "Revision UUID",
	                         "Unique identifier of this revision (actual content depends on asset engine)");

	/* TODO: size, time, etc. */
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

	/* Add Revision */
	func = RNA_def_function(srna, "add", "rna_AssetVariant_revisions_add");
	RNA_def_function_ui_description(func, "Add a new revision to the entry's variant");
//	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* return arg */
	parm = RNA_def_pointer(func, "revision", "AssetRevision", "New Revision", "New asset entry variant revision");
	RNA_def_function_return(func, parm);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "AssetRevision");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_AssetVariant_active_revision_get",
	                               "rna_AssetVariant_active_revision_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Revision", "Active (selected) revision of the asset");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "act_revision");
	RNA_def_property_ui_text(prop, "Active Index", "Index of asset's revision curently active (selected)");
}

static void rna_def_asset_variant(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
//	FunctionRNA *func;

	srna = RNA_def_struct(brna, "AssetVariant", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntryVariant");
	RNA_def_struct_ui_text(srna, "Asset Entry Variant", "A variant of a single asset item (e.g. high-poly, low-poly, etc.)");
//	RNA_def_struct_ui_icon(srna, ICON_NONE);  /* XXX TODO */

	prop = RNA_def_property(srna, "revisions", PROP_COLLECTION, PROP_NONE);
//	RNA_def_property_collection_sdna(prop, NULL, "revisions", "nbr_revisions");
	RNA_def_property_struct_type(prop, "AssetRevision");
	RNA_def_property_ui_text(prop, "Revisions", "Collection of asset variant's revisions");
	rna_def_asset_revision(brna);
	rna_def_asset_revisions(brna, prop);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_AssetVariant_name_get", "rna_AssetVariant_name_length",
	                              "rna_AssetVariant_name_set");
	RNA_def_property_ui_text(prop, "Name", "");

	prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_AssetVariant_description_get", "rna_AssetVariant_description_length",
	                              "rna_AssetVariant_description_set");
	RNA_def_property_ui_text(prop, "Description", "");
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

	/* Add Variant */
	func = RNA_def_function(srna, "add", "rna_AssetEntry_variants_add");
	RNA_def_function_ui_description(func, "Add a new variant to the entry");
//	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* return arg */
	parm = RNA_def_pointer(func, "variant", "AssetVariant", "New Variant", "New asset entry variant");
	RNA_def_function_return(func, parm);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "AssetVariant");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_AssetEntry_active_variant_get",
	                               "rna_AssetEntry_active_variant_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Variant", "Active (selected) variant of the asset");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "act_variant");
	RNA_def_property_ui_text(prop, "Active Index", "Index of asset's variant curently active (selected)");
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

	srna = RNA_def_struct(brna, "AssetEntry", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntry");
	RNA_def_struct_ui_text(srna, "Asset Entry", "A single asset item (quite similar to a file path)");
//	RNA_def_struct_ui_icon(srna, ICON_NONE);  /* XXX TODO */

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
//	RNA_def_property_collection_sdna(prop, NULL, "variants", "nbr_variants");
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
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "AssetEntries");
	srna = RNA_def_struct(brna, "AssetEntries", NULL);
	RNA_def_struct_sdna(srna, "FileDirEntryArr");
	RNA_def_struct_ui_text(srna, "Asset List entries", "Collection of asset entries");

	/* Add Entry */
	func = RNA_def_function(srna, "add", "rna_AssetList_entries_add");
	RNA_def_function_ui_description(func, "Add a new asset entry to the list");
//	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* return arg */
	parm = RNA_def_pointer(func, "entry", "AssetEntry", "New Entry", "New asset entry");
	RNA_def_function_return(func, parm);

#if 0
	/* Remove Path */
	func = RNA_def_function(srna, "remove", "rna_KeyingSet_paths_remove");
	RNA_def_function_ui_description(func, "Remove the given path from the Keying Set");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* path to remove */
	parm = RNA_def_pointer(func, "path", "KeyingSetPath", "Path", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);


	/* Remove All Paths */
	func = RNA_def_function(srna, "clear", "rna_KeyingSet_paths_clear");
	RNA_def_function_ui_description(func, "Remove all the paths from the Keying Set");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyingSetPath");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_editable_func(prop, "rna_KeyingSet_active_ksPath_editable");
	RNA_def_property_pointer_funcs(prop, "rna_KeyingSet_active_ksPath_get",
	                               "rna_KeyingSet_active_ksPath_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "active_path");
	RNA_def_property_int_funcs(prop, "rna_KeyingSet_active_ksPath_index_get", "rna_KeyingSet_active_ksPath_index_set",
	                           "rna_KeyingSet_active_ksPath_index_range");
	RNA_def_property_ui_text(prop, "Active Path Index", "Current Keying Set index");
#endif
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
//	RNA_def_property_collection_sdna(prop, NULL, "entries", "nbr_entries");
	RNA_def_property_struct_type(prop, "AssetEntry");
	RNA_def_property_ui_text(prop, "Entries", "Collection of asset entries");
	rna_def_asset_entry(brna);
	rna_def_asset_entries(brna, prop);

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
	RNA_def_int(func, "job_id", 0, 0, INT_MAX, "", "Job ID (zero to get average progress of all running jobs)", 0, INT_MAX);
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
	RNA_def_pointer(func, "entries", "AssetList", "", "");
	parm = RNA_def_int(func, "job_id_return", 0, 0, INT_MAX, "", "Job ID", 0, INT_MAX);
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
	rna_def_asset_list(brna);
}

#endif /* RNA_RUNTIME */
