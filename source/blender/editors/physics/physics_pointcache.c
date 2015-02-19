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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/physics/physics_pointcache.c
 *  \ingroup edphys
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "PTC_api.h"

#include "ED_particle.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "BLF_translation.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "physics_intern.h"

static int ptcache_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
	return (ptr.data && ptr.id.data);
}

typedef struct PTCacheExportJob {
	short *stop, *do_update;
	float *progress;
	
	struct Main *bmain;
	struct Scene *scene;
	EvaluationContext eval_ctx;
	
	PointerRNA user_ptr;
	struct PointCache *cache;
	struct PTCWriter *writer;
	
	int origfra;				/* original frame to reset scene after export */
	float origframelen;			/* original frame length to reset scene after export */
} PTCacheExportJob;

static void ptcache_export_freejob(void *customdata)
{
	PTCacheExportJob *data= (PTCacheExportJob *)customdata;
	MEM_freeN(data);
}

static void ptcache_export_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
	PTCacheExportJob *data= (PTCacheExportJob *)customdata;
	Scene *scene = data->scene;
	int start_frame, end_frame;
	
	data->stop = stop;
	data->do_update = do_update;
	data->progress = progress;
	
	data->origfra = scene->r.cfra;
	data->origframelen = scene->r.framelen;
	scene->r.framelen = 1.0f;
	memset(&data->eval_ctx, 0, sizeof(EvaluationContext));
	data->eval_ctx.mode = DAG_EVAL_RENDER;
	
	G.is_break = false;
	
	/* XXX where to get this from? */
	start_frame = scene->r.sfra;
	end_frame = scene->r.efra;
	PTC_bake(data->bmain, scene, &data->eval_ctx, data->writer, start_frame, end_frame, stop, do_update, progress);
	
	*do_update = true;
	*stop = 0;
}

static void ptcache_export_endjob(void *customdata)
{
	PTCacheExportJob *data = (PTCacheExportJob *)customdata;
	Scene *scene = data->scene;
	
	G.is_rendering = false;
	BKE_spacedata_draw_locks(false);
	
	/* free the cache writer (closes output file) */
	if (RNA_struct_is_a(data->user_ptr.type, &RNA_PointCacheModifier)) {
		Object *ob = (Object *)data->user_ptr.id.data;
		PointCacheModifierData *pcmd = (PointCacheModifierData *)data->user_ptr.data;
		
		PTC_mod_point_cache_set_mode(scene, ob, pcmd, MOD_POINTCACHE_MODE_NONE);
	}
	else {
		PTC_writer_free(data->writer);
	}
	
	/* reset scene frame */
	scene->r.cfra = data->origfra;
	scene->r.framelen = data->origframelen;
	BKE_scene_update_for_newframe(&data->eval_ctx, data->bmain, scene, scene->lay);
}

static int ptcache_export_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptcache_ptr = CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
	PointerRNA user_ptr = CTX_data_pointer_get(C, "point_cache_user");
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	PointCache *cache = ptcache_ptr.data;
	struct PTCWriter *writer = NULL;
	PTCacheExportJob *data;
	wmJob *wm_job;
	
	if (!user_ptr.data) {
		BKE_reportf(op->reports, RPT_ERROR_INVALID_INPUT, "Missing point cache user info");
		return OPERATOR_CANCELLED;
	}
	
	/* special case: point cache modifier uses internal writer
	 * and needs to be set up for baking.
	 */
	if (RNA_struct_is_a(user_ptr.type, &RNA_PointCacheModifier)) {
		Object *ob = (Object *)user_ptr.id.data;
		PointCacheModifierData *pcmd = (PointCacheModifierData *)user_ptr.data;
		
		PTC_mod_point_cache_set_mode(scene, ob, pcmd, MOD_POINTCACHE_MODE_WRITE);
	}
	else {
		writer = PTC_writer_from_rna(scene, &user_ptr);
		if (!writer) {
			BKE_reportf(op->reports, RPT_ERROR_INVALID_INPUT, "%s is not a valid point cache user type", RNA_struct_identifier(user_ptr.type));
			return OPERATOR_CANCELLED;
		}
	}
	
	/* XXX annoying hack: needed to prevent data corruption when changing
	 * scene frame in separate threads
	 */
	G.is_rendering = true;
	BKE_spacedata_draw_locks(true);
	
	/* XXX set WM_JOB_EXCL_RENDER to prevent conflicts with render jobs,
	 * since we need to set G.is_rendering
	 */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Point Cache Export",
	                     WM_JOB_PROGRESS | WM_JOB_EXCL_RENDER, WM_JOB_TYPE_PTCACHE_EXPORT);
	
	/* setup job */
	data = MEM_callocN(sizeof(PTCacheExportJob), "Point Cache Export Job");
	data->bmain = bmain;
	data->scene = scene;
	data->user_ptr = user_ptr;
	data->cache = cache;
	data->writer = writer;
	
	WM_jobs_customdata_set(wm_job, data, ptcache_export_freejob);
	WM_jobs_timer(wm_job, 0.1, NC_SCENE|ND_FRAME, NC_SCENE|ND_FRAME);
	WM_jobs_callbacks(wm_job, ptcache_export_startjob, NULL, NULL, ptcache_export_endjob);
	
	WM_jobs_start(CTX_wm_manager(C), wm_job);

	return OPERATOR_FINISHED;
}

void PTCACHE_OT_export(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Export";
	ot->description = "Export point data";
	ot->idname = "PTCACHE_OT_export";

	/* api callbacks */
	ot->exec = ptcache_export_exec;
	ot->poll = ptcache_poll;

	/* flags */
	/* no undo for this operator, cannot restore old cache files anyway */
	ot->flag = OPTYPE_REGISTER;
}


/********************** new material operator *********************/

static int new_cachelib_exec(bContext *C, wmOperator *UNUSED(op))
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cachelib", &RNA_CacheLibrary).data;
	Main *bmain = CTX_data_main(C);
	PointerRNA ptr, idptr;
	PropertyRNA *prop;
	
	/* add or copy material */
	if (cachelib) {
		cachelib = BKE_cache_library_copy(cachelib);
	}
	else {
		cachelib = BKE_cache_library_add(bmain, DATA_("CacheLibrary"));
	}
	
	/* hook into UI */
	UI_context_active_but_prop_get_templateID(C, &ptr, &prop);
	
	if (prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		cachelib->id.us--;
		
		RNA_id_pointer_create(&cachelib->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}
	
	WM_event_add_notifier(C, NC_OBJECT, cachelib);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Cache Library";
	ot->idname = "CACHELIBRARY_OT_new";
	ot->description = "Add a new cache library";
	
	/* api callbacks */
	ot->exec = new_cachelib_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}
