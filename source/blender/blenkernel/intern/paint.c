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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BKE_utildefines.h"
#include "BKE_brush.h"
#include "BKE_DerivedMesh.h"
#include "BKE_dmgrid.h"
#include "BKE_library.h"
#include "BKE_paint.h"
#include "BKE_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_pbvh.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char PAINT_CURSOR_SCULPT[3] = {255, 100, 100};
const char PAINT_CURSOR_VERTEX_PAINT[3] = {255, 255, 255};
const char PAINT_CURSOR_WEIGHT_PAINT[3] = {200, 200, 255};
const char PAINT_CURSOR_TEXTURE_PAINT[3] = {255, 255, 255};

Paint *paint_get_active(Scene *sce)
{
	if(sce) {
		ToolSettings *ts = sce->toolsettings;
		
		if(sce->basact && sce->basact->object) {
			switch(sce->basact->object->mode) {
			case OB_MODE_SCULPT:
				return &ts->sculpt->paint;
			case OB_MODE_VERTEX_PAINT:
				return &ts->vpaint->paint;
			case OB_MODE_WEIGHT_PAINT:
				return &ts->wpaint->paint;
			case OB_MODE_TEXTURE_PAINT:
				return &ts->imapaint.paint;
			}
		}

		/* default to image paint */
		return &ts->imapaint.paint;
	}

	return NULL;
}

Brush *paint_brush(Paint *p)
{
	return p ? p->brush : NULL;
}

void paint_brush_set(Paint *p, Brush *br)
{
	if(p)
		p->brush= br;
}

int paint_facesel_test(Object *ob)
{
	return (ob && ob->type==OB_MESH && ob->data && (((Mesh *)ob->data)->editflag & ME_EDIT_PAINT_MASK) && (ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT)));
}

void paint_init(Paint *p, const char col[3])
{
	Brush *brush;

	/* If there's no brush, create one */
	brush = paint_brush(p);
	if(brush == NULL)
		brush= add_brush("Brush");
	paint_brush_set(p, brush);

	memcpy(p->paint_cursor_col, col, 3);
	p->paint_cursor_col[3] = 128;

	p->flags |= PAINT_SHOW_BRUSH;
}

void free_paint(Paint *UNUSED(paint))
{
	/* nothing */
}

void copy_paint(Paint *src, Paint *tar)
{
	tar->brush= src->brush;
}

/* Update the mask without doing a full object recalc */
void paint_refresh_mask_display(Object *ob)
{
	if(ob && ob->paint && ob->paint->pbvh) {
		BLI_pbvh_search_callback(ob->paint->pbvh, NULL, NULL,
					 BLI_pbvh_node_set_flags,
					 SET_INT_IN_POINTER(PBVH_UpdateColorBuffers));
	}
}


float paint_mask_from_gridelem(DMGridData *elem, GridKey *gridkey,
			      CustomData *vdata)
{
	CustomDataLayer *cdl;
	float mask = 0;
	int i, ndx;

	for(i=0; i < gridkey->mask; ++i) {
		ndx = CustomData_get_named_layer_index(vdata,
						       CD_PAINTMASK,
						       gridkey->mask_names[i]);
		cdl = &vdata->layers[ndx];

		if(!(cdl->flag & CD_FLAG_ENABLED))
			continue;

		mask += GRIDELEM_MASK(elem, gridkey)[i] * cdl->strength;
	}

	CLAMP(mask, 0, 1);

	return mask;
}

float paint_mask_from_vertex(CustomData *vdata, int vertex_index,
			    int pmask_totlayer, int pmask_first_layer)
{
	float mask = 0;
	int i;

	for(i = 0; i < pmask_totlayer; ++i) {
		CustomDataLayer *cdl= vdata->layers + pmask_first_layer + i;

		if(!(cdl->flag & CD_FLAG_ENABLED))
			continue;

		mask +=	((float*)cdl->data)[vertex_index] * cdl->strength;
	}

	CLAMP(mask, 0, 1);

	return mask;
}

void create_paintsession(Object *ob)
{
	if(ob->paint)
		free_paintsession(ob);

	ob->paint = MEM_callocN(sizeof(PaintSession), "PaintSession");
}

static void free_sculptsession(PaintSession *ps)
{
	if(ps && ps->sculpt) {
		SculptSession *ss = ps->sculpt;

		BLI_freelistN(&ss->hidden_areas);

		if(ss->texcache)
			MEM_freeN(ss->texcache);

		if(ss->layer_co)
			MEM_freeN(ss->layer_co);

		MEM_freeN(ss);

		ps->sculpt = NULL;
	}
}

void free_paintsession(Object *ob)
{
	if(ob && ob->paint) {
		PaintSession *ps = ob->paint;
		DerivedMesh *dm= ob->derivedFinal;

		free_sculptsession(ps);

		if(ps->pbvh)
			BLI_pbvh_free(ps->pbvh);

		if(dm && dm->getPBVH)
			dm->getPBVH(NULL, dm); /* signal to clear PBVH */

		MEM_freeN(ps);
		ob->paint = NULL;
	}
}
