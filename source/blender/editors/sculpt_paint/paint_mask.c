#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_subsurf.h"

#include "BLI_listbase.h"
#include "BLI_pbvh.h"

#include "ED_mesh.h"
#include "ED_sculpt.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

static void set_mask_value(MaskSetMode mode, float *m)
{
	*m = (mode == MASKING_CLEAR ? 0 :
	      mode == MASKING_FILL ? 1 :
	      mode == MASKING_INVERT ? (1 - *m) :
	      mode == MASKING_RANDOM ? (float)rand() / RAND_MAX : 0);
}

static int paint_mask_set_exec(bContext *C, wmOperator *op)
{
	MaskSetMode mode = RNA_enum_get(op->ptr, "mode");
	struct Scene *scene;
	Object *ob;
	DerivedMesh *dm;
	struct MultiresModifierData *mmd;
	SculptSession *ss;
	Mesh *me;
	PBVH *pbvh;

	scene = CTX_data_scene(C);
	ob = CTX_data_active_object(C);
	ss = ob->sculpt;
	me = get_mesh(ob);
	mmd = paint_multires_active(scene, ob);

	dm = mesh_get_derived_final(scene, ob, 0);
	pbvh = dm->getPBVH(ob, dm);

	if(pbvh) {
		PBVHNode **nodes;
		int n, totnode;

		BLI_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

		sculpt_undo_push_begin(ss, "Paint mask fill");

		for(n=0; n<totnode; n++) {
			PBVHVertexIter vd;

			sculpt_undo_push_node(ss, nodes[n]);

			BLI_pbvh_vertex_iter_begin(pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(vd.mask_active)
					set_mask_value(mode, vd.mask_active);
			}
			BLI_pbvh_vertex_iter_end;

			BLI_pbvh_node_mark_update(nodes[n]);
		}

		if(mmd)
			multires_mark_as_modified(ob);
		BLI_pbvh_update(pbvh, PBVH_UpdateBB|PBVH_UpdateOriginalBB|PBVH_UpdateRedraw, NULL);

		sculpt_undo_push_end(ss);

		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}
	
	return OPERATOR_FINISHED;
}

static int mask_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && get_mesh(ob) && ob->sculpt;
}

/* fills up a mask for the entire object, setting each vertex to
   either 0, 1, or a random value */
void PAINT_OT_mask_set(wmOperatorType *ot)
{
	static EnumPropertyItem mask_items[] = {
		{MASKING_CLEAR, "CLEAR", 0, "Clear", ""},
		{MASKING_FILL, "FILL", 0, "Fill", ""},
		{MASKING_INVERT, "INVERT", 0, "Invert", ""},
		{MASKING_RANDOM, "RANDOM", 0, "Random", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Set Mask";
	ot->idname= "PAINT_OT_mask_set";
	
	/* api callbacks */
	ot->exec= paint_mask_set_exec;
	ot->poll= mask_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "mode", mask_items, MASKING_CLEAR, "Mode", "");
}

typedef enum {
	LAYER_ADDED,
	LAYER_REMOVED
} PaintMaskLayerOp;

/* if this is a multires mesh, update it and free the DM.
   returns 1 if this is a multires mesh, 0 otherwise */
static int paintmask_check_multires(bContext *C)
{
	struct Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);

	if(paint_multires_active(scene, ob)) {
		multires_force_update(ob);
		return 1;
	}

	return 0;
}

/* If this is a multires mesh, update it and free the DM, then add or remove
   a paintmask layer from the grid layer */
