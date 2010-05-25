#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_mesh.h"

#include "paint_intern.h"

#include <stdio.h>
#include <stdlib.h>

static int paint_mask_set_exec(bContext *C, wmOperator *op)
{
	MaskSetMode mode = RNA_enum_get(op->ptr, "mode");
	Object *ob;
	Mesh *me;

	ob = CTX_data_active_object(C);

	if((me = get_mesh(ob))) {
		printf("paint mask set %d\n", mode);

		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	}

	return OPERATOR_FINISHED;
}

static int mask_poll(bContext *C)
{
	return 1; // TODO
}

/* Temporary operator to test masking; simply fills up a mask for the
   entire object, setting each point to either 0, 1, or a random value
*/
void PAINT_OT_mask_set(wmOperatorType *ot)
{
	static EnumPropertyItem mask_items[] = {
		{MASKING_CLEAR, "CLEAR", 0, "Clear", ""},
		{MASKING_FULL, "FULL", 0, "Full", ""},
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
