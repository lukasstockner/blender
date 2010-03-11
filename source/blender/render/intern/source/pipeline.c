/**  
 *
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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_userdef_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_writeavi.h"	/* <------ should be replaced once with generic movie module */
#include "BKE_sequencer.h"
#include "BKE_pointcache.h"
#include "BKE_animsys.h"	/* <------ should this be here?, needed for sequencer update */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_threads.h"

#include "PIL_time.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_pipeline.h"

/* internal */
#include "camera.h"
#include "database.h"
#include "object.h"
#include "object_halo.h"
#include "part.h"
#include "pixelfilter.h"
#include "render_types.h"
#include "rendercore.h"
#include "result.h"

/* render flow

1) Initialize state
- state data, tables
- movie/image file init
- everything that doesn't change during animation

2) Initialize data
- camera, world, matrices
- make render verts, faces, halos, strands
- everything can change per frame/field

3) Render Processor
- multiple layers
- tiles, rect, baking
- layers/tiles optionally to disk or directly in Render Result

4) Composite Render Result
- also read external files etc

5) Image Files
- save file or append in movie

*/


/* ********* globals ******** */

/* here we store all renders */
static struct {
	ListBase renderlist;

	/* render slots */
	int viewslot, renderingslot;

	/* commandline thread override */
	int threads;
} RenderGlobal = {{NULL, NULL}, 0, 0, -1}; 

/* ********* alloc and free ******** */


static volatile int g_break= 0;
static int thread_break(void *unused)
{
	return g_break;
}

/* default callbacks, set in each new render */
static void result_nothing(void *unused, RenderResult *rr) {}
static void result_rcti_nothing(void *unused, RenderResult *rr, volatile struct rcti *rect) {}
static void stats_nothing(void *unused, RenderStats *rs) {}
static void int_nothing(void *unused, int val) {}
static void print_error(void *unused, char *str) {printf("ERROR: %s\n", str);}
static int default_break(void *unused) {return G.afbreek == 1;}

int RE_RenderInProgress(Render *re)
{
	return re->result_ok==0;
}

static void stats_background(void *unused, RenderStats *rs)
{
	uintptr_t mem_in_use= MEM_get_memory_in_use();
	float megs_used_memory= mem_in_use/(1024.0*1024.0);
	char str[400], *spos= str;
	
	spos+= sprintf(spos, "Fra:%d Mem:%.2fM ", rs->cfra, megs_used_memory);
	
	if(rs->curfield)
		spos+= sprintf(spos, "Field %d ", rs->curfield);
	if(rs->curblur)
		spos+= sprintf(spos, "Blur %d ", rs->curblur);
	
	if(rs->infostr) {
		spos+= sprintf(spos, "| %s", rs->infostr);
	}
	else {
		if(rs->tothalo)
			spos+= sprintf(spos, "Sce: %s Ve:%d Fa:%d Ha:%d La:%d", rs->scenename, rs->totvert, rs->totface, rs->tothalo, rs->totlamp);
		else 
			spos+= sprintf(spos, "Sce: %s Ve:%d Fa:%d La:%d", rs->scenename, rs->totvert, rs->totface, rs->totlamp);
	}
	printf("%s\n", str);
}

static int render_scene_needs_vector(Render *re)
{
	SceneRenderLayer *srl;
	
	for(srl= re->db.scene->r.layers.first; srl; srl= srl->next)
		if(!(srl->layflag & SCE_LAY_DISABLE))
			if(srl->passflag & SCE_PASS_VECTOR)
				return 1;

	return 0;
}

void RE_SetViewSlot(int slot)
{
	RenderGlobal.viewslot = slot;
}

int RE_GetViewSlot(void)
{
	return RenderGlobal.viewslot;
}

static int re_get_slot(int slot)
{
	if(slot == RE_SLOT_VIEW)
		return RenderGlobal.viewslot;
	else if(slot == RE_SLOT_RENDERING)
		return (G.rendering)? RenderGlobal.renderingslot: RenderGlobal.viewslot;

	return slot;
}

Render *RE_GetRender(const char *name, int slot)
{
	Render *re;

	slot= re_get_slot(slot);
	
	/* search for existing renders */
	for(re= RenderGlobal.renderlist.first; re; re= re->next) {
		if(strncmp(re->name, name, RE_MAXNAME)==0 && re->slot==slot) {
			break;
		}
	}
	return re;
}

/* displist.c util.... */
Scene *RE_GetScene(Render *re)
{
	if(re)
		return re->db.scene;
	return NULL;
}

RenderLayer *render_get_active_layer(Render *re, RenderResult *rr)
{
	RenderLayer *rl= BLI_findlink(&rr->layers, re->params.r.actlay);
	
	if(rl) 
		return rl;
	else 
		return rr->layers.first;
}

RenderStats *RE_GetStats(Render *re)
{
	return &re->cb.i;
}

Render *RE_NewRender(const char *name, int slot)
{
	Render *re;

	slot= re_get_slot(slot);
	
	/* only one render per name exists */
	re= RE_GetRender(name, slot);
	if(re==NULL) {
		
		/* new render data struct */
		re= MEM_callocN(sizeof(Render), "new render");
		BLI_addtail(&RenderGlobal.renderlist, re);
		strncpy(re->name, name, RE_MAXNAME);
		re->slot= slot;
		BLI_rw_mutex_init(&re->resultmutex);
	}
	
	/* set default empty callbacks */
	re->cb.display_init= result_nothing;
	re->cb.display_clear= result_nothing;
	re->cb.display_draw= result_rcti_nothing;
	re->cb.timecursor= int_nothing;
	re->cb.test_break= default_break;
	re->cb.error= print_error;
	if(G.background)
		re->cb.stats_draw= stats_background;
	else
		re->cb.stats_draw= stats_nothing;
	/* clear callback handles */
	re->cb.dih= re->cb.dch= re->cb.ddh= re->cb.sdh= re->cb.tch= re->cb.tbh= re->cb.erh= NULL;
	
	/* init some variables */
	re->cam.ycor= 1.0f;
	
	return re;
}

/* only call this while you know it will remove the link too */
void RE_FreeRender(Render *re)
{
	BLI_rw_mutex_end(&re->resultmutex);
	
	render_db_free(&re->db);
	pxf_free(re);
	
	RE_FreeRenderResult(re->result);
	RE_FreeRenderResult(re->pushedresult);
	
	BLI_remlink(&RenderGlobal.renderlist, re);
	MEM_freeN(re);
}

/* exit blender */
void RE_FreeAllRender(void)
{
	while(RenderGlobal.renderlist.first) {
		RE_FreeRender(RenderGlobal.renderlist.first);
	}
}

/* ********* initialize state ******** */