static void paintmask_adjust_multires(bContext *C,
				      PaintMaskLayerOp op,
				      int layer_offset,
				      float **removed_multires_data)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = get_mesh(ob);
	CustomData *cd = CustomData_get_layer(&me->fdata, CD_FACEGRID);
	int pmask_first_layer, i;

	assert(cd);

	for(i = 0; i < me->totface; ++i) {
		pmask_first_layer = CustomData_get_layer_index(cd + i,
							       CD_PAINTMASK);

		switch(op) {
		case LAYER_ADDED:
			/* Add a layer of paintmask from grids */

			assert(!removed_multires_data || layer_offset != -1);

			/* if restoring from undo, copy the old data
			   back into CustomData */
			if(removed_multires_data)
				CustomData_add_layer_at_offset(cd + i, CD_PAINTMASK,
							       CD_ASSIGN,
							       removed_multires_data[i],
							       cd[i].grid_elems,
							       layer_offset);
			else							     
				CustomData_add_layer(cd + i, CD_PAINTMASK,
						     CD_CALLOC, NULL,
						     cd[i].grid_elems);
			break;

		case LAYER_REMOVED:
			/* Remove a layer of paintmask from grids */
			CustomData_free_layer(cd + i, CD_PAINTMASK,
					      cd[i].grid_elems,
					      pmask_first_layer + layer_offset);
			break;
		}
	}
}

/* Paint has its own undo */
typedef struct PaintMaskUndoNode {
	struct PaintMaskUndoNode *next, *prev;

	PaintMaskLayerOp type;
	int layer_offset;

	char layer_name[32];
	float *removed_data;
	float **removed_multires_data;

	int totface;
} PaintMaskUndoNode;

/* copy the mesh/multires data for paintmask at layer_offset into unode */
static void paintmask_undo_backup_layer(bContext *C, PaintMaskUndoNode *unode,
					int layer_offset)
{
	struct Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Mesh *me = get_mesh(ob);
	int pmask_first_layer;
	
	pmask_first_layer = CustomData_get_layer_index(&me->vdata,
						       CD_PAINTMASK);

	/* if removing a layer, make a copy of the mask
	   data for restoring from undo */
	unode->removed_data =
		MEM_dupallocN(CustomData_get_layer_n(&me->vdata,
						     CD_PAINTMASK,
						     layer_offset));

	strcpy(unode->layer_name,
	       me->vdata.layers[pmask_first_layer+layer_offset].name);

	if(paint_multires_active(scene, ob)) {
		/* need to copy active layer of multires data too */
		CustomData *grids = CustomData_get_layer(&me->fdata,
							 CD_FACEGRID);
		int i;

		unode->totface = me->totface;
		unode->removed_multires_data =
			MEM_callocN(sizeof(float*) * me->totface,
				    "removed_multires_data");

		for(i = 0; i < me->totface; ++i) {
			unode->removed_multires_data[i] =
				MEM_dupallocN(CustomData_get_layer_n(grids + i,
								     CD_PAINTMASK,
								     layer_offset));
		}
	}
}

/* for redraw, just need to update the pbvh's vbo buffers */
static void paintmask_redraw(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if(ob->sculpt->pbvh)
		BLI_pbvh_search_callback(ob->sculpt->pbvh, NULL, NULL,
					 BLI_pbvh_node_mark_update_draw_buffers, NULL);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);	
}

static void paintmask_undo_restore(bContext *C, ListBase *lb)
{
	struct Object *ob = CTX_data_active_object(C);
	PaintMaskUndoNode *unode = lb->first; /* only one undo node */
	Mesh *me = get_mesh(ob);
	CustomData *vdata = &me->vdata;
	int pmask_first_layer = CustomData_get_layer_index(vdata, CD_PAINTMASK);
	int multires;

	multires = paintmask_check_multires(C);

	switch(unode->type) {
	case LAYER_ADDED:
		paintmask_undo_backup_layer(C, unode, unode->layer_offset);

		if(multires)
			paintmask_adjust_multires(C, LAYER_REMOVED, unode->layer_offset, NULL);

		CustomData_free_layer(vdata, CD_PAINTMASK, me->totvert,
				      pmask_first_layer + unode->layer_offset);
		break;
	case LAYER_REMOVED:
		if(multires)
			paintmask_adjust_multires(C, LAYER_ADDED,
						  unode->layer_offset,
						  unode->removed_multires_data);
		CustomData_add_layer_at_offset(vdata,
					       CD_PAINTMASK,
					       CD_ASSIGN,
					       unode->removed_data,
					       me->totvert,
					       unode->layer_offset);

		CustomData_set_layer_active(vdata, CD_PAINTMASK, unode->layer_offset);
		strcpy(vdata->layers[pmask_first_layer+unode->layer_offset].name, unode->layer_name);

		unode->removed_data = NULL;
		if(unode->removed_multires_data) {
			MEM_freeN(unode->removed_multires_data);
			unode->removed_multires_data = NULL;
		}

		break;
	}

	/* paint undo swaps data between undo node and scene,
	   so reverse the effect of the node after undo/redo */
	unode->type = unode->type == LAYER_ADDED ? LAYER_REMOVED : LAYER_ADDED;

	paintmask_redraw(C);
}

