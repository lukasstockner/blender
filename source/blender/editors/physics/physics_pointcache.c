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

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

/**** NEW POINT CACHE ****/
#include "PTC_api.h"
/*************************/

#include "ED_particle.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "physics_intern.h"

static int cache_break_test(void *UNUSED(cbd))
{
	return (G.is_break == TRUE);
}
static int ptcache_bake_all_poll(bContext *C)
{
	Scene *scene= CTX_data_scene(C);

	if (!scene)
		return 0;
	
	return 1;
}

static int ptcache_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
	return (ptr.data && ptr.id.data);
}

static void bake_console_progress(void *UNUSED(arg), int nr)
{
	printf("\rbake: %3i%%", nr);
	fflush(stdout);
}

static void bake_console_progress_end(void *UNUSED(arg))
{
	printf("\rbake: done!\n");
}

static void ptcache_free_bake(PointCache *cache)
{
	if (cache->edit) {
		if (!cache->edit->edited || 1) {// XXX okee("Lose changes done in particle mode?")) {
			PE_free_ptcache_edit(cache->edit);
			cache->edit = NULL;
			cache->state.flag &= ~PTC_STATE_BAKED;
		}
	}
	else {
		cache->state.flag &= ~PTC_STATE_BAKED;
	}
}

static int ptcache_bake_all_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	wmWindow *win = G.background ? NULL : CTX_wm_window(C);
	PTCacheBaker baker;

	baker.main = bmain;
	baker.scene = scene;
	baker.pid = NULL;
	baker.bake = RNA_boolean_get(op->ptr, "bake");
	baker.render = 0;
	baker.anim_init = 0;
	baker.quick_step = 1;
	baker.break_test = cache_break_test;
	baker.break_data = NULL;

	/* Disabled for now as this doesn't work properly,
	 * and pointcache baking will be reimplemented with
	 * the job system soon anyways. */
	if (win) {
		baker.progressbar = (void (*)(void *, int))WM_cursor_time;
		baker.progressend = (void (*)(void *))WM_cursor_modal_restore;
		baker.progresscontext = win;
	}
	else {
		baker.progressbar = bake_console_progress;
		baker.progressend = bake_console_progress_end;
		baker.progresscontext = NULL;
	}

	BKE_ptcache_bake(&baker);

	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
	WM_event_add_notifier(C, NC_OBJECT|ND_POINTCACHE, NULL);

	return OPERATOR_FINISHED;
}
static int ptcache_free_bake_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene= CTX_data_scene(C);
	Base *base;
	PTCacheID *pid;
	ListBase pidlist;

	for (base=scene->base.first; base; base= base->next) {
		BKE_ptcache_ids_from_object(&pidlist, base->object, scene, MAX_DUPLI_RECUR);

		for (pid=pidlist.first; pid; pid=pid->next) {
			ptcache_free_bake(pid->cache);
		}
		
		BLI_freelistN(&pidlist);
		
		WM_event_add_notifier(C, NC_OBJECT|ND_POINTCACHE, base->object);
	}

	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);

	return OPERATOR_FINISHED;
}

void PTCACHE_OT_bake_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake All Physics";
	ot->description = "Bake all physics";
	ot->idname = "PTCACHE_OT_bake_all";
	
	/* api callbacks */
	ot->exec = ptcache_bake_all_exec;
	ot->poll = ptcache_bake_all_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "bake", 1, "Bake", "");
}
void PTCACHE_OT_free_bake_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Free All Physics Bakes";
	ot->idname = "PTCACHE_OT_free_bake_all";
	ot->description = "Free all baked caches of all objects in the current scene";
	
	/* api callbacks */
	ot->exec = ptcache_free_bake_all_exec;
	ot->poll = ptcache_bake_all_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}