/* what doesn't change during entire render sequence */
/* disprect is optional, if NULL it assumes full window render */
void RE_InitState(Render *re, Render *source, RenderData *rd, SceneRenderLayer *srl, int winx, int winy, rcti *disprect)
{
	re->ok= TRUE;	/* maybe flag */
	
	re->cb.i.starttime= PIL_check_seconds_timer();
	re->params.r= *rd;		/* hardcopy */
	
	re->cam.winx= winx;
	re->cam.winy= winy;
	if(disprect) {
		re->disprect= *disprect;
		re->rectx= disprect->xmax-disprect->xmin;
		re->recty= disprect->ymax-disprect->ymin;
	}
	else {
		re->disprect.xmin= re->disprect.ymin= 0;
		re->disprect.xmax= winx;
		re->disprect.ymax= winy;
		re->rectx= winx;
		re->recty= winy;
	}
	
	if(re->rectx < 2 || re->recty < 2 || (BKE_imtype_is_movie(rd->imtype) &&
										  (re->rectx < 16 || re->recty < 16) )) {
		re->cb.error(re->cb.erh, "Image too small");
		re->ok= 0;
		return;
	}

#ifdef WITH_OPENEXR
	if(re->params.r.scemode & R_FULL_SAMPLE)
		re->params.r.scemode |= R_EXR_TILE_FILE;	/* enable automatic */

	/* Until use_border is made compatible with save_buffers/full_sample, render without the later instead of not rendering at all.*/
	if(re->params.r.mode & R_BORDER) 
		re->params.r.scemode &= ~(R_EXR_TILE_FILE|R_FULL_SAMPLE);

#else
	/* can't do this without openexr support */
	re->params.r.scemode &= ~(R_EXR_TILE_FILE|R_FULL_SAMPLE);
#endif
	
	/* fullsample wants uniform osa levels */
	if(source && (re->params.r.scemode & R_FULL_SAMPLE)) {
		/* but, if source has no full sample we disable it */
		if((source->params.r.scemode & R_FULL_SAMPLE)==0)
			re->params.r.scemode &= ~R_FULL_SAMPLE;
		else
			re->params.r.osa= re->params.osa= source->params.osa;
	}
	else {
		/* check state variables, osa? */
		if(re->params.r.mode & (R_OSA)) {
			re->params.osa= re->params.r.osa;
			if(re->params.osa>16) re->params.osa= 16;
		}
		else re->params.osa= 0;
	}
	
	if (srl) {
		int index = BLI_findindex(&re->params.r.layers, srl);
		if (index != -1) {
			re->params.r.actlay = index;
			re->params.r.scemode |= (R_SINGLE_LAYER|R_COMP_RERENDER);
		}
	}
		
	/* always call, checks for gamma, gamma tables and jitter too */
	pxf_init(re);	
	
	/* if preview render, we try to keep old result */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	if(re->params.r.scemode & R_PREVIEWBUTS) {
		if(re->result && re->result->rectx==re->rectx && re->result->recty==re->recty);
		else {
			RE_FreeRenderResult(re->result);
			re->result= NULL;
		}
	}
	else {
		/* make empty render result, so display callbacks can initialize */
		RE_FreeRenderResult(re->result);
		re->result= MEM_callocN(sizeof(RenderResult), "new render result");
		re->result->rectx= re->rectx;
		re->result->recty= re->recty;
	}

	BLI_rw_mutex_unlock(&re->resultmutex);
	
	/* we clip faces with a minimum of 2 pixel boundary outside of image border. see zbuf.c */
	re->cam.clipcrop= 1.0f + 2.0f/(float)(re->cam.winx>re->cam.winy?re->cam.winy:re->cam.winx);
	
	RE_init_threadcount(re);
}

/* part of external api, not called for regular render pipeline */
void RE_SetDispRect (struct Render *re, rcti *disprect)
{
	re->disprect= *disprect;
	re->rectx= disprect->xmax-disprect->xmin;
	re->recty= disprect->ymax-disprect->ymin;
	
	/* initialize render result */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	RE_FreeRenderResult(re->result);
	re->result= render_result_create(re, &re->disprect, 0, RR_USEMEM);

	BLI_rw_mutex_unlock(&re->resultmutex);
}

/* image and movie output has to move to either imbuf or kernel */
void RE_display_init_cb(Render *re, void *handle, void (*f)(void *handle, RenderResult *rr))
{
	re->cb.display_init= f;
	re->cb.dih= handle;
}
void RE_display_clear_cb(Render *re, void *handle, void (*f)(void *handle, RenderResult *rr))
{
	re->cb.display_clear= f;
	re->cb.dch= handle;
}
void RE_display_draw_cb(Render *re, void *handle, void (*f)(void *handle, RenderResult *rr, volatile rcti *rect))
{
	re->cb.display_draw= f;
	re->cb.ddh= handle;
}
void RE_stats_draw_cb(Render *re, void *handle, void (*f)(void *handle, RenderStats *rs))
{
	re->cb.stats_draw= f;
	re->cb.sdh= handle;
}
void RE_timecursor_cb(Render *re, void *handle, void (*f)(void *handle, int))
{
	re->cb.timecursor= f;
	re->cb.tch= handle;
}

void RE_test_break_cb(Render *re, void *handle, int (*f)(void *handle))
{
	re->cb.test_break= f;
	re->cb.tbh= handle;
}
void RE_error_cb(Render *re, void *handle, void (*f)(void *handle, char *str))
{
	re->cb.error= f;
	re->cb.erh= handle;
}


/* *************************************** */

static int render_display_draw_enabled(Render *re)
{
	/* don't show preprocess for previewrender sss */
	if(re->db.sss_points)
		return !(re->params.r.scemode & R_PREVIEWBUTS);
	else
		return 1;
}

/* allocate osa new results for samples */
static RenderResult *new_full_sample_buffers(Render *re, ListBase *lb, rcti *partrct, int crop)
{
	int a;
	
	if(re->params.osa==0)
		return render_result_create(re, partrct, crop, RR_USEMEM);
	
	for(a=0; a<re->params.osa; a++) {
		RenderResult *rr= render_result_create(re, partrct, crop, RR_USEMEM);
		BLI_addtail(lb, rr);
		rr->sample_nr= a;
	}
	
	return lb->first;
}


/* the main thread call, renders an entire part */
static void *do_part_thread(void *pa_v)
{
	RenderPart *pa= pa_v;
	Render *re= pa->re;
	
	/* need to return nicely all parts on esc */
	if(re->cb.test_break(re->cb.tbh)==0) {
		if(re->db.sss_points) {
			pa->result= render_result_create(re, &pa->disprect, pa->crop, RR_USEMEM);

			render_sss_bake_part(re, pa);

			if(!(re->params.r.scemode & R_PREVIEWBUTS));
				render_result_merge_part(re, pa->result);
		}
		else {
			if(re->params.r.scemode & R_FULL_SAMPLE)
				pa->result= new_full_sample_buffers(re, &pa->fullresult, &pa->disprect, pa->crop);
			else
				pa->result= render_result_create(re, &pa->disprect, pa->crop, RR_USEMEM);

			if(re->params.r.integrator == R_INTEGRATOR_PATHTRACER)
				render_path_trace_part(re, pa);
			else
				render_rasterize_part(re, pa);

			render_result_merge_part(re, pa->result);
		}
	}
	
	pa->ready= 1;
	
	return NULL;
}

/* returns with render result filled, not threaded, used for preview now only */
static void render_tile_processor(Render *re, int firsttile)
{
	RenderPart *pa;
	
	if(re->cb.test_break(re->cb.tbh))
		return;

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	/* hrmf... exception, this is used for preview render, re-entrant, so render result has to be re-used */
	if(re->result==NULL || re->result->layers.first==NULL) {
		if(re->result) RE_FreeRenderResult(re->result);
		re->result= render_result_create(re, &re->disprect, 0, RR_USEMEM);
	}

	BLI_rw_mutex_unlock(&re->resultmutex);
	
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
 
	if(re->result==NULL)
		return;
	
	parts_create(re);

	for(pa= re->parts.first; pa; pa= pa->next) {
		if(firsttile) {
			re->cb.i.partsdone++;	/* was reset in parts_create */
			firsttile--;
		}
		else {
			do_part_thread(pa);
			
			if(pa->result) {
				if(!re->cb.test_break(re->cb.tbh)) {
					if(render_display_draw_enabled(re))
						re->cb.display_draw(re->cb.ddh, pa->result, NULL);
					
					re->cb.i.partsdone++;
					re->cb.stats_draw(re->cb.sdh, &re->cb.i);
				}
				RE_FreeRenderResult(pa->result);
				pa->result= NULL;
			}		
			if(re->cb.test_break(re->cb.tbh))
				break;
		}
	}

	parts_free(re);
}

static void print_part_stats(Render *re, RenderPart *pa)
{
	char str[64];
	
	sprintf(str, "Part %d-%d", pa->nr, re->cb.i.totpart);
	re->cb.i.infostr= str;
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
	re->cb.i.infostr= NULL;
}

typedef struct RenderThread {
	ThreadQueue *workqueue;
	ThreadQueue *donequeue;

	int number;
} RenderThread;