static void paintmask_undo_push_node(bContext *C, PaintMaskLayerOp op, int layer_offset)
{
	ListBase *lb= undo_paint_push_get_list(UNDO_PAINT_MESH);
	PaintMaskUndoNode *unode = MEM_callocN(sizeof(PaintMaskUndoNode),
					       "PaintMaskUndoNode");

	unode->type = op;
	unode->layer_offset = layer_offset;

	if(op == LAYER_REMOVED)
		paintmask_undo_backup_layer(C, unode, layer_offset);

	BLI_addtail(lb, unode);
}

static void paintmask_undo_free(ListBase *lb)
{
	PaintMaskUndoNode *unode = lb->first;
	int i;

	if(unode->removed_multires_data) {
		for(i = 0; i < unode->totface; ++i)
			MEM_freeN(unode->removed_multires_data[i]);
		MEM_freeN(unode->removed_multires_data);	
	}

	if(unode->removed_data)
		MEM_freeN(unode->removed_data);


}

static void paintmask_undo_push(bContext *C, char *name,
			   PaintMaskLayerOp op, int layer_offset)
{
	undo_paint_push_begin(UNDO_PAINT_MESH,
			      name,
			      paintmask_undo_restore,
			      paintmask_undo_free);

	paintmask_undo_push_node(C, op, layer_offset);

	undo_paint_push_end(UNDO_PAINT_MESH);
}

static int mask_layer_poll(bContext *C)
{
	return mask_poll(C) && ED_mesh_layers_poll(C);
}

static int mask_layer_add_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	Mesh *me= ob->data;
	int multires, top;

	multires = paintmask_check_multires(C);

	paintmask_undo_push(C, "Add paint mask", LAYER_ADDED,
			    CustomData_number_of_layers(&me->vdata, CD_PAINTMASK));

	if(multires)
		paintmask_adjust_multires(C, LAYER_ADDED, -1, NULL);

	top= CustomData_number_of_layers(&me->vdata, CD_PAINTMASK);
	CustomData_add_layer(&me->vdata, CD_PAINTMASK, CD_DEFAULT, NULL, me->totvert);
	CustomData_set_layer_active(&me->vdata, CD_PAINTMASK, top);

	paintmask_redraw(C);

	return OPERATOR_FINISHED;
}

void PAINT_OT_mask_layer_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Mask Layer";
	ot->description= "Add a paint mask layer";
	ot->idname= "PAINT_OT_mask_layer_add";
	
	/* api callbacks */
	ot->poll= mask_layer_poll;
	ot->exec= mask_layer_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int mask_layer_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	Mesh *me= ob->data;
	int active_offset;

	active_offset = CustomData_get_active_layer(&me->vdata, CD_PAINTMASK);

	if(active_offset >= 0) {
		int multires = paintmask_check_multires(C);

		paintmask_undo_push(C, "Remove paint mask", LAYER_REMOVED,
				    active_offset);

		if(multires)
			paintmask_adjust_multires(C, LAYER_REMOVED, active_offset, NULL);

		CustomData_free_layer_active(&me->vdata, CD_PAINTMASK, me->totvert);

		paintmask_redraw(C);
	}
	else
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

void PAINT_OT_mask_layer_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Mask Layer";
	ot->description= "Remove the active paint mask layer";
	ot->idname= "PAINT_OT_mask_layer_remove";
	
	/* api callbacks */
	ot->poll= mask_layer_poll;
	ot->exec= mask_layer_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
