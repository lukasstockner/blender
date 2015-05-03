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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/io/io_cache_shapekey.c
 *  \ingroup editor/io
 */


#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_cache_library_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_strands_types.h"

#include "BKE_cache_library.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "io_cache_library.h"

/*********************** add shape key ***********************/

static void ED_cache_shape_key_add(bContext *C, StrandsKeyCacheModifier *skmd, Strands *strands, const bool from_mix)
{
	KeyBlock *kb;
	if ((kb = BKE_cache_modifier_strands_key_insert_key(skmd, strands, NULL, from_mix))) {
		Key *key = skmd->key;
		/* for absolute shape keys, new keys may not be added last */
		skmd->shapenr = BLI_findindex(&key->block, kb) + 1;
		
		WM_event_add_notifier(C, NC_WINDOW, NULL);
	}
}

/*********************** remove shape key ***********************/

static void keyblock_free(StrandsKeyCacheModifier *skmd, KeyBlock *kb)
{
	Key *key = skmd->key;
	
	BLI_remlink(&key->block, kb);
	key->totkey--;
		
	if (kb->data) MEM_freeN(kb->data);
	MEM_freeN(kb);
}

static bool ED_cache_shape_key_remove_all(StrandsKeyCacheModifier *skmd, Strands *UNUSED(strands))
{
	Key *key = skmd->key;
	KeyBlock *kb, *kb_next;
	
	if (key == NULL)
		return false;
	
	for (kb = key->block.first; kb; kb = kb_next) {
		kb_next = kb->next;
		
		keyblock_free(skmd, kb);
	}
	
	key->refkey = NULL;
	skmd->shapenr = 0;
	
	return true;
}

static bool ED_cache_shape_key_remove(StrandsKeyCacheModifier *skmd, Strands *strands)
{
	Key *key = skmd->key;
	KeyBlock *kb, *rkb;
	
	if (key == NULL)
		return false;
	
	kb = BLI_findlink(&key->block, skmd->shapenr - 1);
	if (kb) {
		for (rkb = key->block.first; rkb; rkb = rkb->next) {
			if (rkb->relative == skmd->shapenr - 1) {
				/* remap to the 'Basis' */
				rkb->relative = 0;
			}
			else if (rkb->relative >= skmd->shapenr) {
				/* Fix positional shift of the keys when kb is deleted from the list */
				rkb->relative -= 1;
			}
		}
		
		keyblock_free(skmd, kb);
		
		if (key->refkey == kb) {
			key->refkey = key->block.first;
	
			if (key->refkey) {
				/* apply new basis key on original data */
				BKE_keyblock_convert_to_strands(key->refkey, strands);
			}
		}
		
		if (skmd->shapenr > 1) {
			skmd->shapenr--;
		}
	}
	
	return true;
}

/********************** shape key operators *********************/

static bool shape_key_get_context(bContext *C, CacheLibrary **r_cachelib, StrandsKeyCacheModifier **r_skmd, Strands **r_strands)
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cache_library", &RNA_CacheLibrary).data;
	CacheModifier *md = CTX_data_pointer_get_type(C, "cache_modifier", &RNA_CacheLibraryModifier).data;
	StrandsKeyCacheModifier *skmd;
	Object *ob = CTX_data_active_object(C);
	Strands *strands;
	
	if (!(cachelib && !cachelib->id.lib && md && md->type == eCacheModifierType_StrandsKey))
		return false;
	skmd = (StrandsKeyCacheModifier *)md;
	
	if (!(ob && ob->dup_cache && (ob->transflag & OB_DUPLIGROUP) && ob->dup_group))
		return false;
	if (!BKE_cache_modifier_find_strands(ob->dup_cache, skmd->object, skmd->hair_system, NULL, &strands))
		return false;
	
	if (r_cachelib) *r_cachelib = cachelib;
	if (r_skmd) *r_skmd = skmd;
	if (r_strands) *r_strands = strands;
	return true;
}

static int shape_key_poll(bContext *C)
{
	return shape_key_get_context(C, NULL, NULL, NULL);
}

static int shape_key_exists_poll(bContext *C)
{
	StrandsKeyCacheModifier *skmd;
	
	if (!shape_key_get_context(C, NULL, &skmd, NULL))
		return false;
	
	return (skmd->key && skmd->shapenr >= 0 && skmd->shapenr < skmd->key->totkey);
}

static int shape_key_move_poll(bContext *C)
{
	StrandsKeyCacheModifier *skmd;
	
	if (!shape_key_get_context(C, NULL, &skmd, NULL))
		return false;
	
	return (skmd->key != NULL && skmd->key->totkey > 1);
}

