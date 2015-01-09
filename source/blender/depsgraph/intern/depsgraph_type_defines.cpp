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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_fcurve.h"
#include "BKE_scene.h"
#include "BKE_object.h"
#include "BKE_rigidbody.h"

/* TODO(sergey): This is rather temp solution anyway. */
#include "../../ikplugin/BIK_api.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph_debug.h"
#include "depsgraph_types.h"
#include "depsgraph_intern.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"

#include "depsgraph_util_map.h"

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

void BKE_pose_eval_init(EvaluationContext *eval_ctx,
                        Scene *scene,
                        Object *ob,
                        bPose *pose)
{
	DEG_DEBUG_PRINTF("%s on %s\n", __func__, ob->id.name);
	BLI_assert(ob->type == OB_ARMATURE);
	float ctime = BKE_scene_frame_get(scene); /* not accurate... */

	/* We demand having proper pose. */
	BLI_assert(ob->pose != NULL);
	BLI_assert((ob->pose->flag & POSE_RECALC) == 0);

	/* 1. clear flags */
	for (bPoseChannel *pchan = (bPoseChannel *)ob->pose->chanbase.first;
	     pchan != NULL;
	     pchan = (bPoseChannel *)pchan->next)
	{
		pchan->flag &= ~(POSE_DONE | POSE_CHAIN | POSE_IKTREE | POSE_IKSPLINE);
	}

	/* 2a. construct the IK tree (standard IK) */
	BIK_initialize_tree(scene, ob, ctime);

	/* 2b. construct the Spline IK trees
	 *  - this is not integrated as an IK plugin, since it should be able
	 *	  to function in conjunction with standard IK
	 */
	BKE_pose_splineik_init_tree(scene, ob, ctime);
}

void BKE_pose_eval_bone(EvaluationContext *eval_ctx,
                        Scene *scene,
                        Object *ob,
                        bPoseChannel *pchan) 
{
	DEG_DEBUG_PRINTF("%s on %s pchan %s\n", __func__, ob->id.name, pchan->name);
	bArmature *arm = (bArmature *)ob->data;
	BLI_assert(ob->type == OB_ARMATURE);
	if (arm->edbo || (arm->flag & ARM_RESTPOS)) {
		Bone *bone = pchan->bone;
		if (bone) {
			copy_m4_m4(pchan->pose_mat, bone->arm_mat);
			copy_v3_v3(pchan->pose_head, bone->arm_head);
			copy_v3_v3(pchan->pose_tail, bone->arm_tail);
		}
	}
	else {
		/* TODO(sergey): Currently if there are constraints full transform is being
		 * evaluated in BKE_pose_constraints_evaluate.
		 */
		if (pchan->constraints.first == NULL) {
			if (pchan->flag & POSE_IKTREE || pchan->flag & POSE_IKSPLINE) {
				/* pass */
			}
			else {
				/* TODO(sergey): Use time source node for time. */
				float ctime = BKE_scene_frame_get(scene); /* not accurate... */
				BKE_pose_where_is_bone(scene, ob, pchan, ctime, 1);
			}
		}
	}
}

void BKE_pose_constraints_evaluate(EvaluationContext *eval_ctx,
                                   Object *ob,
                                   bPoseChannel *pchan)
{
	DEG_DEBUG_PRINTF("%s on %s pchan %s\n", __func__, ob->id.name, pchan->name);
	Scene *scene = (Scene*)G.main->scene.first;
	float ctime = BKE_scene_frame_get(scene); /* not accurate... */

	if (pchan->flag & POSE_IKTREE || pchan->flag & POSE_IKSPLINE) {
		/* IK are being solved separately/ */
	}
	else {
		BKE_pose_where_is_bone(scene, ob, pchan, ctime, 1);
	}
}

void BKE_pose_iktree_evaluate(EvaluationContext *eval_ctx,
                              Scene *scene,
                              Object *ob,
                              bPoseChannel *rootchan)
{
	DEG_DEBUG_PRINTF("%s on %s pchan %s\n", __func__, ob->id.name, rootchan->name);
	float ctime = BKE_scene_frame_get(scene); /* not accurate... */
	BIK_execute_tree(scene, ob, rootchan, ctime);
}

void BKE_pose_splineik_evaluate(EvaluationContext *eval_ctx,
                                Scene *scene,
                                Object *ob,
                                bPoseChannel *rootchan)
{
	DEG_DEBUG_PRINTF("%s on %s pchan %s\n", __func__, ob->id.name, rootchan->name);
	float ctime = BKE_scene_frame_get(scene); /* not accurate... */
	BKE_splineik_execute_tree(scene, ob, rootchan, ctime);
}

void BKE_pose_eval_flush(EvaluationContext *eval_ctx,
                         Scene *scene,
                         Object *ob,
                         bPose *pose)
{
	DEG_DEBUG_PRINTF("%s on %s\n", __func__, ob->id.name);
	bPoseChannel *pchan;
	float imat[4][4];
	BLI_assert(ob->type == OB_ARMATURE);

	/* 6. release the IK tree */
	float ctime = BKE_scene_frame_get(scene); /* not accurate... */
	BIK_release_tree(scene, ob, ctime);

	/* calculating deform matrices */
	for (pchan = (bPoseChannel *)ob->pose->chanbase.first;
	     pchan;
	     pchan = (bPoseChannel *)pchan->next)
	{
		if (pchan->bone) {
			invert_m4_m4(imat, pchan->bone->arm_mat);
			mul_m4_m4m4(pchan->chan_mat, pchan->pose_mat, imat);
		}
	}
}

void BKE_particle_system_eval(EvaluationContext *eval_ctx, Object *ob, ParticleSystem *psys) {}

void BKE_rigidbody_rebuild_sim(EvaluationContext *eval_ctx, Scene *scene) {}
void BKE_rigidbody_eval_simulation(EvaluationContext *eval_ctx, Scene *scene) {}

void BKE_rigidbody_object_sync_transforms(EvaluationContext *eval_ctx, Scene *scene, Object *ob)
{
	DEG_DEBUG_PRINTF("%s on %s\n", __func__, ob->id.name);
	RigidBodyWorld *rbw = scene->rigidbody_world;
	float ctime = BKE_scene_frame_get(scene);
	/* read values pushed into RBO from sim/cache... */
	BKE_rigidbody_sync_transforms(rbw, ob, ctime);
}

void BKE_mesh_eval_geometry(EvaluationContext *eval_ctx, Mesh *mesh) {}
void BKE_mball_eval_geometry(EvaluationContext *eval_ctx, MetaBall *mball) {}
void BKE_curve_eval_geometry(EvaluationContext *eval_ctx, Curve *curve) {}
void BKE_curve_eval_path(EvaluationContext *eval_ctx, Curve *curve) {}
void BKE_lattice_eval_geometry(EvaluationContext *eval_ctx, Lattice *latt) {}

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
