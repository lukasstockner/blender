/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): (c) 2006 Blender Foundation, full refactor
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_TYPES_H__
#define __RENDER_TYPES_H__

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_threads.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"	/* TexResult, ShadeResult, ShadeInput */

#include "camera.h"
#include "object.h"

struct GHash;
struct IrrCache;
struct MemArena;
struct Object;
struct ObjectInstanceRen;
struct RadioCache;
struct RayFace;
struct RayObject;
struct VlakPrimitive;

#define TABLEINITSIZE 1024
#define LAMPINITSIZE 256

struct SampleTables;

typedef struct RenderDB {
	/* scene and world */
	struct Scene *scene;
	struct World wrld;

	/* objects */
	ListBase objecttable;

	/* object instances */
	struct ObjectInstanceRen *objectinstance;
	ListBase instancetable;
	int totinstance;

	/* extra object data */
	ListBase customdata_names;
	struct Object *excludeob;
	struct GHash *orco_hash;

	/* statistics */
	int totvlak, totvert, totstrand, totlamp;

	/* halos */
	struct HaloRen **sortedhalos;
	int tothalo;

	/* lamps */
	ListBase lights;	/* GroupObject pointers */
	ListBase lampren;	/* storage, for free */

	/* subsurface scattering */
	struct GHash *sss_hash;
	ListBase *sss_points;
	struct Material *sss_mat;

	/* raytracing */
	struct RayObject *raytree;
	struct RayFace *rayfaces;
	struct VlakPrimitive *rayprimitives;
	float maxdist; /* needed for keeping an incorrect behaviour of SUN and HEMI lights (avoid breaking old scenes) */

	/* approximate ao */
	void *occlusiontree;
	ListBase surfacecache;

	/* volume */
	ListBase render_volumes_inside;
	ListBase volumes;
	ListBase volume_precache_parts;

	/* memory pool for quick alloc */
	struct MemArena *memArena;

	/* irradiance cache for AO/env/indirect */
	struct IrrCache *irrcache[BLENDER_MAX_THREADS];
	struct RadioCache *radiocache[BLENDER_MAX_THREADS];
} RenderDB;

typedef struct RenderSampleData {
	/* samples */
	struct SampleTables *table;
	float jit[32][2];
	float mblur_jit[32][2];
	ListBase *qmcsamplers;
	
	/* shadow counter, detect shadow-reuse for shaders */
	int shadowsamplenr[BLENDER_MAX_THREADS];
} RenderSampleData;

typedef struct RenderCallbacks {
	/* callbacks */
	void (*display_init)(void *handle, RenderResult *rr);
	void *dih;
	void (*display_clear)(void *handle, RenderResult *rr);
	void *dch;
	void (*display_draw)(void *handle, RenderResult *rr, volatile rcti *rect);
	void *ddh;
	
	void (*stats_draw)(void *handle, RenderStats *ri);
	void *sdh;
	void (*timecursor)(void *handle, int i);
	void *tch;

	int (*test_break)(void *handle);
	void *tbh;
	
	void (*error)(void *handle, char *str);
	void *erh;
	
	RenderStats i;
} RenderCallbacks;

typedef struct RenderParams {
	/* full copy of scene->r */
	RenderData r;
	short osa, flag;
} RenderParams;

/* controls state of render, everything that's read-only during render stage */
struct Render {
	struct Render *next, *prev;
	char name[RE_MAXNAME];
	int slot;
	
	/* state settings */
	short ok, result_ok;

	RenderParams params;
	
	/* camera settings */
	RenderCamera cam;
	
	struct Image *bakebuf;

	/* database */
	RenderDB db;

	/* callbacks */
	RenderCallbacks cb;

	/* samplers */
	RenderSampleData sample;

	/* result of rendering */
	RenderResult *result;
	/* if render with single-layer option, other rendered layers are stored here */
	RenderResult *pushedresult;
	/* a list of RenderResults, for fullsample */
	ListBase fullresult;	
	/* read/write mutex, all internal code that writes to re->result must use a
	   write lock, all external code must use a read lock. internal code is assumed
	   to not conflict with writes, so no lock used for that */
	ThreadRWMutex resultmutex;
	
	/* window size, display rect, viewplane */
	rcti disprect;			/* part within camera winx winy */
	
	/* final picture width and height (within disprect) */
	int rectx, recty;
	
	/* real maximum amount of xparts/yparts after correction for minimum */
	int xparts, yparts;
	/* real maximum size of parts after correction for minimum 
	   partx*xparts can be larger than rectx, in that case last part is smaller */
	int partx, party;

	ListBase parts;
};

/* Defines */

/* R.r.mode flag is same as for renderdata */

/* R.flag */
#define R_ZTRA			1
#define R_HALO			2
#define R_SEC_FIELD		4
#define R_LAMPHALO		8
#define R_GLOB_NOPUNOFLIP	16
#define R_NEED_TANGENT	32
#define R_BAKE_TRACE	128
#define R_BAKING		256

#endif /* __RENDER_TYPES_H__ */