static void *do_render_thread(void *thread_v)
{
	RenderThread *thread= thread_v;
	RenderPart *pa;
	Render *re;

	while((pa=BLI_thread_queue_pop(thread->workqueue))) {
		re= pa->re;
		pa->thread= thread->number;
		do_part_thread(pa);
		BLI_thread_queue_push(thread->donequeue, pa);

		if(re->cb.test_break(re->cb.tbh))
			break;
	}

	return NULL;
}

static void threaded_tile_processor(Render *re)
{
	RenderThread thread[BLENDER_MAX_THREADS];
	ThreadQueue *workqueue, *donequeue;
	ListBase threads;
	RenderPart *pa; //, *nextpa;
	rctf viewplane= re->cam.viewplane;
	double lastdraw, elapsed, redrawtime= 1.0f;
	//int rendering=1, totpart= 0, drawtimer=0, hasdrawn, minx=0, a, wait;
	int (*test_break)(void *handle);
	int totpart= 0, minx=0, slice=0, a, wait;
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	/* first step; free the entire render result, make new, and/or prepare exr buffer saving */
	if(re->result==NULL || !(re->params.r.scemode & R_PREVIEWBUTS)) {
		RE_FreeRenderResult(re->result);
	
		if(re->db.sss_points && render_display_draw_enabled(re))
			re->result= render_result_create(re, &re->disprect, 0, 0);
		else if(re->params.r.scemode & R_FULL_SAMPLE)
			re->result= render_result_full_sample_create(re);
		else
			re->result= render_result_create(re, &re->disprect, 0, re->params.r.scemode & (R_EXR_TILE_FILE|R_FULL_SAMPLE));
	}

	BLI_rw_mutex_unlock(&re->resultmutex);
	
	if(re->result==NULL)
		return;
	
	/* warning; no return here without closing exr file */
	
	parts_create(re);

	if(re->result->exrhandle)
		render_result_exr_write(re);

	/* we use original slower break function, while render threads
	   use quick thread_break callback based on variable we set */
	test_break= re->cb.test_break;
	re->cb.test_break= thread_break;
	
	/* create and fill work queue */
	workqueue= BLI_thread_queue_init();
	donequeue= BLI_thread_queue_init();

	/* for panorama we loop over slices */
	while(parts_find_next_slice(re, &slice, &minx, &viewplane)) {
		/* gather parts into queue */
		while((pa= parts_find_next(re, minx))) {
			pa->nr= totpart+1; /* for nicest part, and for stats */
			totpart++;
			BLI_thread_queue_push(workqueue, pa);
		}

		BLI_thread_queue_nowait(workqueue);

		/* start all threads */
		BLI_init_threads(&threads, do_render_thread, re->params.r.threads);

		for(a=0; a<re->params.r.threads; a++) {
			thread[a].workqueue= workqueue;
			thread[a].donequeue= donequeue;
			thread[a].number= a;
			BLI_insert_thread(&threads, &thread[a]);
		}

		/* wait for results to come back */
		lastdraw = PIL_check_seconds_timer();

		while(1) {
			elapsed= PIL_check_seconds_timer() - lastdraw;
			wait= (redrawtime - elapsed)*1000;

			/* handle finished part */
			if((pa=BLI_thread_queue_pop_timeout(donequeue, wait))) {
				if(pa->result) {
					if(render_display_draw_enabled(re))
						re->cb.display_draw(re->cb.ddh, pa->result, NULL);
					print_part_stats(re, pa);
					
					render_result_free(&pa->fullresult, pa->result);
					pa->result= NULL;
					re->cb.i.partsdone++;
				}

				totpart--;
			}

			/* check for render cancel */
			if((g_break=test_break(re->cb.tbh)))
				break;

			/* or done with parts */
			if(totpart == 0)
				break;

			/* redraw in progress parts */
			elapsed= PIL_check_seconds_timer() - lastdraw;
			if(elapsed > redrawtime) {
				if(render_display_draw_enabled(re))
					for(pa= re->parts.first; pa; pa= pa->next)
						if(!pa->ready && pa->nr && pa->result)
							re->cb.display_draw(re->cb.ddh, pa->result, &pa->result->renrect);

				lastdraw= PIL_check_seconds_timer();
			}
		}

		BLI_end_threads(&threads);

		/* in case we cancelled, free remaining results */
		while(BLI_thread_queue_size(donequeue)) {
			pa= BLI_thread_queue_pop(donequeue);
			render_result_free(&pa->fullresult, pa->result);
			pa->result= NULL;
		}

		if((g_break=test_break(re->cb.tbh)))
			break;
	}

	BLI_thread_queue_free(donequeue);
	BLI_thread_queue_free(workqueue);

	if(re->result->exrhandle)
		render_result_exr_read(re);

	/* unset threadsafety */
	g_break= 0;
	
	parts_free(re);
	re->cam.viewplane= viewplane; /* restore viewplane, modified by pano render */
}

/* currently threaded=0 only used by envmap */
void RE_TileProcessor(Render *re, int firsttile, int threaded)
{
	/* the partsdone variable has to be reset to firsttile, to survive esc before it was set to zero */
	
	re->cb.i.partsdone= firsttile;

	if(!re->db.sss_points)
		re->cb.i.starttime= PIL_check_seconds_timer();

	if(threaded)
		threaded_tile_processor(re);
	else
		render_tile_processor(re, firsttile);
		
	if(!re->db.sss_points)
		re->cb.i.lastframetime= PIL_check_seconds_timer()- re->cb.i.starttime;
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
}


/* ************  This part uses API, for rendering Blender scenes ********** */

static void external_render_3d(Render *re, RenderEngineType *type);

static void do_render_3d(Render *re)
{
	RenderEngineType *type;

	/* try external */
	for(type=R_engines.first; type; type=type->next)
		if(strcmp(type->idname, re->params.r.engine) == 0)
			break;

	if(type && type->render) {
		external_render_3d(re, type);
		return;
	}

	/* internal */
	
	/* make render verts/faces/halos/lamps */
	if(render_scene_needs_vector(re))
		RE_Database_FromScene_Vectors(re, re->db.scene);
	else
	   RE_Database_FromScene(re, re->db.scene, 1);
	
	threaded_tile_processor(re);
	
	/* do left-over 3d post effects (flares) */
	if(re->params.flag & R_HALO)
		if(!re->cb.test_break(re->cb.tbh))
			halos_render_flare(re);
	
	/* free all render verts etc */
	RE_Database_Free(re);
}

/* called by blur loop, accumulate RGBA key alpha */
static void addblur_rect_key(RenderResult *rr, float *rectf, float *rectf1, float blurfac)
{
	float mfac= 1.0f - blurfac;
	int a, b, stride= 4*rr->rectx;
	int len= stride*sizeof(float);
	
	for(a=0; a<rr->recty; a++) {
		if(blurfac==1.0f) {
			memcpy(rectf, rectf1, len);
		}
		else {
			float *rf= rectf, *rf1= rectf1;
			
			for( b= rr->rectx; b>0; b--, rf+=4, rf1+=4) {
				if(rf1[3]<0.01f)
					rf[3]= mfac*rf[3];
				else if(rf[3]<0.01f) {
					rf[0]= rf1[0];
					rf[1]= rf1[1];
					rf[2]= rf1[2];
					rf[3]= blurfac*rf1[3];
				}
				else {
					rf[0]= mfac*rf[0] + blurfac*rf1[0];
					rf[1]= mfac*rf[1] + blurfac*rf1[1];
					rf[2]= mfac*rf[2] + blurfac*rf1[2];
					rf[3]= mfac*rf[3] + blurfac*rf1[3];
				}				
			}
		}
		rectf+= stride;
		rectf1+= stride;
	}
}

