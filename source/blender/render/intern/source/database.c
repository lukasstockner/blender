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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_memarena.h"

#include "cache.h"
#include "database.h"
#include "diskocclusion.h"
#include "lamp.h"
#include "object.h"
#include "raytrace.h"
#include "render_types.h"
#include "rendercore.h"
#include "sss.h"
#include "object_strand.h"
#include "volume_precache.h"

/******************************** Database ***********************************/

void render_db_init(RenderDB *rdb)
{
	rdb->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);

	rdb->totvlak= 0;
	rdb->totvert= 0;
	rdb->totstrand= 0;
	rdb->totlamp= 0;
	rdb->tothalo= 0;

	rdb->lights.first= rdb->lights.last= NULL;
	rdb->lampren.first= rdb->lampren.last= NULL;
}

void render_db_free(RenderDB *rdb)
{
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	LampRen *lar;
	
	/* rendering structures */
	sss_free(rdb);
	disk_occlusion_free(rdb);
	raytree_free(rdb);
	surface_cache_free(rdb);
	volume_precache_free(rdb);
	BLI_freelistN(&rdb->render_volumes_inside);

	/* objects and instances */
	for(obr=rdb->objecttable.first; obr; obr=obr->next)
		render_object_free(obr);

	if(rdb->objectinstance) {
		for(obi=rdb->instancetable.first; obi; obi=obi->next)
			render_instance_free(obi);

		MEM_freeN(rdb->objectinstance);
		rdb->objectinstance= NULL;
		rdb->totinstance= 0;
		rdb->instancetable.first= rdb->instancetable.last= NULL;
	}

	if(rdb->sortedhalos) {
		MEM_freeN(rdb->sortedhalos);
		rdb->sortedhalos= NULL;
	}

	BLI_freelistN(&rdb->customdata_names);
	BLI_freelistN(&rdb->objecttable);
	BLI_freelistN(&rdb->instancetable);

	if(rdb->orco_hash) {
		BLI_ghash_free(rdb->orco_hash, NULL, (GHashValFreeFP)MEM_freeN);
		rdb->orco_hash = NULL;
	}

	/* lamps */
	for(lar= rdb->lampren.first; lar; lar= lar->next)
		lamp_free(lar);
	
	BLI_freelistN(&rdb->lampren);
	BLI_freelistN(&rdb->lights);

	/* memarea */
	if(rdb->memArena) {
		BLI_memarena_free(rdb->memArena);
		rdb->memArena = NULL;
	}

	/* clear counts */
	rdb->totvlak= 0;
	rdb->totvert= 0;
	rdb->totstrand= 0;
	rdb->totlamp= 0;
	rdb->tothalo= 0;
}

