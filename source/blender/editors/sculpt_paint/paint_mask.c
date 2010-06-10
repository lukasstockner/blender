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

#include "BLI_pbvh.h"

#include "paint_intern.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_mask_value(MaskSetMode mode, float *m)
{
	*m = (mode == MASKING_CLEAR ? 1 :
	      mode == MASKING_FILL ? 0 :
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
	Mesh *me;
	PBVH *pbvh;

	scene = CTX_data_scene(C);
	ob = CTX_data_active_object(C);
	me = get_mesh(ob);
	mmd = paint_multires_active(scene, ob);

	/* Make sure a mask layer has been allocated for the mesh */
	if(!CustomData_get_layer(&me->vdata, CD_PAINTMASK))
		CustomData_add_layer(&me->vdata, CD_PAINTMASK, CD_CALLOC, NULL, me->totvert);

	dm = mesh_get_derived_final(scene, ob, 0);
	pbvh = dm->getPBVH(ob, dm);

	if(pbvh) {
		PBVHNode **nodes;
		int n, totnode;

		BLI_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
		for(n=0; n<totnode; n++) {
			PBVHVertexIter vd;

			BLI_pbvh_vertex_iter_begin(pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(vd.mask)
					set_mask_value(mode, vd.mask);
			}
			BLI_pbvh_vertex_iter_end;

			BLI_pbvh_node_mark_update(nodes[n]);
		}

		if(mmd)
			multires_mark_as_modified(ob);
		BLI_pbvh_update(pbvh, PBVH_UpdateBB|PBVH_UpdateOriginalBB|PBVH_UpdateRedraw, NULL);
		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}
	
	return OPERATOR_FINISHED;
}

static int mask_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && get_mesh(ob) && ob->sculpt;
}

/* Temporary operator to test masking; simply fills up a mask for the
   entire object, setting each point to either 0, 1, or a random value
*/
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