/* called by blur loop, accumulate renderlayers */
static void addblur_rect(RenderResult *rr, float *rectf, float *rectf1, float blurfac, int channels)
{
	float mfac= 1.0f - blurfac;
	int a, b, stride= channels*rr->rectx;
	int len= stride*sizeof(float);
	
	for(a=0; a<rr->recty; a++) {
		if(blurfac==1.0f) {
			memcpy(rectf, rectf1, len);
		}
		else {
			float *rf= rectf, *rf1= rectf1;
			
			for( b= rr->rectx*channels; b>0; b--, rf++, rf1++) {
				rf[0]= mfac*rf[0] + blurfac*rf1[0];
			}
		}
		rectf+= stride;
		rectf1+= stride;
	}
}


/* called by blur loop, accumulate renderlayers */
static void merge_renderresult_blur(RenderResult *rr, RenderResult *brr, float blurfac, int key_alpha)
{
	RenderLayer *rl, *rl1;
	RenderPass *rpass, *rpass1;
	
	rl1= brr->layers.first;
	for(rl= rr->layers.first; rl && rl1; rl= rl->next, rl1= rl1->next) {
		
		/* combined */
		if(rl->rectf && rl1->rectf) {
			if(key_alpha)
				addblur_rect_key(rr, rl->rectf, rl1->rectf, blurfac);
			else
				addblur_rect(rr, rl->rectf, rl1->rectf, blurfac, 4);
		}
		
		/* passes are allocated in sync */
		rpass1= rl1->passes.first;
		for(rpass= rl->passes.first; rpass && rpass1; rpass= rpass->next, rpass1= rpass1->next) {
			addblur_rect(rr, rpass->rect, rpass1->rect, blurfac, rpass->channels);
		}
	}
}

/* main blur loop, can be called by fields too */
static void do_render_blur_3d(Render *re)
{
	RenderResult *rres;
	float blurfac;
	int blur= re->params.r.mblur_samples;
	
	/* create accumulation render result */
	rres= render_result_create(re, &re->disprect, 0, RR_USEMEM);
	
	/* do the blur steps */
	while(blur--) {
		set_mblur_offs( re->params.r.blurfac*((float)(re->params.r.mblur_samples-blur))/(float)re->params.r.mblur_samples );
		
		re->cb.i.curblur= re->params.r.mblur_samples-blur;	/* stats */
		
		do_render_3d(re);
		
		blurfac= 1.0f/(float)(re->params.r.mblur_samples-blur);
		
		merge_renderresult_blur(rres, re->result, blurfac, re->params.r.alphamode & R_ALPHAKEY);
		if(re->cb.test_break(re->cb.tbh)) break;
	}
	
	/* swap results */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	RE_FreeRenderResult(re->result);
	re->result= rres;
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	set_mblur_offs(0.0f);
	re->cb.i.curblur= 0;	/* stats */
	
	/* weak... the display callback wants an active renderlayer pointer... */
	re->result->renlay= render_get_active_layer(re, re->result);
	re->cb.display_draw(re->cb.ddh, re->result, NULL);	
}


/* function assumes rectf1 and rectf2 to be half size of rectf */
static void interleave_rect(RenderResult *rr, float *rectf, float *rectf1, float *rectf2, int channels)
{
	int a, stride= channels*rr->rectx;
	int len= stride*sizeof(float);
	
	for(a=0; a<rr->recty; a+=2) {
		memcpy(rectf, rectf1, len);
		rectf+= stride;
		rectf1+= stride;
		memcpy(rectf, rectf2, len);
		rectf+= stride;
		rectf2+= stride;
	}
}

/* merge render results of 2 fields */
static void merge_renderresult_fields(RenderResult *rr, RenderResult *rr1, RenderResult *rr2)
{
	RenderLayer *rl, *rl1, *rl2;
	RenderPass *rpass, *rpass1, *rpass2;
	
	rl1= rr1->layers.first;
	rl2= rr2->layers.first;
	for(rl= rr->layers.first; rl && rl1 && rl2; rl= rl->next, rl1= rl1->next, rl2= rl2->next) {
		
		/* combined */
		if(rl->rectf && rl1->rectf && rl2->rectf)
			interleave_rect(rr, rl->rectf, rl1->rectf, rl2->rectf, 4);
		
		/* passes are allocated in sync */
		rpass1= rl1->passes.first;
		rpass2= rl2->passes.first;
		for(rpass= rl->passes.first; rpass && rpass1 && rpass2; rpass= rpass->next, rpass1= rpass1->next, rpass2= rpass2->next) {
			interleave_rect(rr, rpass->rect, rpass1->rect, rpass2->rect, rpass->channels);
		}
	}
}


/* interleaves 2 frames */
static void do_render_fields_3d(Render *re)
{
	RenderResult *rr1, *rr2= NULL;
	
	/* no render result was created, we can safely halve render y */
	re->cam.winy /= 2;
	re->recty /= 2;
	re->disprect.ymin /= 2;
	re->disprect.ymax /= 2;
	
	re->cb.i.curfield= 1;	/* stats */
	
	/* first field, we have to call camera routine for correct aspect and subpixel offset */
	RE_SetCamera(re, re->db.scene->camera);
	if(re->params.r.mode & R_MBLUR)
		do_render_blur_3d(re);
	else
		do_render_3d(re);

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	rr1= re->result;
	re->result= NULL;
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	/* second field */
	if(!re->cb.test_break(re->cb.tbh)) {
		
		re->cb.i.curfield= 2;	/* stats */
		
		re->params.flag |= R_SEC_FIELD;
		if((re->params.r.mode & R_FIELDSTILL)==0) 
			set_field_offs(0.5f);
		RE_SetCamera(re, re->db.scene->camera);
		if(re->params.r.mode & R_MBLUR)
			do_render_blur_3d(re);
		else
			do_render_3d(re);
		re->params.flag &= ~R_SEC_FIELD;
		set_field_offs(0.0f);
		
		rr2= re->result;
	}
	
	/* allocate original height new buffers */
	re->cam.winy *= 2;
	re->recty *= 2;
	re->disprect.ymin *= 2;
	re->disprect.ymax *= 2;

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	re->result= render_result_create(re, &re->disprect, 0, RR_USEMEM);

	if(rr2) {
		if(re->params.r.mode & R_ODDFIELD)
			merge_renderresult_fields(re->result, rr2, rr1);
		else
			merge_renderresult_fields(re->result, rr1, rr2);
		
		RE_FreeRenderResult(rr2);
	}

	RE_FreeRenderResult(rr1);
	
	re->cb.i.curfield= 0;	/* stats */
	
	/* weak... the display callback wants an active renderlayer pointer... */
	re->result->renlay= render_get_active_layer(re, re->result);

	BLI_rw_mutex_unlock(&re->resultmutex);

	re->cb.display_draw(re->cb.ddh, re->result, NULL);
}

/* main render routine, no compositing */
static void do_render_fields_blur_3d(Render *re)
{
	/* also check for camera here */
	if(re->db.scene->camera==NULL) {
		printf("ERROR: Cannot render, no camera\n");
		G.afbreek= 1;
		return;
	}
	
	/* now use renderdata and camera to set viewplane */
	RE_SetCamera(re, re->db.scene->camera);
	
	if(re->params.r.mode & R_FIELDS)
		do_render_fields_3d(re);
	else if(re->params.r.mode & R_MBLUR)
		do_render_blur_3d(re);
	else
		do_render_3d(re);
	
	/* when border render, check if we have to insert it in black */
	if(re->result)
		if(re->params.r.mode & R_BORDER)
			render_result_border_merge(re);
}


