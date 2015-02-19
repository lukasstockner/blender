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
 * The Original Code is Copyright (C) 2004-2005 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_pointcache_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_POINTCACHE_TYPES_H__
#define __DNA_POINTCACHE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_listBase.h" /* XXX only needed for deprecated PTCacheMem, remove once that is replaced */

/* XXX TODO point cache do_versions
 * This needs to be updated until officially included in master
 */
#define PTCACHE_DO_VERSIONS(main) MAIN_VERSION_ATLEAST(main, 269, 6)

typedef struct CacheLibrary {
	ID id;
	
	char filepath[1024]; /* 1024 = FILE_MAX */
} CacheLibrary;

/* Point cache file data types:
 * - used as (1<<flag) so poke jahka if you reach the limit of 15
 * - to add new data types update:
 *		* BKE_ptcache_data_size()
 *		* ptcache_file_init_pointers()
 */
#define BPHYS_DATA_INDEX		0
#define BPHYS_DATA_LOCATION		1
#define BPHYS_DATA_SMOKE_LOW	1
#define BPHYS_DATA_VELOCITY		2
#define BPHYS_DATA_SMOKE_HIGH	2
#define BPHYS_DATA_ROTATION		3
#define BPHYS_DATA_DYNAMICPAINT 3
#define BPHYS_DATA_AVELOCITY	4	/* used for particles */
#define BPHYS_DATA_XCONST		4	/* used for cloth */
#define BPHYS_DATA_SIZE			5
#define BPHYS_DATA_TIMES		6
#define BPHYS_DATA_BOIDS		7

#define BPHYS_TOT_DATA			8

#define BPHYS_EXTRA_FLUID_SPRINGS	1

typedef struct PTCacheExtra {
	struct PTCacheExtra *next, *prev;
	unsigned int type, totdata;
	void *data;
} PTCacheExtra;

typedef struct PTCacheMem {
	struct PTCacheMem *next, *prev;
	unsigned int frame, totpoint;
	unsigned int data_types, flag;

	void *data[8]; /* BPHYS_TOT_DATA */
	void *cur[8]; /* BPHYS_TOT_DATA */

	struct ListBase extradata;
} PTCacheMem;

typedef struct PointCacheState {
	int flag;

	int simframe;	/* current frame of simulation (only if SIMULATION_VALID) */
	int last_exact; /* last exact frame that's cached */
	int last_valid; /* used for editing cache - what is the last baked frame */

	/* for external cache files */
	int totpoint;   /* number of cached points */
	int pad;

	char info[64];
	char *cached_frames;	/* array of length endframe-startframe+1 with flags to indicate cached frames */
							/* can be later used for other per frame flags too if needed */
} PointCacheState;

typedef enum ePointCacheStateFlag {
	PTC_STATE_OUTDATED			= 1,
	/* XXX remove BAKING flag! only used for overriding display percentage in particles
	 * to cache data with full particle amount. This should be based on some contextual info,
	 * not a flag in the cache state
	 */
	PTC_STATE_BAKING			= 2,
	PTC_STATE_FRAMES_SKIPPED	= 4,
	PTC_STATE_READ_INFO			= 8,
	PTC_STATE_REDO_NEEDED		= PTC_STATE_OUTDATED | PTC_STATE_FRAMES_SKIPPED,

	/* high resolution cache is saved for smoke for backwards compatibility, so set this flag to know it's a "fake" cache */
	/* XXX compatibility flag, remove later */
	PTC_STATE_FAKE_SMOKE		= (1<<16)
} ePointCacheStateFlag;

typedef struct PointCache {
	int flag;		/* generic flag */
	
	int step;		/* The number of frames between cached frames.
					 * This should probably be an upper bound for a per point adaptive step in the future,
					 * buf for now it's the same for all points. Without adaptivity this can effect the perceived
					 * simulation quite a bit though. If for example particles are colliding with a horizontal
					 * plane (with high damping) they quickly come to a stop on the plane, however there are still
					 * forces acting on the particle (gravity and collisions), so the particle velocity isn't necessarily
					 * zero for the whole duration of the frame even if the particle seems stationary. If all simulation
					 * frames aren't cached (step > 1) these velocities are interpolated into movement for the non-cached
					 * frames. The result will look like the point is oscillating around the collision location. So for
					 * now cache step should be set to 1 for accurate reproduction of collisions.
					 */

	int startframe;	/* simulation start frame */
	int endframe;	/* simulation end frame */
	int editframe;	/* frame being edited (runtime only) */

	/* for external cache files */
	int index;	/* modifier stack index */
	short compression, rt;
	int pad;
	
	char name[64];
	char prev_name[64];
	char path[1024]; /* file path, 1024 = FILE_MAX */

	PointCacheState state;

	struct PTCacheEdit *edit;
	void (*free_edit)(struct PTCacheEdit *edit);	/* free callback */

	/**** NEW POINTCACHE ****/
	struct CacheLibrary *cachelib;
} PointCache;

typedef enum ePointCacheFlag {
	PTC_EXTERNAL				= 1,
	/* don't use the filename of the blendfile the data is linked from (write a local cache) */
	PTC_IGNORE_LIBPATH		= 2,
	PTC_IGNORE_CLEAR		= 4,
	/* lock the simulation settings and use the cache read-only
	 * XXX this should perhaps be moved out of the cache entirely,
	 * but for now provides a flag for keeping UI functionality
	 */
	PTC_LOCK_SETTINGS		= 8
} ePointCacheFlag;

typedef enum ePointCacheCompression {
	PTC_COMPRESS_NO			= 0,
	PTC_COMPRESS_LZO		= 1,
	PTC_COMPRESS_LZMA		= 2
} ePointCacheCompression;

/* pointcache->flag */
#define _PTCACHE_BAKED_DEPRECATED				1
#define _PTCACHE_OUTDATED_DEPRECATED			2
#define _PTCACHE_SIMULATION_VALID_DEPRECATED	4
#define _PTCACHE_BAKING_DEPRECATED				8
//#define PTCACHE_BAKE_EDIT						16
//#define PTCACHE_BAKE_EDIT_ACTIVE				32
//#define PTCACHE_DISK_CACHE					64 /* DEPRECATED all caches are disk-based now (with optional packing in blend files) */
//#define PTCACHE_QUICK_CACHE					128  /* removed since 2.64 - [#30974], could be added back in a more useful way */
#define _PTCACHE_FRAMES_SKIPPED_DEPRECATED		256
#define _PTCACHE_EXTERNAL_DEPRECATED			512
#define _PTCACHE_READ_INFO_DEPRECATED			1024
/* don't use the filename of the blendfile the data is linked from (write a local cache) */
#define _PTCACHE_IGNORE_LIBPATH_DEPRECATED		2048
/* high resolution cache is saved for smoke for backwards compatibility, so set this flag to know it's a "fake" cache */
#define _PTCACHE_FAKE_SMOKE_DEPRECATED			(1<<12)
#define _PTCACHE_IGNORE_CLEAR_DEPRECATED		(1<<13)

/* PTCACHE_OUTDATED + PTCACHE_FRAMES_SKIPPED */
#define _PTCACHE_REDO_NEEDED_DEPRECATED			258

typedef enum ePointCacheArchive_Type {
	PTC_ARCHIVE_BPHYS		= 0,
	PTC_ARCHIVE_ALEMBIC		= 1,
} ePointCacheArchive_Type;

#endif