static int ptcache_bake_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	wmWindow *win = G.background ? NULL : CTX_wm_window(C);
	PointerRNA ptr= CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
	Object *ob= ptr.id.data;
	PointCache *cache= ptr.data;
	PTCacheBaker baker;
	PTCacheID *pid;
	ListBase pidlist;

	BKE_ptcache_ids_from_object(&pidlist, ob, scene, MAX_DUPLI_RECUR);
	
	for (pid=pidlist.first; pid; pid=pid->next) {
		if (pid->cache == cache)
			break;
	}

	baker.main = bmain;
	baker.scene = scene;
	baker.pid = pid;
	baker.bake = RNA_boolean_get(op->ptr, "bake");
	baker.render = 0;
	baker.anim_init = 0;
	baker.quick_step = 1;
	baker.break_test = cache_break_test;
	baker.break_data = NULL;

	/* Disabled for now as this doesn't work properly,
	 * and pointcache baking will be reimplemented with
	 * the job system soon anyways. */
	if (win) {
		baker.progressbar = (void (*)(void *, int))WM_cursor_time;
		baker.progressend = (void (*)(void *))WM_cursor_modal_restore;
		baker.progresscontext = win;
	}
	else {
		printf("\n"); /* empty first line before console reports */
		baker.progressbar = bake_console_progress;
		baker.progressend = bake_console_progress_end;
		baker.progresscontext = NULL;
	}

	BKE_ptcache_bake(&baker);

	BLI_freelistN(&pidlist);

	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
	WM_event_add_notifier(C, NC_OBJECT|ND_POINTCACHE, ob);

	return OPERATOR_FINISHED;
}
static int ptcache_free_bake_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
	PointCache *cache= ptr.data;
	Object *ob= ptr.id.data;

	ptcache_free_bake(cache);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_POINTCACHE, ob);

	return OPERATOR_FINISHED;
}
static int ptcache_bake_from_cache_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
	PointCache *cache= ptr.data;
	Object *ob= ptr.id.data;
	
	cache->state.flag |= PTC_STATE_BAKED;
	
	WM_event_add_notifier(C, NC_OBJECT|ND_POINTCACHE, ob);

	return OPERATOR_FINISHED;
}
void PTCACHE_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake Physics";
	ot->description = "Bake physics";
	ot->idname = "PTCACHE_OT_bake";
	
	/* api callbacks */
	ot->exec = ptcache_bake_exec;
	ot->poll = ptcache_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "bake", 0, "Bake", "");
}
void PTCACHE_OT_free_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Free Physics Bake";
	ot->description = "Free physics bake";
	ot->idname = "PTCACHE_OT_free_bake";
	
	/* api callbacks */
	ot->exec = ptcache_free_bake_exec;
	ot->poll = ptcache_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}
void PTCACHE_OT_bake_from_cache(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake From Cache";
	ot->description = "Bake from cache";
	ot->idname = "PTCACHE_OT_bake_from_cache";
	
	/* api callbacks */
	ot->exec = ptcache_bake_from_cache_exec;
	ot->poll = ptcache_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}


/**** NEW POINT CACHE ****/

typedef struct PTCacheExportJob {
	short *stop, *do_update;
	float *progress;
	
	struct Main *bmain;
	struct Scene *scene;
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
	
	G.is_break = FALSE;
	
	/* XXX where to get this from? */
	start_frame = scene->r.sfra;
	end_frame = scene->r.efra;
	PTC_bake(data->bmain, scene, data->writer, start_frame, end_frame, stop, do_update, progress);
	
	*do_update = TRUE;
	*stop = 0;
}

static void ptcache_export_endjob(void *customdata)
{
	PTCacheExportJob *data = (PTCacheExportJob *)customdata;
	Scene *scene = data->scene;
	
	G.is_rendering = FALSE;
	BKE_spacedata_draw_locks(false);
	
	/* free the cache writer (closes output file) */
	PTC_writer_free(data->writer);
	
	/* reset scene frame */
	scene->r.cfra = data->origfra;
	scene->r.framelen = data->origframelen;
	BKE_scene_update_for_newframe(data->bmain, scene, scene->lay);
}

static int ptcache_export_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptcache_ptr = CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
	PointerRNA user_ptr = CTX_data_pointer_get(C, "point_cache_user");
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	PointCache *cache = ptcache_ptr.data;
	struct PTCWriter *writer;
	PTCacheExportJob *data;
	wmJob *wm_job;
	
	writer = PTC_writer_from_rna(scene, &user_ptr);
	if (!writer) {
		BKE_reportf(op->reports, RPT_ERROR_INVALID_INPUT, "%s is not a valid point cache user type", RNA_struct_identifier(user_ptr.type));
		return OPERATOR_CANCELLED;
	}
	
	/* XXX annoying hack: needed to prevent data corruption when changing
	 * scene frame in separate threads
	 */
	G.is_rendering = TRUE;
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