/* within context of current Render *re, render another scene.
   it uses current render image size and disprect, but doesn't execute composite
*/
static void render_scene(Render *re, Scene *sce, int cfra)
{
	Render *resc= RE_NewRender(sce->id.name, RE_SLOT_RENDERING);
	int winx= re->cam.winx, winy= re->cam.winy;
	
	sce->r.cfra= cfra;
		
	/* exception: scene uses own size (unfinished code) */
	if(0) {
		winx= (sce->r.size*sce->r.xsch)/100;
		winy= (sce->r.size*sce->r.ysch)/100;
	}
	
	/* initial setup */
	RE_InitState(resc, re, &sce->r, NULL, winx, winy, &re->disprect);
	
	/* still unsure entity this... */
	resc->db.scene= sce;
	
	/* ensure scene has depsgraph, base flags etc OK */
	set_scene_bg(sce);

	/* copy callbacks */
	resc->cb.display_draw= re->cb.display_draw;
	resc->cb.ddh= re->cb.ddh;
	resc->cb.test_break= re->cb.test_break;
	resc->cb.tbh= re->cb.tbh;
	resc->cb.stats_draw= re->cb.stats_draw;
	resc->cb.sdh= re->cb.sdh;
	
	do_render_fields_blur_3d(resc);
}

void tag_scenes_for_render(Render *re)
{
	bNode *node;
	Scene *sce;
	
	for(sce= G.main->scene.first; sce; sce= sce->id.next)
		sce->id.flag &= ~LIB_DOIT;
	
	re->db.scene->id.flag |= LIB_DOIT;
	
	if(re->db.scene->nodetree==NULL) return;
	
	/* check for render-layers nodes using other scenes, we tag them LIB_DOIT */
	for(node= re->db.scene->nodetree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS) {
			if(node->id) {
				if(node->id != (ID *)re->db.scene)
					node->id->flag |= LIB_DOIT;
			}
		}
	}
	
}

static void ntree_render_scenes(Render *re)
{
	bNode *node;
	int cfra= re->db.scene->r.cfra;
	
	if(re->db.scene->nodetree==NULL) return;
	
	tag_scenes_for_render(re);
	
	/* now foreach render-result node tagged we do a full render */
	/* results are stored in a way compisitor will find it */
	for(node= re->db.scene->nodetree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS) {
			if(node->id && node->id != (ID *)re->db.scene) {
				if(node->id->flag & LIB_DOIT) {
					render_scene(re, (Scene *)node->id, cfra);
					node->id->flag &= ~LIB_DOIT;
				}
			}
		}
	}
}

/* helper call to detect if theres a composite with render-result node */
static int composite_needs_render(Scene *sce)
{
	bNodeTree *ntree= sce->nodetree;
	bNode *node;
	
	if(ntree==NULL) return 1;
	if(sce->use_nodes==0) return 1;
	if((sce->r.scemode & R_DOCOMP)==0) return 1;
		
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS)
			if(node->id==NULL || node->id==&sce->id)
				return 1;
	}
	return 0;
}

/* bad call... need to think over proper method still */
static void render_composit_stats(void *handle, char *str)
{
	Render *re= (Render*)handle;

	re->cb.i.infostr= str;
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
	re->cb.i.infostr= NULL;
}

void RE_MergeFullSample(Render *re, Scene *sce, bNodeTree *ntree)
{
	Scene *scene;
	bNode *node;
	
	/* first call RE_ReadRenderResult on every renderlayer scene. this creates Render structs */
	
	/* tag scenes unread */
	for(scene= G.main->scene.first; scene; scene= scene->id.next) 
		scene->id.flag |= LIB_DOIT;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS) {
			Scene *nodescene= (Scene *)node->id;
			
			if(nodescene==NULL) nodescene= sce;
			if(nodescene->id.flag & LIB_DOIT) {
				nodescene->r.mode |= R_OSA;	/* render struct needs tables */
				RE_ReadRenderResult(sce, nodescene);
				nodescene->id.flag &= ~LIB_DOIT;
			}
		}
	}
	
	/* own render result should be read/allocated */
	if(re->db.scene->id.flag & LIB_DOIT)
		RE_ReadRenderResult(re->db.scene, re->db.scene);
	
	/* and now we can draw (result is there) */
	re->cb.display_init(re->cb.dih, re->result);
	re->cb.display_clear(re->cb.dch, re->result);
	
	do_merge_fullsample(re, ntree, &RenderGlobal.renderlist);
}

/* returns fully composited render-result on given time step (in RenderData) */
static void do_render_composite_fields_blur_3d(Render *re)
{
	bNodeTree *ntree= re->db.scene->nodetree;
	int update_newframe=0;
	
	/* INIT seeding, compositor can use random texture */
	BLI_srandom(re->params.r.cfra);
	
	if(composite_needs_render(re->db.scene)) {
		/* save memory... free all cached images */
		ntreeFreeCache(ntree);
		
		do_render_fields_blur_3d(re);
	} else {
		/* scene render process already updates animsys */
		update_newframe = 1;
	}
	
	/* swap render result */
	if(re->params.r.scemode & R_SINGLE_LAYER)
		pop_render_result(re);
	
	if(!re->cb.test_break(re->cb.tbh)) {
		
		if(ntree) {
			ntreeCompositTagRender(re->db.scene);
			ntreeCompositTagAnimated(ntree);
		}
		
		if(1 || !(re->params.r.scemode & R_COMP_RERENDER)) {
			if(ntree && re->params.r.scemode & R_DOCOMP) {
				/* checks if there are render-result nodes that need scene */
				if((re->params.r.scemode & R_SINGLE_LAYER)==0)
					ntree_render_scenes(re);
				
				if(!re->cb.test_break(re->cb.tbh)) {
					ntree->stats_draw= render_composit_stats;
					ntree->sdh= re;
					ntree->test_break= re->cb.test_break;
					ntree->tbh= re->cb.tbh;
					
					if(update_newframe)
						scene_update_for_newframe(re->db.scene, re->db.scene->lay);
					
					if(re->params.r.scemode & R_FULL_SAMPLE) 
						do_merge_fullsample(re, ntree, &RenderGlobal.renderlist);
					else
						ntreeCompositExecTree(ntree, &re->params.r, G.background==0);
					
					ntree->stats_draw= NULL;
					ntree->test_break= NULL;
					ntree->tbh= ntree->sdh= NULL;
				}
			}
			else if(re->params.r.scemode & R_FULL_SAMPLE)
				do_merge_fullsample(re, NULL, &RenderGlobal.renderlist);
		}
	}

	/* weak... the display callback wants an active renderlayer pointer... */
	re->result->renlay= render_get_active_layer(re, re->result);
	re->cb.display_draw(re->cb.ddh, re->result, NULL);
}

static void renderresult_stampinfo(Scene *scene)
{
	RenderResult rres;
	Render *re= RE_GetRender(scene->id.name, RE_SLOT_RENDERING);

	/* this is the basic trick to get the displayed float or char rect from render result */
	RE_AcquireResultImage(re, &rres);
	BKE_stamp_buf(scene, (unsigned char *)rres.rect32, rres.rectf, rres.rectx, rres.recty, 4);
	RE_ReleaseResultImage(re);
}

