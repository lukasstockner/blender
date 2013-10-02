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
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_pointcache.c
 *  \ingroup RNA
 */

#include "DNA_object_force.h"
#include "DNA_scene_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "PTC_api.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BLI_math_base.h"

#include "DNA_object_types.h"

#include "BKE_depsgraph.h"
#include "BKE_pointcache.h"

static void rna_Cache_change(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointCache *cache = (PointCache *)ptr->data;
	PTCacheID *pid = NULL;
	ListBase pidlist;

	if (!ob)
		return;

	cache->flag |= PTCACHE_OUTDATED;

	BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	for (pid = pidlist.first; pid; pid = pid->next) {
		if (pid->cache == cache)
			break;
	}

	if (pid) {
		/* Just make sure this wasn't changed. */
		if (pid->type == PTCACHE_TYPE_SMOKE_DOMAIN)
			cache->step = 1;
		BKE_ptcache_update_info(pid);
	}

	BLI_freelistN(&pidlist);
}

static void rna_Cache_toggle_disk_cache(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointCache *cache = (PointCache *)ptr->data;
	PTCacheID *pid = NULL;
	ListBase pidlist;

	if (!ob)
		return;

	BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);

	for (pid = pidlist.first; pid; pid = pid->next) {
		if (pid->cache == cache)
			break;
	}

	/* smoke can only use disk cache */
	if (pid && pid->type != PTCACHE_TYPE_SMOKE_DOMAIN)
		BKE_ptcache_toggle_disk_cache(pid);
	else
		cache->flag ^= PTCACHE_DISK_CACHE;

	BLI_freelistN(&pidlist);
}

static void rna_Cache_idname_change(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointCache *cache = (PointCache *)ptr->data;
	PTCacheID *pid = NULL, *pid2 = NULL;
	ListBase pidlist;
	int new_name = 1;

	if (!ob)
		return;

	/* TODO: check for proper characters */

	BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);

	if (cache->flag & PTCACHE_EXTERNAL) {
		for (pid = pidlist.first; pid; pid = pid->next) {
			if (pid->cache == cache)
				break;
		}

		if (!pid)
			return;

		BKE_ptcache_load_external(pid);

		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_main_add_notifier(NC_OBJECT | ND_POINTCACHE, ob);
	}
	else {
		for (pid = pidlist.first; pid; pid = pid->next) {
			if (pid->cache == cache)
				pid2 = pid;
			else if (cache->name[0] != '\0' && strcmp(cache->name, pid->cache->name) == 0) {
				/*TODO: report "name exists" to user */
				BLI_strncpy(cache->name, cache->prev_name, sizeof(cache->name));
				new_name = 0;
			}
		}

		if (new_name) {
			if (pid2 && cache->flag & PTCACHE_DISK_CACHE) {
				char old_name[80];
				char new_name[80];

				BLI_strncpy(old_name, cache->prev_name, sizeof(old_name));
				BLI_strncpy(new_name, cache->name, sizeof(new_name));

				BKE_ptcache_disk_cache_rename(pid2, old_name, new_name);
			}

			BLI_strncpy(cache->prev_name, cache->name, sizeof(cache->prev_name));
		}
	}

	BLI_freelistN(&pidlist);
}

static void rna_Cache_list_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	PointCache *cache = ptr->data;
	ListBase lb;

	while (cache->prev)
		cache = cache->prev;

	lb.first = cache;
	lb.last = NULL; /* not used by listbase_begin */

	rna_iterator_listbase_begin(iter, &lb, NULL);
}
static void rna_Cache_active_point_cache_index_range(PointerRNA *ptr, int *min, int *max,
                                                     int *UNUSED(softmin), int *UNUSED(softmax))
{
	Object *ob = ptr->id.data;
	PointCache *cache = ptr->data;
	PTCacheID *pid;
	ListBase pidlist;

	BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);
	
	*min = 0;
	*max = 0;

	for (pid = pidlist.first; pid; pid = pid->next) {
		if (pid->cache == cache) {
			*max = max_ii(0, BLI_countlist(pid->ptcaches) - 1);
			break;
		}
	}

	BLI_freelistN(&pidlist);
}

static int rna_Cache_active_point_cache_index_get(PointerRNA *ptr)
{
	Object *ob = ptr->id.data;
	PointCache *cache = ptr->data;
	PTCacheID *pid;
	ListBase pidlist;
	int num = 0;

	BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);
	
	for (pid = pidlist.first; pid; pid = pid->next) {
		if (pid->cache == cache) {
			num = BLI_findindex(pid->ptcaches, cache);
			break;
		}
	}

	BLI_freelistN(&pidlist);

	return num;
}

