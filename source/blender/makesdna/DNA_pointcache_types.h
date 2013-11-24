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

typedef struct PointCache {
	struct PointCache *next, *prev;
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

	int simframe;	/* current frame of simulation (only if SIMULATION_VALID) */
	int startframe;	/* simulation start frame */
	int endframe;	/* simulation end frame */
	int editframe;	/* frame being edited (runtime only) */
	int last_exact; /* last exact frame that's cached */
	int last_valid; /* used for editing cache - what is the last baked frame */
	int pad;

	/* for external cache files */
	int totpoint;   /* number of cached points */
	int index;	/* modifier stack index */
	short compression, rt;
	
	char name[64];
	char prev_name[64];
	char info[64];
	char path[1024]; /* file path, 1024 = FILE_MAX */
	char *cached_frames;	/* array of length endframe-startframe+1 with flags to indicate cached frames */
							/* can be later used for other per frame flags too if needed */
	struct ListBase mem_cache;

	struct PTCacheEdit *edit;
	void (*free_edit)(struct PTCacheEdit *edit);	/* free callback */

//	struct PTCWriter *writer;
//	struct PTCReader *reader;
} PointCache;

/* pointcache->flag */
#define PTCACHE_BAKED				1
#define PTCACHE_OUTDATED			2
#define PTCACHE_SIMULATION_VALID	4
#define PTCACHE_BAKING				8
//#define PTCACHE_BAKE_EDIT			16
//#define PTCACHE_BAKE_EDIT_ACTIVE	32
#define PTCACHE_DISK_CACHE			64
//#define PTCACHE_QUICK_CACHE		128  /* removed since 2.64 - [#30974], could be added back in a more useful way */
#define PTCACHE_FRAMES_SKIPPED		256
#define PTCACHE_EXTERNAL			512
#define PTCACHE_READ_INFO			1024
/* don't use the filename of the blendfile the data is linked from (write a local cache) */
#define PTCACHE_IGNORE_LIBPATH		2048
/* high resolution cache is saved for smoke for backwards compatibility, so set this flag to know it's a "fake" cache */
#define PTCACHE_FAKE_SMOKE			(1<<12)
#define PTCACHE_IGNORE_CLEAR		(1<<13)

/* PTCACHE_OUTDATED + PTCACHE_FRAMES_SKIPPED */
#define PTCACHE_REDO_NEEDED			258

#define PTCACHE_COMPRESS_NO			0
#define PTCACHE_COMPRESS_LZO		1
#define PTCACHE_COMPRESS_LZMA		2


/**** NEW POINTCACHE ****/

/* settings for point caches */
typedef struct PointCacheSettings {
	char cachedir[768];	/* FILE_MAXDIR length */
} PointCacheSettings;

#endif