static void do_render_seq(Render * re)
{
	static int recurs_depth = 0;
	struct ImBuf *ibuf;
	RenderResult *rr = re->result;
	int cfra = re->params.r.cfra;

	if(recurs_depth==0) {
		/* otherwise sequencer animation isnt updated */
		BKE_animsys_evaluate_all_animation(G.main, (float)cfra); // XXX, was frame_to_float(re->scene, cfra)
	}

	recurs_depth++;

	ibuf= give_ibuf_seq(re->db.scene, rr->rectx, rr->recty, cfra, 0, 100.0);

	recurs_depth--;
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	if(ibuf) {
		if(ibuf->rect_float) {
			if (!rr->rectf)
				rr->rectf= MEM_mallocN(4*sizeof(float)*rr->rectx*rr->recty, "render_seq rectf");
			
			memcpy(rr->rectf, ibuf->rect_float, 4*sizeof(float)*rr->rectx*rr->recty);
			
			/* TSK! Since sequence render doesn't free the *rr render result, the old rect32
			   can hang around when sequence render has rendered a 32 bits one before */
			if(rr->rect32) {
				MEM_freeN(rr->rect32);
				rr->rect32= NULL;
			}
		}
		else if(ibuf->rect) {
			if (!rr->rect32)
				rr->rect32= MEM_mallocN(sizeof(int)*rr->rectx*rr->recty, "render_seq rect");

			memcpy(rr->rect32, ibuf->rect, 4*rr->rectx*rr->recty);

			/* if (ibuf->zbuf) { */
			/* 	if (re->rectz) freeN(re->rectz); */
			/* 	re->rectz = BLI_dupallocN(ibuf->zbuf); */
			/* } */
		}
		
		if (recurs_depth == 0) { /* with nested scenes, only free on toplevel... */
			Editing * ed = re->db.scene->ed;
			if (ed) {
				free_imbuf_seq(re->db.scene, &ed->seqbase, TRUE);
			}
		}
	}
	else {
		/* render result is delivered empty in most cases, nevertheless we handle all cases */
		if (rr->rectf)
			memset(rr->rectf, 0, 4*sizeof(float)*rr->rectx*rr->recty);
		else if (rr->rect32)
			memset(rr->rect32, 0, 4*rr->rectx*rr->recty);
		else
			rr->rect32= MEM_callocN(sizeof(int)*rr->rectx*rr->recty, "render_seq rect");
	}

	BLI_rw_mutex_unlock(&re->resultmutex);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* main loop: doing sequence + fields + blur + 3d render + compositing */
static void do_render_all_options(Render *re)
{
	scene_camera_switch_update(re->db.scene);

	re->cb.i.starttime= PIL_check_seconds_timer();

	/* ensure no images are in memory from previous animated sequences */
	BKE_image_all_free_anim_ibufs(re->params.r.cfra);
	
	if((re->params.r.scemode & R_DOSEQ) && re->db.scene->ed && re->db.scene->ed->seqbase.first) {
		/* note: do_render_seq() frees rect32 when sequencer returns float images */
		if(!re->cb.test_break(re->cb.tbh)) 
			do_render_seq(re);
		
		re->cb.stats_draw(re->cb.sdh, &re->cb.i);
		re->cb.display_draw(re->cb.ddh, re->result, NULL);
	}
	else {
		do_render_composite_fields_blur_3d(re);
	}
	
	/* for UI only */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	renderresult_add_names(re->result);
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	re->cb.i.lastframetime= PIL_check_seconds_timer()- re->cb.i.starttime;
	
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
	
	/* stamp image info here */
	if((re->params.r.stamp & R_STAMP_ALL) && (re->params.r.stamp & R_STAMP_DRAW)) {
		renderresult_stampinfo(re->db.scene);
		re->cb.display_draw(re->cb.ddh, re->result, NULL);
	}
}

static int is_rendering_allowed(Render *re)
{
	SceneRenderLayer *srl;
	
	/* forbidden combinations */
	if(re->cam.type == CAM_PANORAMA) {
		if(re->params.r.mode & R_BORDER) {
			re->cb.error(re->cb.erh, "No border supported for Panorama");
			return 0;
		}
		if(re->cam.type == CAM_ORTHO) {
			re->cb.error(re->cb.erh, "No Ortho render possible for Panorama");
			return 0;
		}
	}
	
	if(re->params.r.mode & R_BORDER) {
		if(re->params.r.border.xmax <= re->params.r.border.xmin || 
		   re->params.r.border.ymax <= re->params.r.border.ymin) {
			re->cb.error(re->cb.erh, "No border area selected.");
			return 0;
		}
	}
	
	if(re->params.r.scemode & (R_EXR_TILE_FILE|R_FULL_SAMPLE)) {
		char str[FILE_MAX];
		
		render_unique_exr_name(re, str, 0);
		
		if (BLI_is_writable(str)==0) {
			re->cb.error(re->cb.erh, "Can not save render buffers, check the temp default path");
			return 0;
		}
		
		/* no osa + fullsample won't work... */
		if(re->params.osa==0)
			re->params.r.scemode &= ~R_FULL_SAMPLE;
		
		/* no fullsample and edge */
		if((re->params.r.scemode & R_FULL_SAMPLE) && (re->params.r.mode & R_EDGE)) {
			re->cb.error(re->cb.erh, "Full Sample doesn't support Edge Enhance");
			return 0;
		}
		
	}
	else
		re->params.r.scemode &= ~R_FULL_SAMPLE;	/* clear to be sure */
	
	if(re->params.r.scemode & R_DOCOMP) {
		if(re->db.scene->use_nodes) {
			bNodeTree *ntree= re->db.scene->nodetree;
			bNode *node;
		
			if(ntree==NULL) {
				re->cb.error(re->cb.erh, "No Nodetree in Scene");
				return 0;
			}
			
			for(node= ntree->nodes.first; node; node= node->next)
				if(node->type==CMP_NODE_COMPOSITE)
					break;
			
			
			if(node==NULL) {
				re->cb.error(re->cb.erh, "No Render Output Node in Scene");
				return 0;
			}
		}
	}
	
 	/* check valid camera, without camera render is OK (compo, seq) */
	if(re->db.scene->camera==NULL)
		re->db.scene->camera= scene_find_camera(re->db.scene);
	
	if(!(re->params.r.scemode & (R_DOSEQ|R_DOCOMP))) {
		if(re->db.scene->camera==NULL) {
			re->cb.error(re->cb.erh, "No camera");
			return 0;
		}
	}
	
	/* layer flag tests */
	if(re->params.r.scemode & R_SINGLE_LAYER) {
		srl= BLI_findlink(&re->db.scene->r.layers, re->params.r.actlay);
		/* force layer to be enabled */
		srl->layflag &= ~SCE_LAY_DISABLE;
	}
	
	for(srl= re->db.scene->r.layers.first; srl; srl= srl->next)
		if(!(srl->layflag & SCE_LAY_DISABLE))
			break;
	if(srl==NULL) {
		re->cb.error(re->cb.erh, "All RenderLayers are disabled");
		return 0;
	}
	
	/* renderer */
	if(!ELEM(re->params.r.renderer, R_INTERN, R_YAFRAY)) {
		re->cb.error(re->cb.erh, "Unknown render engine set");
		return 0;
	}
	return 1;
}

static void update_physics_cache(Render *re, Scene *scene, int anim_init)
{
	PTCacheBaker baker;

	baker.scene = scene;
	baker.pid = NULL;
	baker.bake = 0;
	baker.render = 1;
	baker.anim_init = 1;
	baker.quick_step = 1;
	baker.break_test = re->cb.test_break;
	baker.break_data = re->cb.tbh;
	baker.progressbar = NULL;

	BKE_ptcache_make_cache(&baker);
}
/* evaluating scene options for general Blender render */
static int render_initialize_from_scene(Render *re, Scene *scene, SceneRenderLayer *srl, int anim, int anim_init)
{
	int winx, winy;
	rcti disprect;
	
	/* r.xsch and r.ysch has the actual view window size
		r.border is the clipping rect */
	
	/* calculate actual render result and display size */
	winx= (scene->r.size*scene->r.xsch)/100;
	winy= (scene->r.size*scene->r.ysch)/100;
	
	/* we always render smaller part, inserting it in larger image is compositor bizz, it uses disprect for it */
	if(scene->r.mode & R_BORDER) {
		disprect.xmin= scene->r.border.xmin*winx;
		disprect.xmax= scene->r.border.xmax*winx;
		
		disprect.ymin= scene->r.border.ymin*winy;
		disprect.ymax= scene->r.border.ymax*winy;
	}
	else {
		disprect.xmin= disprect.ymin= 0;
		disprect.xmax= winx;
		disprect.ymax= winy;
	}
	
	re->db.scene= scene;
	
	/* not too nice, but it survives anim-border render */
	if(anim) {
		re->disprect= disprect;
		return 1;
	}
	
	/* check all scenes involved */
	tag_scenes_for_render(re);

	/*
	 * Disabled completely for now,
	 * can be later set as render profile option
	 * and default for background render.
	*/
	if(0) {
		/* make sure dynamics are up to date */
		update_physics_cache(re, scene, anim_init);
	}
	
	if(srl || scene->r.scemode & R_SINGLE_LAYER)
		push_render_result(re);
	
	RE_InitState(re, NULL, &scene->r, srl, winx, winy, &disprect);
	if(!re->ok)  /* if an error was printed, abort */
		return 0;
	
	/* initstate makes new result, have to send changed tags around */
	ntreeCompositTagRender(re->db.scene);
	
	if(!is_rendering_allowed(re))
		return 0;
	
	re->cb.display_init(re->cb.dih, re->result);
	re->cb.display_clear(re->cb.dch, re->result);
	
	return 1;
}

/* general Blender frame render call */
void RE_BlenderFrame(Render *re, Scene *scene, SceneRenderLayer *srl, int frame)
{
	/* ugly global still... is to prevent preview events and signal subsurfs etc to make full resol */
	RenderGlobal.renderingslot= re->slot;
	re->result_ok= 0;
	G.rendering= 1;
	
	scene->r.cfra= frame;
	
	if(render_initialize_from_scene(re, scene, srl, 0, 0)) {
		do_render_all_options(re);
	}
	
	/* UGLY WARNING */
	re->result_ok= 1;
	G.rendering= 0;
	RenderGlobal.renderingslot= RenderGlobal.viewslot;
}

static int do_write_image_or_movie(Render *re, Scene *scene, bMovieHandle *mh, ReportList *reports)
{
	char name[FILE_MAX];
	RenderResult rres;
	int ok= 1;
	
	RE_AcquireResultImage(re, &rres);

	/* write movie or image */
	if(BKE_imtype_is_movie(scene->r.imtype)) {
		int dofree = 0;
		/* note; the way it gets 32 bits rects is weak... */
		if(rres.rect32==NULL) {
			rres.rect32= MEM_mapallocN(sizeof(int)*rres.rectx*rres.recty, "temp 32 bits rect");
			dofree = 1;
		}
		RE_ResultGet32(re, (unsigned int *)rres.rect32);
		ok= mh->append_movie(&re->params.r, scene->r.cfra, rres.rect32, rres.rectx, rres.recty, reports);
		if(dofree) {
			MEM_freeN(rres.rect32);
		}
		printf("Append frame %d", scene->r.cfra);
	} 
	else {
		BKE_makepicstring(name, scene->r.pic, scene->r.cfra, scene->r.imtype, scene->r.scemode & R_EXTENSION);
		
		if(re->params.r.imtype==R_MULTILAYER) {
			if(re->result) {
				RE_WriteRenderResult(re->result, name, scene->r.quality);
				printf("Saved: %s", name);
			}
		}
		else {
			ImBuf *ibuf= IMB_allocImBuf(rres.rectx, rres.recty, scene->r.planes, 0, 0);
			
			/* if not exists, BKE_write_ibuf makes one */
			ibuf->rect= (unsigned int *)rres.rect32;    
			ibuf->rect_float= rres.rectf;
			ibuf->zbuf_float= rres.rectz;
			
			/* float factor for random dither, imbuf takes care of it */
			ibuf->dither= scene->r.dither_intensity;
			
			/* prepare to gamma correct to sRGB color space */
			if (scene->r.color_mgt_flag & R_COLOR_MANAGEMENT) {
				/* sequence editor can generate 8bpc render buffers */
				if (ibuf->rect) {
					ibuf->profile = IB_PROFILE_SRGB;
					if (ELEM(scene->r.imtype, R_OPENEXR, R_RADHDR))
						IMB_float_from_rect(ibuf);
				} else {				
					ibuf->profile = IB_PROFILE_LINEAR_RGB;
				}
			}

			ok= BKE_write_ibuf(scene, ibuf, name, scene->r.imtype, scene->r.subimtype, scene->r.quality);
			
			if(ok==0) {
				printf("Render error: cannot save %s\n", name);
			}
			else printf("Saved: %s", name);
			
			/* optional preview images for exr */
			if(ok && scene->r.imtype==R_OPENEXR && (scene->r.subimtype & R_PREVIEW_JPG)) {
				if(BLI_testextensie(name, ".exr")) 
					name[strlen(name)-4]= 0;
				BKE_add_image_extension(name, R_JPEG90);
				ibuf->depth= 24; 
				BKE_write_ibuf(scene, ibuf, name, R_JPEG90, scene->r.subimtype, scene->r.quality);
				printf("\nSaved: %s", name);
			}
			
					/* imbuf knows which rects are not part of ibuf */
			IMB_freeImBuf(ibuf);
		}
	}
	
	RE_ReleaseResultImage(re);

	BLI_timestr(re->cb.i.lastframetime, name);
	printf(" Time: %s\n", name);
	fflush(stdout); /* needed for renderd !! (not anymore... (ton)) */

	return ok;
}

/* saves images to disk */
void RE_BlenderAnim(Render *re, Scene *scene, int sfra, int efra, int tfra, ReportList *reports)
{
	bMovieHandle *mh= BKE_get_movie_handle(scene->r.imtype);
	unsigned int lay;
	int cfrao= scene->r.cfra;
	int nfra;
	
	/* do not fully call for each frame, it initializes & pops output window */
	if(!render_initialize_from_scene(re, scene, NULL, 0, 1))
		return;
	
	/* ugly global still... is to prevent renderwin events and signal subsurfs etc to make full resol */
	/* is also set by caller renderwin.c */
	G.rendering= 1;
	RenderGlobal.renderingslot= re->slot;
	re->result_ok= 0;
	
	if(BKE_imtype_is_movie(scene->r.imtype))
		if(!mh->start_movie(scene, &re->params.r, re->rectx, re->recty, reports))
			G.afbreek= 1;

	if (mh->get_next_frame) {
		while (!(G.afbreek == 1)) {
			int nf = mh->get_next_frame(&re->params.r, reports);
			if (nf >= 0 && nf >= scene->r.sfra && nf <= scene->r.efra) {
				scene->r.cfra = re->params.r.cfra = nf;
				
				do_render_all_options(re);

				if(re->cb.test_break(re->cb.tbh) == 0) {
					if(!do_write_image_or_movie(re, scene, mh, reports))
						G.afbreek= 1;
				}
			} else {
				if(re->cb.test_break(re->cb.tbh))
					G.afbreek= 1;
			}
		}
	} else {
		for(nfra= sfra, scene->r.cfra= sfra; scene->r.cfra<=efra; scene->r.cfra++) {
			char name[FILE_MAX];
			
			/* only border now, todo: camera lens. (ton) */
			render_initialize_from_scene(re, scene, NULL, 1, 0);

			if(nfra!=scene->r.cfra) {
				/*
				 * Skip this frame, but update for physics and particles system.
				 * From convertblender.c:
				 * in localview, lamps are using normal layers, objects only local bits.
				 */
				if(scene->lay & 0xFF000000)
					lay= scene->lay & 0xFF000000;
				else
					lay= scene->lay;

				scene_update_for_newframe(scene, lay);
				continue;
			}
			else
				nfra+= tfra;

			/* Touch/NoOverwrite options are only valid for image's */
			if(BKE_imtype_is_movie(scene->r.imtype) == 0) {
				if(scene->r.mode & (R_NO_OVERWRITE | R_TOUCH))
					BKE_makepicstring(name, scene->r.pic, scene->r.cfra, scene->r.imtype, scene->r.scemode & R_EXTENSION);

				if(scene->r.mode & R_NO_OVERWRITE && BLI_exist(name)) {
					printf("skipping existing frame \"%s\"\n", name);
					continue;
				}
				if(scene->r.mode & R_TOUCH && !BLI_exist(name)) {
					BLI_make_existing_file(name); /* makes the dir if its not there */
					BLI_touch(name);
				}
			}

			re->params.r.cfra= scene->r.cfra;	   /* weak.... */
			
			do_render_all_options(re);
			
			if(re->cb.test_break(re->cb.tbh) == 0) {
				if(!G.afbreek)
					if(!do_write_image_or_movie(re, scene, mh, reports))
						G.afbreek= 1;
			}
			else
				G.afbreek= 1;
		
			if(G.afbreek==1) {
				/* remove touched file */
				if(BKE_imtype_is_movie(scene->r.imtype) == 0) {
					if (scene->r.mode & R_TOUCH && BLI_exist(name) && BLI_filepathsize(name) == 0) {
						BLI_delete(name, 0, 0);
					}
				}
				
				break;
			}
		}
	}
	
	/* end movie */
	if(BKE_imtype_is_movie(scene->r.imtype))
		mh->end_movie();

	scene->r.cfra= cfrao;
	
	/* UGLY WARNING */
	G.rendering= 0;
	re->result_ok= 1;
	RenderGlobal.renderingslot= RenderGlobal.viewslot;
}

/* note; repeated win/disprect calc... solve that nicer, also in compo */

/* only the temp file! */
void RE_ReadRenderResult(Scene *scene, Scene *scenode)
{
	Render *re;
	int winx, winy;
	rcti disprect;
	
	/* calculate actual render result and display size */
	winx= (scene->r.size*scene->r.xsch)/100;
	winy= (scene->r.size*scene->r.ysch)/100;
	
	/* only in movie case we render smaller part */
	if(scene->r.mode & R_BORDER) {
		disprect.xmin= scene->r.border.xmin*winx;
		disprect.xmax= scene->r.border.xmax*winx;
		
		disprect.ymin= scene->r.border.ymin*winy;
		disprect.ymax= scene->r.border.ymax*winy;
	}
	else {
		disprect.xmin= disprect.ymin= 0;
		disprect.xmax= winx;
		disprect.ymax= winy;
	}
	
	if(scenode)
		scene= scenode;
	
	/* get render: it can be called from UI with draw callbacks */
	re= RE_GetRender(scene->id.name, RE_SLOT_VIEW);
	if(re==NULL)
		re= RE_NewRender(scene->id.name, RE_SLOT_VIEW);
	RE_InitState(re, NULL, &scene->r, NULL, winx, winy, &disprect);
	re->db.scene= scene;
	
	render_result_read(re, 0);
}

void RE_set_max_threads(int threads)
{
	if(threads==0)
		RenderGlobal.threads = BLI_system_thread_count();
	else if(threads>=1 && threads<=BLENDER_MAX_THREADS)
		RenderGlobal.threads= threads;
	else
		printf("Error, threads has to be in range 0-%d\n", BLENDER_MAX_THREADS);
}

void RE_init_threadcount(Render *re) 
{
	if(RenderGlobal.threads >= 1) /* only set as an arg in background mode */
		re->params.r.threads= MIN2(RenderGlobal.threads, BLENDER_MAX_THREADS);
	else if((re->params.r.mode & R_FIXED_THREADS)==0 || RenderGlobal.threads == 0) /* Automatic threads */
		re->params.r.threads = BLI_system_thread_count();
}

/************************** External Engines ***************************/

RenderResult *RE_engine_begin_result(RenderEngine *engine, int x, int y, int w, int h)
{
	Render *re= engine->re;
	RenderResult *result;
	rcti disprect;

	/* ensure the coordinates are within the right limits */
	CLAMP(x, 0, re->result->rectx);
	CLAMP(y, 0, re->result->recty);
	CLAMP(w, 0, re->result->rectx);
	CLAMP(h, 0, re->result->recty);

	if(x + w > re->result->rectx)
		w= re->result->rectx - x;
	if(y + h > re->result->recty)
		h= re->result->recty - y;

	/* allocate a render result */
	disprect.xmin= x;
	disprect.xmax= x+w;
	disprect.ymin= y;
	disprect.ymax= y+h;

	if(0) { // XXX (re->params.r.scemode & R_FULL_SAMPLE)) {
		result= new_full_sample_buffers(re, &engine->fullresult, &disprect, 0);
	}
	else {
		result= render_result_create(re, &disprect, 0, RR_USEMEM);
		BLI_addtail(&engine->fullresult, result);
	}

	return result;
}

void RE_engine_update_result(RenderEngine *engine, RenderResult *result)
{
	Render *re= engine->re;

	if(result && render_display_draw_enabled(re)) {
		result->renlay= result->layers.first; // weak
		re->cb.display_draw(re->cb.ddh, result, NULL);
	}
}

void RE_engine_end_result(RenderEngine *engine, RenderResult *result)
{
	Render *re= engine->re;

	if(!result)
		return;

	// XXX crashes with full sample, exr expects very particular part sizes
	render_result_merge_part(re, result);

	/* draw */
	if(!re->cb.test_break(re->cb.tbh) && render_display_draw_enabled(re)) {
		result->renlay= result->layers.first; // weak
		re->cb.display_draw(re->cb.ddh, result, NULL);
	}

	/* free */
	render_result_free(&engine->fullresult, result);
}

int RE_engine_test_break(RenderEngine *engine)
{
	Render *re= engine->re;

	return re->cb.test_break(re->cb.tbh);
}

void RE_engine_update_stats(RenderEngine *engine, char *stats, char *info)
{
	Render *re= engine->re;

	re->cb.i.statstr= stats;
	re->cb.i.infostr= info;
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
	re->cb.i.infostr= NULL;
	re->cb.i.statstr= NULL;
}

/* loads in image into a result, size must match
 * x/y offsets are only used on a partial copy when dimensions dont match */
void RE_layer_load_from_file(RenderLayer *layer, ReportList *reports, char *filename)
{
	ImBuf *ibuf = IMB_loadiffname(filename, IB_rect);

	if(ibuf  && (ibuf->rect || ibuf->rect_float)) {
		if (ibuf->x == layer->rectx && ibuf->y == layer->recty) {
			if(ibuf->rect_float==NULL)
				IMB_float_from_rect(ibuf);

			memcpy(layer->rectf, ibuf->rect_float, sizeof(float)*4*layer->rectx*layer->recty);
		} else {
			if ((ibuf->x >= layer->rectx) && (ibuf->y >= layer->recty)) {
				ImBuf *ibuf_clip;

				if(ibuf->rect_float==NULL)
					IMB_float_from_rect(ibuf);

				ibuf_clip = IMB_allocImBuf(layer->rectx, layer->recty, 32, IB_rectfloat, 0);
				if(ibuf_clip) {
					IMB_rectcpy(ibuf_clip, ibuf, 0,0, 0,0, layer->rectx, layer->recty);

					memcpy(layer->rectf, ibuf_clip->rect_float, sizeof(float)*4*layer->rectx*layer->recty);
					IMB_freeImBuf(ibuf_clip);
				}
				else {
					BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: failed to allocate clip buffer '%s'\n", filename);
				}
			}
			else {
				BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: incorrect dimensions for partial copy '%s'\n", filename);
			}
		}

		IMB_freeImBuf(ibuf);
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: failed to load '%s'\n", filename);
	}
}

void RE_result_load_from_file(RenderResult *result, ReportList *reports, char *filename)
{
	if(!render_result_read_from_file(filename, result)) {
		BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: failed to load '%s'\n", filename);
		return;
	}
}

static void external_render_3d(Render *re, RenderEngineType *type)
{
	RenderEngine engine;

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	if(re->result==NULL || !(re->params.r.scemode & R_PREVIEWBUTS)) {
		RE_FreeRenderResult(re->result);
	
		if(0) // XXX re->params.r.scemode & R_FULL_SAMPLE)
			re->result= render_result_full_sample_create(re);
		else
			re->result= render_result_create(re, &re->disprect, 0, 0); // XXX re->params.r.scemode & (R_EXR_TILE_FILE|R_FULL_SAMPLE));
	}
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	if(re->result==NULL)
		return;

	/* external */
	memset(&engine, 0, sizeof(engine));
	engine.type= type;
	engine.re= re;

	type->render(&engine, re->db.scene);

	render_result_free(&engine.fullresult, engine.fullresult.first);

	if(re->result->exrhandle)
		render_result_exr_read(re);
	
	{
		extern void stupid();
		stupid();
	}
}