static void rna_Cache_active_point_cache_index_set(struct PointerRNA *ptr, int value)
{
	Object *ob = ptr->id.data;
	PointCache *cache = ptr->data;
	PTCacheID *pid;
	ListBase pidlist;

	BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);
	
	for (pid = pidlist.first; pid; pid = pid->next) {
		if (pid->cache == cache) {
			*(pid->cache_ptr) = BLI_findlink(pid->ptcaches, value);
			break;
		}
	}

	BLI_freelistN(&pidlist);
}

static void rna_PointCache_frame_step_range(PointerRNA *ptr, int *min, int *max,
                                            int *UNUSED(softmin), int *UNUSED(softmax))
{
	Object *ob = ptr->id.data;
	PointCache *cache = ptr->data;
	PTCacheID *pid;
	ListBase pidlist;

	*min = 1;
	*max = 20;

	BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);
	
	for (pid = pidlist.first; pid; pid = pid->next) {
		if (pid->cache == cache) {
			*max = pid->max_step;
			break;
		}
	}

	BLI_freelistN(&pidlist);
}

#else

/* ptcache.point_caches */
static void rna_def_ptcache_point_caches(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* FunctionRNA *func; */
	/* PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "PointCaches");
	srna = RNA_def_struct(brna, "PointCaches", NULL);
	RNA_def_struct_sdna(srna, "PointCache");
	RNA_def_struct_ui_text(srna, "Point Caches", "Collection of point caches");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Cache_active_point_cache_index_get",
	                           "rna_Cache_active_point_cache_index_set",
	                           "rna_Cache_active_point_cache_index_range");
	RNA_def_property_ui_text(prop, "Active Point Cache Index", "");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_change");
}

static void rna_def_pointcache(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem point_cache_compress_items[] = {
		{PTCACHE_COMPRESS_NO, "NO", 0, "No", "No compression"},
		{PTCACHE_COMPRESS_LZO, "LIGHT", 0, "Light", "Fast but not so effective compression"},
		{PTCACHE_COMPRESS_LZMA, "HEAVY", 0, "Heavy", "Effective but slow compression"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "PointCache", NULL);
	RNA_def_struct_ui_text(srna, "Point Cache", "Point cache for physics simulations");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);
	
	prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "startframe");
	RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
	RNA_def_property_ui_range(prop, 1, MAXFRAME, 1, 1);
	RNA_def_property_ui_text(prop, "Start", "Frame on which the simulation starts");
	
	prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "endframe");
	RNA_def_property_range(prop, 1, MAXFRAME);
	RNA_def_property_ui_text(prop, "End", "Frame on which the simulation stops");

	prop = RNA_def_property(srna, "frame_step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "step");
	RNA_def_property_range(prop, 1, 20);
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_PointCache_frame_step_range");
	RNA_def_property_ui_text(prop, "Cache Step", "Number of frames between cached frames");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_change");

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "index");
	RNA_def_property_range(prop, -1, 100);
	RNA_def_property_ui_text(prop, "Cache Index", "Index number of cache files");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");

	prop = RNA_def_property(srna, "compression", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, point_cache_compress_items);
	RNA_def_property_ui_text(prop, "Cache Compression", "Compression method to be used");

	/* flags */
	prop = RNA_def_property(srna, "is_baked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_BAKED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "is_baking", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_BAKING);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_disk_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_DISK_CACHE);
	RNA_def_property_ui_text(prop, "Disk Cache", "Save cache files to disk (.blend file must be saved first)");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_toggle_disk_cache");

	prop = RNA_def_property(srna, "is_outdated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_OUTDATED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Cache is outdated", "");

	prop = RNA_def_property(srna, "frames_skipped", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_FRAMES_SKIPPED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "Cache name");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "path");
	RNA_def_property_ui_text(prop, "File Path", "Cache file path");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");

	/* removed, see PTCACHE_QUICK_CACHE */
#if 0
	prop = RNA_def_property(srna, "use_quick_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_QUICK_CACHE);
	RNA_def_property_ui_text(prop, "Quick Cache", "Update simulation with cache steps");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_change");
#endif

	prop = RNA_def_property(srna, "info", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "info");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Cache Info", "Info on current cache status");

	prop = RNA_def_property(srna, "use_external", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_EXTERNAL);
	RNA_def_property_ui_text(prop, "External", "Read cache from an external location");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");

	prop = RNA_def_property(srna, "use_library_path", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", PTCACHE_IGNORE_LIBPATH);
	RNA_def_property_ui_text(prop, "Library Path",
	                         "Use this file's path for the disk cache when library linked into another file "
	                         "(for local bakes per scene file, disable this option)");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");

	prop = RNA_def_property(srna, "point_caches", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_Cache_list_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "PointCache");
	RNA_def_property_ui_text(prop, "Point Cache List", "Point cache list");
	rna_def_ptcache_point_caches(brna, prop);
}

void RNA_def_pointcache(BlenderRNA *brna)
{
	rna_def_pointcache(brna);
}

#endif