static int shape_key_add_exec(bContext *C, wmOperator *op)
{
	const bool from_mix = RNA_boolean_get(op->ptr, "from_mix");
	CacheLibrary *cachelib;
	StrandsKeyCacheModifier *skmd;
	Strands *strands;
	
	shape_key_get_context(C, &cachelib, &skmd, &strands);
	
	ED_cache_shape_key_add(C, skmd, strands, from_mix);
	
	DAG_id_tag_update(&cachelib->id, OB_RECALC_DATA);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_shape_key_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Shape Key";
	ot->idname = "CACHELIBRARY_OT_shape_key_add";
	ot->description = "Add shape key to the object";
	
	/* api callbacks */
	ot->poll = shape_key_poll;
	ot->exec = shape_key_add_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "from_mix", true, "From Mix", "Create the new shape key from the existing mix of keys");
}

static int shape_key_remove_exec(bContext *C, wmOperator *op)
{
	CacheLibrary *cachelib;
	StrandsKeyCacheModifier *skmd;
	Strands *strands;
	bool changed = false;
	
	shape_key_get_context(C, &cachelib, &skmd, &strands);
	
	if (RNA_boolean_get(op->ptr, "all")) {
		changed = ED_cache_shape_key_remove_all(skmd, strands);
	}
	else {
		changed = ED_cache_shape_key_remove(skmd, strands);
	}
	
	if (changed) {
		DAG_id_tag_update(&cachelib->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_WINDOW, NULL);
		
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void CACHELIBRARY_OT_shape_key_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Shape Key";
	ot->idname = "CACHELIBRARY_OT_shape_key_remove";
	ot->description = "Remove shape key from the object";
	
	/* api callbacks */
	ot->poll = shape_key_exists_poll;
	ot->exec = shape_key_remove_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "all", 0, "All", "Remove all shape keys");
}

static int shape_key_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	CacheLibrary *cachelib;
	StrandsKeyCacheModifier *skmd;
	Strands *strands;
	KeyBlock *kb;
	
	shape_key_get_context(C, &cachelib, &skmd, &strands);
	
	for (kb = skmd->key->block.first; kb; kb = kb->next)
		kb->curval = 0.0f;
	
	DAG_id_tag_update(&cachelib->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_shape_key_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Shape Keys";
	ot->description = "Clear weights for all shape keys";
	ot->idname = "CACHELIBRARY_OT_shape_key_clear";
	
	/* api callbacks */
	ot->poll = shape_key_poll;
	ot->exec = shape_key_clear_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* starting point and step size could be optional */
static int shape_key_retime_exec(bContext *C, wmOperator *UNUSED(op))
{
	CacheLibrary *cachelib;
	StrandsKeyCacheModifier *skmd;
	Strands *strands;
	KeyBlock *kb;
	float cfra = 0.0f;
	
	shape_key_get_context(C, &cachelib, &skmd, &strands);
	
	for (kb = skmd->key->block.first; kb; kb = kb->next)
		kb->pos = (cfra += 0.1f);
	
	DAG_id_tag_update(&cachelib->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_shape_key_retime(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Re-Time Shape Keys";
	ot->description = "Resets the timing for absolute shape keys";
	ot->idname = "CACHELIBRARY_OT_shape_key_retime";
	
	/* api callbacks */
	ot->poll = shape_key_poll;
	ot->exec = shape_key_retime_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


enum {
	KB_MOVE_TOP = -2,
	KB_MOVE_UP = -1,
	KB_MOVE_DOWN = 1,
	KB_MOVE_BOTTOM = 2,
};

static int shape_key_move_exec(bContext *C, wmOperator *op)
{
	const int type = RNA_enum_get(op->ptr, "type");
	CacheLibrary *cachelib;
	StrandsKeyCacheModifier *skmd;
	Strands *strands;
	Key *key;
	int totkey, act_index, new_index;
	
	shape_key_get_context(C, &cachelib, &skmd, &strands);
	key = skmd->key;
	totkey = key->totkey;
	act_index = skmd->shapenr - 1;

	switch (type) {
		case KB_MOVE_TOP:
			/* Replace the ref key only if we're at the top already (only for relative keys) */
			new_index = (ELEM(act_index, 0, 1) || key->type == KEY_NORMAL) ? 0 : 1;
			break;
		case KB_MOVE_BOTTOM:
			new_index = totkey - 1;
			break;
		case KB_MOVE_UP:
		case KB_MOVE_DOWN:
		default:
			new_index = (totkey + act_index + type) % totkey;
			break;
	}

	if (!BKE_keyblock_move_ex(key, &skmd->shapenr, act_index, new_index)) {
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(&cachelib->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_shape_key_move(wmOperatorType *ot)
{
	static EnumPropertyItem slot_move[] = {
		{KB_MOVE_TOP, "TOP", 0, "Top", "Top of the list"},
		{KB_MOVE_UP, "UP", 0, "Up", ""},
		{KB_MOVE_DOWN, "DOWN", 0, "Down", ""},
		{KB_MOVE_BOTTOM, "BOTTOM", 0, "Bottom", "Bottom of the list"},
		{ 0, NULL, 0, NULL, NULL }
	};

	/* identifiers */
	ot->name = "Move Shape Key";
	ot->idname = "CACHELIBRARY_OT_shape_key_move";
	ot->description = "Move the active shape key up/down in the list";

	/* api callbacks */
	ot->poll = shape_key_move_poll;
	ot->exec = shape_key_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

