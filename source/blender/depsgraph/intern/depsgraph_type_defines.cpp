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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Defines and code for core node types
 */

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "DNA_anim_types.h"

#include "BKE_animsys.h"
#include "BKE_fcurve.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
} /* extern "C" */

#include "depsgraph_debug.h"
#include "depsgraph_intern.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

void BKE_animsys_eval_animdata(EvaluationContext *UNUSED(eval_ctx),
                               ID *id,
                               TimeSourceDepsNode *time_src)
{
	DEG_DEBUG_PRINTF("%s on %s\n", __func__, id->name);
	AnimData *adt = BKE_animdata_from_id(id);
	Scene *scene = NULL; // XXX: this is only needed for flushing RNA updates, which should get handled as part of the graph instead...
	float ctime = time_src->cfra;
	BKE_animsys_evaluate_animdata(scene, id, adt, ctime, ADT_RECALC_ANIM);
}

void BKE_animsys_eval_driver(EvaluationContext *UNUSED(eval_ctx),
                             ID *id,
                             FCurve *fcu,
                             TimeSourceDepsNode *time_src)
{
	/* TODO(sergey): De-duplicate with BKE animsys. */
	DEG_DEBUG_PRINTF("%s on %s (%s[%d])\n",
	                 __func__,
	                 id->name,
	                 fcu->rna_path,
	                 fcu->array_index);

	ChannelDriver *driver = fcu->driver;
	PointerRNA id_ptr;
	float ctime = time_src->cfra;
	bool ok = false;
	
	RNA_id_pointer_create(id, &id_ptr);
	
	/* check if this driver's curve should be skipped */
	if ((fcu->flag & (FCURVE_MUTED | FCURVE_DISABLED)) == 0) {
		/* check if driver itself is tagged for recalculation */
		/* XXX driver recalc flag is not set yet by depsgraph! */
		if ((driver) && !(driver->flag & DRIVER_FLAG_INVALID) /*&& (driver->flag & DRIVER_FLAG_RECALC)*/) {
			/* evaluate this using values set already in other places
			 * NOTE: for 'layering' option later on, we should check if we should remove old value before adding
			 *       new to only be done when drivers only changed */
			//printf("\told val = %f\n", fcu->curval);
			calculate_fcurve(fcu, ctime);
			ok = BKE_animsys_execute_fcurve(&id_ptr, NULL, fcu);
			//printf("\tnew val = %f\n", fcu->curval);
			
			/* clear recalc flag */
			driver->flag &= ~DRIVER_FLAG_RECALC;
			
			/* set error-flag if evaluation failed */
			if (ok == 0) {
				printf("invalid driver - %s[%d]\n", fcu->rna_path, fcu->array_index);
				driver->flag |= DRIVER_FLAG_INVALID;
			}
		}
	}
}

/* ******************************************************** */
/* External API */

/* Global type registry */

/* NOTE: For now, this is a hashtable not array, since the core node types
 * currently do not have contiguous ID values. Using a hash here gives us
 * more flexibility, albeit using more memory and also sacrificing a little
 * speed. Later on, when things stabilise we may turn this back to an array
 * since there are only just a few node types that an array would cope fine...
 */
static GHash *_depsnode_typeinfo_registry = NULL;

/* Registration ------------------------------------------- */

/* Register node type */
void DEG_register_node_typeinfo(DepsNodeFactory *factory)
{
	BLI_assert(factory != NULL);
	BLI_ghash_insert(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(factory->type()), factory);
}

/* Register all node types */
void DEG_register_node_types(void)
{
	/* initialise registry */
	_depsnode_typeinfo_registry = BLI_ghash_int_new("Depsgraph Node Type Registry");
	
	/* register node types */
	DEG_register_base_depsnodes();
	DEG_register_component_depsnodes();
	DEG_register_operation_depsnodes();
}

/* Free registry on exit */
void DEG_free_node_types(void)
{
	BLI_ghash_free(_depsnode_typeinfo_registry, NULL, NULL);
}

/* Getters ------------------------------------------------- */

/* Get typeinfo for specified type */
DepsNodeFactory *DEG_get_node_factory(const eDepsNode_Type type)
{
	/* look up type - at worst, it doesn't exist in table yet, and we fail */
	return (DepsNodeFactory *)BLI_ghash_lookup(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(type));
}

/* Get typeinfo for provided node */
DepsNodeFactory *DEG_node_get_factory(const DepsNode *node)
{
	if (!node)
		return NULL;
	
	return DEG_get_node_factory(node->type);
}

/* ******************************************************** */
