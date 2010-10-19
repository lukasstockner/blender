/**
 * $Id$
 *
 * Undo system for painting and sculpting.
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BKE_mesh.h"
#include "BLI_string.h"

#include "BKE_utildefines.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_multires.h"

#include "ED_sculpt.h"

#include "paint_intern.h"

#define MAXUNDONAME	64

typedef struct UndoElem {
	struct UndoElem *next, *prev;
	char name[MAXUNDONAME];
	uintptr_t undosize;

	ListBase elems;

	UndoRestoreCb restore;
	UndoFreeCb free;
} UndoElem;

typedef struct UndoStack {
	int type;
	ListBase elems;
	UndoElem *current;
} UndoStack;

static UndoStack ImageUndoStack = {UNDO_PAINT_IMAGE, {NULL, NULL}, NULL};
static UndoStack MeshUndoStack = {UNDO_PAINT_MESH, {NULL, NULL}, NULL};

/* Generic */

static void undo_restore(bContext *C, UndoStack *UNUSED(stack), UndoElem *uel)
{
	if(uel && uel->restore)
		uel->restore(C, &uel->elems);
}

static void undo_elem_free(UndoStack *UNUSED(stack), UndoElem *uel)
{
	if(uel && uel->free) {
		uel->free(&uel->elems);
		BLI_freelistN(&uel->elems);
	}
}

static void undo_stack_push_begin(UndoStack *stack, const char *name, UndoRestoreCb restore, UndoFreeCb free)
{
	UndoElem *uel;
	int nr;
	
	/* Undo push is split up in begin and end, the reason is that as painting
	 * happens more tiles/nodes are added to the list, and at the very end we
	 * know how much memory the undo used to remove old undo elements */

	/* remove all undos after (also when stack->current==NULL) */
	while(stack->elems.last != stack->current) {
		uel= stack->elems.last;
		undo_elem_free(stack, uel);
		BLI_freelinkN(&stack->elems, uel);
	}
	
	/* make new */
	stack->current= uel= MEM_callocN(sizeof(UndoElem), "undo file");
	uel->restore= restore;
	uel->free= free;
	BLI_addtail(&stack->elems, uel);

	/* name can be a dynamic string */
	strncpy(uel->name, name, MAXUNDONAME-1);
	
	/* limit amount to the maximum amount*/
	nr= 0;
	uel= stack->elems.last;
	while(uel) {
		nr++;
		if(nr==U.undosteps) break;
		uel= uel->prev;
	}
	if(uel) {
		while(stack->elems.first!=uel) {
			UndoElem *first= stack->elems.first;
			undo_elem_free(stack, first);
			BLI_freelinkN(&stack->elems, first);
		}
	}
}

static void undo_stack_push_end(UndoStack *stack)
{
	UndoElem *uel;
	uintptr_t totmem, maxmem;

	if(U.undomemory != 0) {
		/* limit to maximum memory (afterwards, we can't know in advance) */
		totmem= 0;
		maxmem= ((uintptr_t)U.undomemory)*1024*1024;

		uel= stack->elems.last;
		while(uel) {
			totmem+= uel->undosize;
			if(totmem>maxmem) break;
			uel= uel->prev;
		}

		if(uel) {
			while(stack->elems.first!=uel) {
				UndoElem *first= stack->elems.first;
				undo_elem_free(stack, first);
				BLI_freelinkN(&stack->elems, first);
			}
		}
	}
}

static int undo_stack_step(bContext *C, UndoStack *stack, int step, const char *name)
{
	UndoElem *undo;

	if(step==1) {
		if(stack->current==NULL);
		else {
			if(!name || strcmp(stack->current->name, name) == 0) {
				if(G.f & G_DEBUG) printf("undo %s\n", stack->current->name);
				undo_restore(C, stack, stack->current);
				stack->current= stack->current->prev;
				return 1;
			}
		}
	}
	else if(step==-1) {
		if((stack->current!=NULL && stack->current->next==NULL) || stack->elems.first==NULL);
		else {
			if(!name || strcmp(stack->current->name, name) == 0) {
				undo= (stack->current && stack->current->next)? stack->current->next: stack->elems.first;
				undo_restore(C, stack, undo);
				stack->current= undo;
				if(G.f & G_DEBUG) printf("redo %s\n", undo->name);
				return 1;
			}
		}
	}

	return 0;
}

