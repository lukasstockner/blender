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

/* TODO(sergey): This is rather temp solution anyway. */
#include "../../ikplugin/BIK_api.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph_types.h"
#include "depsgraph_intern.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"

#include "depsgraph_util_map.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

void BKE_animsys_eval_action(EvaluationContext *eval_ctx, ID *id, bAction *action, TimeSourceDepsNode *time_src)
{
	printf("%s on %s\n", __func__, id->name);
	if (ID_REAL_USERS(id) > 0) {
		AnimData *adt = BKE_animdata_from_id(id);
		float ctime = time_src->cfra;
		BKE_animsys_evaluate_animdata(NULL, id, adt, ctime, ADT_RECALC_ANIM);
	}
}

void BKE_animsys_eval_driver(EvaluationContext *eval_ctx, ID *id, FCurve *fcurve, TimeSourceDepsNode *time_src)
{
	/* TODO(sergey): De-duplicate with BKE animsys. */
	printf("%s on %s\n", __func__, id->name);
	if (ID_REAL_USERS(id) > 0 && (fcurve->driver->flag & DRIVER_FLAG_INVALID) == 0) {
		float ctime = time_src->cfra;
		PointerRNA id_ptr;
		RNA_id_pointer_create(id, &id_ptr);
		calculate_fcurve(fcurve, ctime);
		if (!BKE_animsys_execute_fcurve(&id_ptr, NULL, fcurve)) {
			fcurve->driver->flag |= DRIVER_FLAG_INVALID;
		}
		fcurve->driver->flag &= ~DRIVER_FLAG_RECALC;
	}
}

void BKE_pose_splineik_evaluate(EvaluationContext *eval_ctx, Object *ob, bPoseChannel *rootchan) {}

void BKE_pose_rebuild_op(EvaluationContext *eval_ctx, Object *ob, bPose *pose)
{
	bArmature *arm = (bArmature *)ob->data;
	printf("%s on %s\n", __func__, ob->id.name);
	BLI_assert(ob->type == OB_ARMATURE);
	if ((ob->pose == NULL) || (ob->pose->flag & POSE_RECALC)) {
		BKE_pose_rebuild(ob, arm);
	}
}

void BKE_pose_eval_init(EvaluationContext *eval_ctx,
                        Scene *scene,
                        Object *ob,
                        bPose *pose)
{
	printf("%s on %s\n", __func__, ob->id.name);
	BLI_assert(ob->type == OB_ARMATURE);
	float ctime = BKE_scene_frame_get(scene); /* not accurate... */
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
                        bPoseChannel *pchan) {
	bArmature *arm = (bArmature *)ob->data;
	printf("%s on %s phan %s\n", __func__, ob->id.name, pchan->name);
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
	printf("%s on %s phan %s\n", __func__, ob->id.name, pchan->name);
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
	printf("%s on %s phan %s\n", __func__, ob->id.name, rootchan->name);
	float ctime = BKE_scene_frame_get(scene); /* not accurate... */
	BIK_execute_tree(scene, ob, rootchan, ctime);
}

void BKE_pose_eval_flush(EvaluationContext *eval_ctx,
                         Scene *scene,
                         Object *ob,
                         bPose *pose)
{
	bPoseChannel *pchan;
	float imat[4][4];
	printf("%s on %s\n", __func__, ob->id.name);
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
void BKE_rigidbody_object_sync_transforms(EvaluationContext *eval_ctx, Scene *scene, Object *ob) {}

void BKE_object_eval_parent(EvaluationContext *eval_ctx, Object *ob) {}

void BKE_mesh_eval_geometry(EvaluationContext *eval_ctx, Mesh *mesh) {}
void BKE_mball_eval_geometry(EvaluationContext *eval_ctx, MetaBall *mball) {}
void BKE_curve_eval_geometry(EvaluationContext *eval_ctx, Curve *curve) {}
void BKE_curve_eval_path(EvaluationContext *eval_ctx, Curve *curve) {}
void BKE_lattice_eval_geometry(EvaluationContext *eval_ctx, Lattice *latt) {}

const string deg_op_name_object_parent = "BKE_object_eval_parent";
const string deg_op_name_object_local_transform = "BKE_object_eval_local_transform";
const string deg_op_name_constraint_stack = "Constraint Stack";
const string deg_op_name_rigidbody_world_rebuild = "Rigidbody World Rebuild";
const string deg_op_name_rigidbody_world_simulate = "Rigidbody World Do Simulation";
const string deg_op_name_rigidbody_object_sync = "RigidBodyObject Sync";
const string deg_op_name_pose_rebuild = "Rebuild Pose";
const string deg_op_name_pose_eval_init = "Init Pose Eval";
const string deg_op_name_pose_eval_flush = "Flush Pose Eval";
const string deg_op_name_ik_solver = "IK Solver";
const string deg_op_name_spline_ik_solver = "Spline IK Solver";
const string deg_op_name_psys_eval = "PSys Eval";
string deg_op_name_action(const bAction *action)
{
	return string_format("Action %s", action->id.name);
}
string deg_op_name_driver(const ChannelDriver *driver)
{
	return string_format("Driver @ %p", driver);
}
string deg_op_name_modifier(const ModifierData *md)
{
	return string_format("Modifier %s", md->name);
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