static void undo_stack_free(UndoStack *stack)
{
	UndoElem *uel;
	
	for(uel=stack->elems.first; uel; uel=uel->next)
		undo_elem_free(stack, uel);

	BLI_freelistN(&stack->elems);
	stack->current= NULL;
}

/**** paint undo for layers (mcol, paintmask) ****/

typedef enum {
	LAYER_ADDED,
	LAYER_REMOVED
} PaintLayerUndoOp;

struct PaintLayerUndoNode {
	struct PaintLayerUndoNode *next, *prev;

	/* customdata type */
	int type;
	/* add/remove */
	PaintLayerUndoOp op;
	/* only for restoring into its original location */
	int layer_offset;
	/* for identifying layer, don't use layer_offset for that */
	char layer_name[32];
	/* copy of a removed layer's data */
	void *layer_data;
	void **multires_layer_data;
	float strength;
	/* length of multires_layer_data array */
	int totface;
	/* whether layer has multires data */
	int flag_multires;
};

void paint_layer_undo_set_add(PaintLayerUndoNode *unode, char *name)
{
	unode->op = LAYER_ADDED;

	/* check for restore */
	if(unode->layer_name != name) {
		BLI_strncpy(unode->layer_name, name,
			    sizeof(unode->layer_name));
	}

	unode->totface = 0;
}

void paint_layer_undo_set_remove(PaintLayerUndoNode *unode, char *name,
				 CustomData *data, CustomData *fdata,
				 int totvert, int totface)
{
	CustomDataMultires *cdm;
	int ndx;

	unode->op = LAYER_REMOVED;
	/* check for restore */
	if(unode->layer_name != name) {
		BLI_strncpy(unode->layer_name, name,
			    sizeof(unode->layer_name));
	}

	unode->totface = totface;

	ndx = CustomData_get_named_layer_index(data, unode->type, name);
	assert(ndx >= 0);

	/* store the layer offset so we can re-insert layer at the
	   same location on undo */
	unode->layer_offset =
		ndx - CustomData_get_layer_index(data, unode->type);

	/* backup layer data */
	unode->layer_data = MEM_dupallocN(data->layers[ndx].data);

	unode->strength = data->layers[ndx].strength;

	unode->flag_multires = data->layers[ndx].flag & CD_FLAG_MULTIRES;
	if(!unode->flag_multires)
		return;

	/* back multires data */
	cdm = CustomData_get_layer(fdata, CD_GRIDS);
	if(cdm && totface) {
		int i;

		/* check first cdm to see if this layer has multires data */
		if(!CustomData_multires_get_data(cdm, unode->type, name))
			return;

		unode->multires_layer_data =
			MEM_callocN(sizeof(void*) * totface,
				    "PaintLayerUndoNode.multires_layer_data");
		
		for(i = 0; i < totface; ++i, ++cdm) {
			float *f;

			f = CustomData_multires_get_data(cdm, unode->type,
							 name);
			assert(f);
			
			unode->multires_layer_data[i] = MEM_dupallocN(f);
		}
	}
}

void paint_layer_undo_restore(bContext *C, ListBase *lb)
{
	PaintLayerUndoNode *unode = lb->first; /* only one undo node */
	Object *ob;
	Mesh *me;
	CustomData *data, *fdata;
	CustomDataMultires *cdm;
	int i, ndx, offset, active, totelem;

	ob = CTX_data_active_object(C);
	me = get_mesh(ob);
	fdata = &me->fdata;

	switch(unode->type) {
	case CD_MCOL:
		data = &me->fdata;
		totelem = me->totface;
		break;
	case CD_PAINTMASK:
		data = &me->vdata;
		totelem = me->totface;
		break;
	default:
		assert(0);
	}

	/* update multires before making changes */
	if(ED_paint_multires_active(CTX_data_scene(C), ob))
		multires_force_update(ob);

	switch(unode->op) {
	case LAYER_ADDED:
		/* backup current layer data for redo */
		paint_layer_undo_set_remove(unode, unode->layer_name, data,
					    fdata, me->totvert, me->totface);

		active = CustomData_get_active_layer(data, unode->type);
		
		/* remove layer */
		ndx = CustomData_get_named_layer_index(data, unode->type,
						       unode->layer_name);
		CustomData_free_layer(data, unode->type, totelem, ndx);

		/* set active layer */
		offset = CustomData_number_of_layers(data, unode->type) - 1;
		if(active > offset)
			active = offset;
		CustomData_set_layer_active(data, unode->type, active);
		
		/* remove layer's multires data */
		cdm = CustomData_get_layer(fdata, CD_GRIDS);
		if(!cdm)
			break;

		CustomData_multires_remove_layers(cdm, me->totface,
						  unode->type,
						  unode->layer_name);

		break;
	case LAYER_REMOVED:
		paint_layer_undo_set_add(unode, unode->layer_name);

		/* add layer */
		CustomData_add_layer_at_offset(data, unode->type, CD_ASSIGN,
					       unode->layer_data, totelem,
					       unode->layer_offset);

		ndx = CustomData_get_named_layer_index(data, unode->type,
						       unode->layer_name);
		offset = ndx - CustomData_get_layer_index(data, unode->type);

		CustomData_set_layer_active(data, unode->type, offset);
		BLI_strncpy(data->layers[ndx].name, unode->layer_name,
			    sizeof(data->layers[ndx].name));
		data->layers[ndx].strength = unode->strength;

		if(!unode->flag_multires)
			break;
		
		/* add multires layer */
		CustomData_set_layer_offset_flag(data, unode->type,
						 offset, CD_FLAG_MULTIRES);

		cdm = CustomData_get_layer(fdata, CD_GRIDS);
		if(!cdm)
			break;

		for(i = 0; i < me->totface; ++i, ++cdm) {
			void *griddata = unode->multires_layer_data[i];

			CustomData_multires_add_layer_data(cdm, unode->type,
							   unode->layer_name,
							   griddata);
		}

		unode->layer_data = NULL;
		if(unode->multires_layer_data)
			MEM_freeN(unode->multires_layer_data);
		unode->multires_layer_data = NULL;

		break;
	}
}

static void paint_layer_undo_node_free(ListBase *lb)
{
	PaintLayerUndoNode *unode = lb->first;

	if(unode->layer_data)
		MEM_freeN(unode->layer_data);

	if(unode->multires_layer_data) {
		int i;

		for(i = 0; i < unode->totface; ++i)
			MEM_freeN(unode->multires_layer_data[i]);
		MEM_freeN(unode->multires_layer_data);	
	}
}

PaintLayerUndoNode *paint_layer_undo_push(int type, char *description)
{
	PaintLayerUndoNode *unode;

	undo_paint_push_begin(UNDO_PAINT_MESH, description,
			      paint_layer_undo_restore,
			      paint_layer_undo_node_free);

	unode = MEM_callocN(sizeof(PaintLayerUndoNode), "PaintLayerUndoNode");
	unode->type = type;

	BLI_addtail(undo_paint_push_get_list(UNDO_PAINT_MESH), unode);
	undo_paint_push_end(UNDO_PAINT_MESH);

	return unode;
}

/* Exported Functions */

void undo_paint_push_begin(int type, const char *name, UndoRestoreCb restore, UndoFreeCb free)
{
	if(type == UNDO_PAINT_IMAGE)
		undo_stack_push_begin(&ImageUndoStack, name, restore, free);
	else if(type == UNDO_PAINT_MESH)
		undo_stack_push_begin(&MeshUndoStack, name, restore, free);
}

ListBase *undo_paint_push_get_list(int type)
{
	if(type == UNDO_PAINT_IMAGE) {
		if(ImageUndoStack.current)
			return &ImageUndoStack.current->elems;
	}
	else if(type == UNDO_PAINT_MESH) {
		if(MeshUndoStack.current)
			return &MeshUndoStack.current->elems;
	}
	
	return NULL;
}

void undo_paint_push_count_alloc(int type, int size)
{
	if(type == UNDO_PAINT_IMAGE)
		ImageUndoStack.current->undosize += size;
	else if(type == UNDO_PAINT_MESH)
		MeshUndoStack.current->undosize += size;
}

void undo_paint_push_end(int type)
{
	if(type == UNDO_PAINT_IMAGE)
		undo_stack_push_end(&ImageUndoStack);
	else if(type == UNDO_PAINT_MESH)
		undo_stack_push_end(&MeshUndoStack);
}

int ED_undo_paint_step(bContext *C, int type, int step, const char *name)
{
	if(type == UNDO_PAINT_IMAGE)
		return undo_stack_step(C, &ImageUndoStack, step, name);
	else if(type == UNDO_PAINT_MESH)
		return undo_stack_step(C, &MeshUndoStack, step, name);
	
	return 0;
}

void ED_undo_paint_free(void)
{
	undo_stack_free(&ImageUndoStack);
	undo_stack_free(&MeshUndoStack);
}
