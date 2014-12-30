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
 * Contributor(s): Based on original depsgraph.c code - Blender Foundation (2005-2013)
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Methods for constructing depsgraph
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_group.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_rigidbody.h"
#include "BKE_sound.h"
#include "BKE_texture.h"
#include "BKE_tracking.h"
#include "BKE_world.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"
#include "depsgraph_types.h"
#include "depsgraph_build.h"
#include "depsgraph_eval.h"
#include "depsgraph_intern.h"

#include "stubs.h" // XXX: REMOVE THIS INCLUDE ONCE DEPSGRAPH REFACTOR PROJECT IS DONE!!!

namespace {

/* TODO(sergey): This is a stupid copy of function from depsgraph.c/ */
bool modifier_check_depends_on_time(Object *ob, ModifierData *md)
{
	if (modifier_dependsOnTime(md)) {
		return true;
	}

	/* Check whether modifier is animated. */
	if (ob->adt) {
		AnimData *adt = ob->adt;
		FCurve *fcu;

		char pattern[MAX_NAME + 10];
		/* TODO(sergey): Escape modifier name. */
		BLI_snprintf(pattern, sizeof(pattern), "modifiers[%s", md->name);

		/* action - check for F-Curves with paths containing 'modifiers[' */
		if (adt->action) {
			for (fcu = (FCurve *)adt->action->curves.first;
			     fcu != NULL;
			     fcu = (FCurve *)fcu->next)
			{
				if (fcu->rna_path && strstr(fcu->rna_path, pattern))
					return true;
			}
		}

		/* This here allows modifier properties to get driven and still update properly
		 *
		 * Workaround to get [#26764] (e.g. subsurf levels not updating when animated/driven)
		 * working, without the updating problems ([#28525] [#28690] [#28774] [#28777]) caused
		 * by the RNA updates cache introduced in r.38649
		 */
		for (fcu = (FCurve *)adt->drivers.first;
		     fcu != NULL;
		     fcu = (FCurve *)fcu->next)
		{
			if (fcu->rna_path && strstr(fcu->rna_path, pattern))
				return true;
		}

		/* XXX: also, should check NLA strips, though for now assume that nobody uses
		 * that and we can omit that for performance reasons... */
	}

	return false;
}

BLI_INLINE OperationKey bone_transforms_key(Object *ob,
                                            bPoseChannel *pchan)
{
	if (pchan->constraints.first != NULL) {
		return OperationKey(&ob->id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_CONSTRAINTS);
	}
	return OperationKey(&ob->id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_POSE_PARENT);
}

void root_map_add_bone(const char *bone,
                       const char *root,
                       DepsgraphRelationBuilder::RootPChanMap *root_map)
{
	DepsgraphRelationBuilder::RootPChanMap::iterator found = root_map->find(bone);
	if (found != root_map->end()) {
		found->second.push_back(root);
	}
	else {
		DepsgraphRelationBuilder::RootPChanVector new_vector;
		new_vector.push_back(root);
		root_map->insert(std::pair<const char*, DepsgraphRelationBuilder::RootPChanVector> (bone, new_vector));
	}
}

bool pchan_check_common_solver_root(const DepsgraphRelationBuilder::RootPChanMap &root_map,
                                    const char *pchan1, const char *pchan2)
{
	const DepsgraphRelationBuilder::RootPChanMap::const_iterator found1 = root_map.find(pchan1);
	if (found1 == root_map.end()) {
		return false;
	}

	const DepsgraphRelationBuilder::RootPChanMap::const_iterator found2 = root_map.find(pchan2);
	if (found2 == root_map.end()) {
		return false;
	}

	const DepsgraphRelationBuilder::RootPChanVector &vec1 = found1->second;
	const DepsgraphRelationBuilder::RootPChanVector &vec2 = found2->second;

	for (DepsgraphRelationBuilder::RootPChanVector::const_iterator it1 = vec1.begin();
	     it1 != vec1.end();
	     ++it1)
	{
		for (DepsgraphRelationBuilder::RootPChanVector::const_iterator it2 = vec2.begin();
		     it2 != vec2.end();
		     ++it2)
		{
			if (strcmp(*it1, *it2) == 0) {
				return true;
			}
		}
	}

	return false;
}

}  /* namespace */

/* ************************************************* */
/* Relations Builder */

void DepsgraphRelationBuilder::build_scene(Scene *scene)
{
	if (scene->set) {
		// TODO: link set to scene, especially our timesource...
	}
	
	/* scene objects */
	for (Base *base = (Base *)scene->base.first; base; base = base->next) {
		Object *ob = base->object;
		
		/* object itself */
		build_object(scene, ob);
		
#if 0
		/* object that this is a proxy for */
		// XXX: the way that proxies work needs to be completely reviewed!
		if (ob->proxy) {
			build_object(scene, ob->proxy);
		}
#endif
		
#if 0
		/* handled in next loop... 
		 * NOTE: in most cases, setting dupli-group means that we may want
		 *       to instance existing data and/or reuse it with very few
		 *       modifications...
		 */
		if (ob->dup_group) {
			id_tag_set(ob->dup_group);
		}
#endif
	}
	
#if 0
	/* tagged groups */
	for (Group *group = (Group *)m_bmain->group.first; group; group = (Group *)group->id.next) {
		if (is_id_tagged(group)) {
			// TODO: we need to make this group reliant on the object that spawned it...
			build_subgraph_nodes(group);
			
			id_tag_clear(group);
		}
	}
#endif
}

void DepsgraphRelationBuilder::build_object(Scene *scene, Object *ob)
{
	/* Object Transforms */
	eDepsOperation_Code base_op = (ob->parent) ? DEG_OPCODE_TRANSFORM_PARENT : DEG_OPCODE_TRANSFORM_LOCAL;
	OperationKey base_op_key(&ob->id, DEPSNODE_TYPE_TRANSFORM, base_op);
	
	OperationKey local_transform_key(&ob->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_LOCAL);
	OperationKey parent_transform_key(&ob->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_PARENT);
	OperationKey final_transform_key(&ob->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_FINAL);
	
	OperationKey ob_ubereval_key(&ob->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_OBJECT_UBEREVAL);
	
	/* parenting */
	if (ob->parent) {
		/* parent relationship */
		build_object_parent(ob);
		
		/* local -> parent */
		add_relation(local_transform_key, parent_transform_key, DEPSREL_TYPE_COMPONENT_ORDER, "[ObLocal -> ObParent]");
	}
	
	/* object constraints */
	if (ob->constraints.first) {
		OperationKey constraint_key(&ob->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_CONSTRAINTS);
		
		/* constraint relations */
		// TODO: provide base op
		// XXX: this is broken
		build_constraints(scene, &ob->id, DEPSNODE_TYPE_TRANSFORM, "", &ob->constraints);
		
		/* operation order */
		add_relation(base_op_key, constraint_key, DEPSREL_TYPE_COMPONENT_ORDER, "[ObBase-> Constraint Stack]");
		add_relation(constraint_key, final_transform_key, DEPSREL_TYPE_COMPONENT_ORDER, "[ObConstraints -> Done]");
		
		// XXX
		add_relation(constraint_key, ob_ubereval_key, DEPSREL_TYPE_COMPONENT_ORDER, "Temp Ubereval");
		add_relation(ob_ubereval_key, final_transform_key, DEPSREL_TYPE_COMPONENT_ORDER, "Temp Ubereval");
	}
	else {
		/* operation order */
		add_relation(base_op_key, final_transform_key, DEPSREL_TYPE_COMPONENT_ORDER, "Object Transform");
		
		// XXX
		add_relation(base_op_key, ob_ubereval_key, DEPSREL_TYPE_COMPONENT_ORDER, "Temp Ubereval");
		add_relation(ob_ubereval_key, final_transform_key, DEPSREL_TYPE_COMPONENT_ORDER, "Temp Ubereval");
	}
	
	
	/* AnimData */
	build_animdata(&ob->id);
	
	// XXX: fixme
	if (ob->adt) {
		ComponentKey adt_key(&ob->id, DEPSNODE_TYPE_ANIMATION);
		add_relation(adt_key, local_transform_key, DEPSREL_TYPE_OPERATION, "Object Animation");
	}
	
	
	/* object data */
	if (ob->data) {
		ID *obdata_id = (ID *)ob->data;
		
		/* ob data animation */
		build_animdata(obdata_id);
		
		/* type-specific data... */
		switch (ob->type) {
			case OB_MESH:     /* Geometry */
			case OB_CURVE:
			case OB_FONT:
			case OB_SURF:
			case OB_MBALL:
			case OB_LATTICE:
			{
				build_obdata_geom(scene, ob);
			}
			break;
			
			
			case OB_ARMATURE: /* Pose */
				build_rig(scene, ob);
				break;
			
			case OB_LAMP:   /* Lamp */
				build_lamp(ob);
				break;
				
			case OB_CAMERA: /* Camera */
				build_camera(ob);
				break;
		}
	}
	
	/* particle systems */
	if (ob->particlesystem.first) {
		build_particles(scene, ob);
	}
}

void DepsgraphRelationBuilder::build_object_parent(Object *ob)
{
	/* XXX: for now, need to use the component key (not just direct to the parent op), or else the matrix doesn't get reset */
	//OperationKey ob_key(&ob->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_PARENT);
	ComponentKey ob_key(&ob->id, DEPSNODE_TYPE_TRANSFORM);
	
	/* type-specific links */
	switch (ob->partype) {
		case PARSKEL:  /* Armature Deform (Virtual Modifier) */
		{
			ComponentKey parent_key(&ob->parent->id, DEPSNODE_TYPE_TRANSFORM);
			add_relation(parent_key, ob_key, DEPSREL_TYPE_STANDARD, "Armature Deform Parent");
		}
		break;
			
		case PARVERT1: /* Vertex Parent */
		case PARVERT3:
		{
			ComponentKey parent_key(&ob->parent->id, DEPSNODE_TYPE_GEOMETRY);
			add_relation(parent_key, ob_key, DEPSREL_TYPE_GEOMETRY_EVAL, "Vertex Parent");
			/* XXX not sure what this is for or how you could be done properly - lukas */
			//parent_node->customdata_mask |= CD_MASK_ORIGINDEX;
		}
		break;
			
		case PARBONE: /* Bone Parent */
		{
			ComponentKey parent_key(&ob->parent->id, DEPSNODE_TYPE_BONE, ob->parsubstr);
			add_relation(parent_key, ob_key, DEPSREL_TYPE_TRANSFORM, "Bone Parent");
		}
		break;
			
		default:
		{
			if (ob->parent->type == OB_LATTICE) {
				/* Lattice Deform Parent - Virtual Modifier */
				ComponentKey parent_key(&ob->parent->id, DEPSNODE_TYPE_TRANSFORM);
				add_relation(parent_key, ob_key, DEPSREL_TYPE_STANDARD, "Lattice Deform Parent");
			}
			else if (ob->parent->type == OB_CURVE) {
				Curve *cu = (Curve *)ob->parent->data;
				
				if (cu->flag & CU_PATH) {
					/* Follow Path */
					ComponentKey parent_key(&ob->parent->id, DEPSNODE_TYPE_GEOMETRY);
					add_relation(parent_key, ob_key, DEPSREL_TYPE_TRANSFORM, "Curve Follow Parent");
					// XXX: link to geometry or object? both are needed?
					// XXX: link to timesource too?
				}
				else {
					/* Standard Parent */
					ComponentKey parent_key(&ob->parent->id, DEPSNODE_TYPE_TRANSFORM);
					add_relation(parent_key, ob_key, DEPSREL_TYPE_TRANSFORM, "Curve Parent");
				}
			}
			else {
				/* Standard Parent */
				ComponentKey parent_key(&ob->parent->id, DEPSNODE_TYPE_TRANSFORM);
				add_relation(parent_key, ob_key, DEPSREL_TYPE_TRANSFORM, "Parent");
			}
		}
		break;
	}
	
	/* exception case: parent is duplivert */
	if ((ob->type == OB_MBALL) && (ob->parent->transflag & OB_DUPLIVERTS)) {
		//dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_OB, "Duplivert");
	}
}

void DepsgraphRelationBuilder::build_constraints(Scene *scene, ID *id, eDepsNode_Type component_type, const string &component_subdata,
                                                 ListBase *constraints)
{
	OperationKey constraint_op_key(id, component_type, component_subdata,
	                               (component_type == DEPSNODE_TYPE_BONE) ? DEG_OPCODE_BONE_CONSTRAINTS : DEG_OPCODE_TRANSFORM_CONSTRAINTS);

	/* add dependencies for each constraint in turn */
	for (bConstraint *con = (bConstraint *)constraints->first; con; con = con->next) {
		bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

		/* invalid constraint type... */
		if (cti == NULL)
			continue;

		/* special case for camera tracking -- it doesn't use targets to define relations */
		// TODO: we can now represent dependencies in a much richer manner, so review how this is done...
		if (ELEM(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_CAMERASOLVER, CONSTRAINT_TYPE_OBJECTSOLVER)) {
			bool depends_on_camera = false;
			
			if (cti->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
				bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;

				if (((data->clip) || (data->flag & FOLLOWTRACK_ACTIVECLIP)) && data->track[0])
					depends_on_camera = true;
				
				if (data->depth_ob) {
					// DAG_RL_DATA_OB | DAG_RL_OB_OB
					ComponentKey depth_key(&data->depth_ob->id, DEPSNODE_TYPE_TRANSFORM);
					add_relation(depth_key, constraint_op_key, DEPSREL_TYPE_TRANSFORM, cti->name);
				}
			}
			else if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
				depends_on_camera = true;
			}

			if (depends_on_camera && scene->camera) {
				// DAG_RL_DATA_OB | DAG_RL_OB_OB
				ComponentKey camera_key(&scene->camera->id, DEPSNODE_TYPE_TRANSFORM);
				add_relation(camera_key, constraint_op_key, DEPSREL_TYPE_TRANSFORM, cti->name);
			}
			
			/* tracker <-> constraints */
			// FIXME: actually motionclip dependency on results of motionclip block here...
			//dag_add_relation(dag, scenenode, node, DAG_RL_SCENE, "Scene Relation");
		}
		else if (cti->get_constraint_targets) {
			ListBase targets = {NULL, NULL};
			cti->get_constraint_targets(con, &targets);
			
			for (bConstraintTarget *ct = (bConstraintTarget *)targets.first; ct; ct = ct->next) {
				if (!ct->tar)
					continue;
				
				if (ELEM(con->type, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_SPLINEIK)) {
					/* ignore IK constraints - these are handled separately (on pose level) */
				}
				else if (ELEM(con->type, CONSTRAINT_TYPE_FOLLOWPATH, CONSTRAINT_TYPE_CLAMPTO)) {
					/* these constraints require path geometry data... */
					ComponentKey target_key(&ct->tar->id, DEPSNODE_TYPE_GEOMETRY);
					add_relation(target_key, constraint_op_key, DEPSREL_TYPE_GEOMETRY_EVAL, cti->name); // XXX: type = geom_transform
					// TODO: path dependency
				}
				else if ((ct->tar->type == OB_ARMATURE) && (ct->subtarget[0])) {
					/* bone */
					OperationKey target_key(&ct->tar->id, DEPSNODE_TYPE_BONE, ct->subtarget, DEG_OPCODE_BONE_DONE); // XXX: ik vs normal
					add_relation(target_key, constraint_op_key, DEPSREL_TYPE_TRANSFORM, cti->name);
				}
				else if (ELEM(ct->tar->type, OB_MESH, OB_LATTICE) && (ct->subtarget[0])) {
					/* vertex group */
					/* NOTE: for now, we don't need to represent vertex groups separately... */
					ComponentKey target_key(&ct->tar->id, DEPSNODE_TYPE_GEOMETRY);
					add_relation(target_key, constraint_op_key, DEPSREL_TYPE_GEOMETRY_EVAL, cti->name);
					
					if (ct->tar->type == OB_MESH) {
						//node2->customdata_mask |= CD_MASK_MDEFORMVERT;
					}
				}
				else if (con->type == CONSTRAINT_TYPE_SHRINKWRAP) {
					/* Constraints which requires the target object surface. */
					/* TODO(sergey): More cosntarints here? */
					ComponentKey target_key(&ct->tar->id, DEPSNODE_TYPE_GEOMETRY);
					add_relation(target_key, constraint_op_key, DEPSREL_TYPE_TRANSFORM, cti->name);
				}
				else {
					/* standard object relation */
					// TODO: loc vs rot vs scale?
					/* TODO(sergey): What to do if target is self? */
					if (&ct->tar->id != id) {
						OperationKey target_key(&ct->tar->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_FINAL);
						add_relation(target_key, constraint_op_key, DEPSREL_TYPE_TRANSFORM, cti->name);
					}
				}
			}
			
			if (cti->flush_constraint_targets)
				cti->flush_constraint_targets(con, &targets, 1);
		}
	}
}

void DepsgraphRelationBuilder::build_animdata(ID *id)
{
	AnimData *adt = BKE_animdata_from_id(id);
	
	if (adt == NULL)
		return;
	
	ComponentKey adt_key(id, DEPSNODE_TYPE_ANIMATION);
	
	/* animation */
	if (adt->action || adt->nla_tracks.first) {
		/* wire up dependency to time source */
		TimeSourceKey time_src_key;
		add_relation(time_src_key, adt_key, DEPSREL_TYPE_TIME, "[TimeSrc -> Animation]");
		
		// XXX: Hook up specific update callbacks for special properties which may need it...
		
		// XXX: animdata "hierarchy" - top-level overrides need to go after lower-down
	}
	
	/* drivers */
	for (FCurve *fcu = (FCurve *)adt->drivers.first; fcu; fcu = fcu->next) {
		OperationKey driver_key(id, DEPSNODE_TYPE_PARAMETERS, DEG_OPCODE_DRIVER, fcu->rna_path);
		
		/* create the driver's relations to targets */
		build_driver(id, fcu);
		
		/* prevent driver from occurring before own animation... */
		if (adt->action || adt->nla_tracks.first) {
			add_relation(adt_key, driver_key, DEPSREL_TYPE_OPERATION, 
						 "[AnimData Before Drivers]");
		}
	}
}

void DepsgraphRelationBuilder::build_driver(ID *id, FCurve *fcu)
{
	ChannelDriver *driver = fcu->driver;
	OperationKey driver_key(id, DEPSNODE_TYPE_PARAMETERS, DEG_OPCODE_DRIVER, deg_fcurve_id_name(fcu));
	
	/* create dependency between driver and data affected by it */
	/* - direct property relationship... */
	RNAPathKey affected_key(id, fcu->rna_path);
	add_relation(driver_key, affected_key, DEPSREL_TYPE_DRIVER, "[Driver -> Data] DepsRel");
	
	/* hook up update callback associated with F-Curve */
	// ...
	
	/* driver -> data components (for interleaved evaluation - bones/constraints/modifiers) */
	// XXX: this probably shouldn't be inlined here like this...
	if (strstr(fcu->rna_path, "pose.bones[") != NULL) {
		/* interleaved drivers during bone eval */
		// TODO: ideally, if this is for a constraint, it goes to said constraint
		Object *ob = (Object *)id;
		bPoseChannel *pchan;
		char *bone_name;
		
		bone_name = BLI_str_quoted_substrN(fcu->rna_path, "pose.bones[");
		pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
		
		if (bone_name) {
			MEM_freeN(bone_name);
			bone_name = NULL;
		}
		
		if (pchan) {
			OperationKey bone_key(id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_LOCAL);
			add_relation(driver_key, bone_key, DEPSREL_TYPE_DRIVER, "[Driver -> Bone]");
		}
		else {
			printf("Couldn't find bone name for driver path - '%s'\n", fcu->rna_path);
		}
	}
	else if (GS(id->name) == ID_OB && strstr(fcu->rna_path, "modifiers[")) {
		/* modifier driver - connect directly to the modifier */
		char *modifier_name = BLI_str_quoted_substrN(fcu->rna_path, "modifiers[");
		if (modifier_name) {
			OperationKey modifier_key(id, DEPSNODE_TYPE_GEOMETRY, DEG_OPCODE_GEOMETRY_MODIFIER, modifier_name);
			add_relation(driver_key, modifier_key, DEPSREL_TYPE_DRIVER, "[Driver -> Modifier]");
			
			MEM_freeN(modifier_name);
		}
	}
	else if (GS(id->name) == ID_KE && strstr(fcu->rna_path, "key_blocks[")) {
		/* shape key driver - hook into the base geometry operation */
		// XXX: double check where this points
		Key *shape_key = (Key *)id;
		
		ComponentKey geometry_key(shape_key->from, DEPSNODE_TYPE_GEOMETRY);
		add_relation(driver_key, geometry_key, DEPSREL_TYPE_DRIVER, "[Driver -> ShapeKey Geom]");
	}
	else {
		if (GS(id->name) == ID_OB) {
			/* assume that driver affects a transform... */
			OperationKey local_transform_key(id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_LOCAL);
			add_relation(driver_key, local_transform_key, DEPSREL_TYPE_OPERATION, "[Driver -> Transform]");
		}
	}
	
	/* ensure that affected prop's update callbacks will be triggered once done */
	// TODO: implement this once the functionality to add these links exists in RNA
	// XXX: the data itself could also set this, if it were to be truly initialised later?
	
	/* loop over variables to get the target relationships */
	for (DriverVar *dvar = (DriverVar *)driver->variables.first; dvar; dvar = dvar->next) {
		/* only used targets */
		DRIVER_TARGETS_USED_LOOPER(dvar) 
		{
			if (dtar->id == NULL)
				continue;
			
			/* special handling for directly-named bones */
			if ((dtar->flag & DTAR_FLAG_STRUCT_REF) && (dtar->pchan_name[0])) {
				Object *ob = (Object *)dtar->id;
				bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, dtar->pchan_name);
				
				if (pchan != NULL) {
					/* get node associated with bone */
					OperationKey target_key(dtar->id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_DONE);
					add_relation(target_key, driver_key, DEPSREL_TYPE_DRIVER_TARGET, "[Bone Target -> Driver]");
				}
			}
			else if (dtar->flag & DTAR_FLAG_STRUCT_REF) {
				/* get node associated with the object's transforms */
				OperationKey target_key(dtar->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_FINAL);
				add_relation(target_key, driver_key, DEPSREL_TYPE_DRIVER_TARGET, "[Bone Target -> Driver]");
			}
			else {
				/* resolve path to get node */
				RNAPathKey target_key(dtar->id, dtar->rna_path ? dtar->rna_path : "");
				add_relation(target_key, driver_key, DEPSREL_TYPE_DRIVER_TARGET, "[RNA Target -> Driver]");
			}
		}
		DRIVER_TARGETS_LOOPER_END
	}
}

void DepsgraphRelationBuilder::build_world(Scene *scene, World *world)
{
	/* Prevent infinite recursion by checking (and tagging the world) as having been visited 
	 * already. This assumes wo->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	ID *world_id = &world->id;
	if (id_is_tagged(world_id))
		return;
	id_tag_set(world_id);
	
	build_animdata(world_id);
	
	/* TODO: other settings? */
	
	/* textures */
	build_texture_stack(world_id, world->mtex);
	
	/* world's nodetree */
	build_nodetree(world_id, world->nodetree);

	id_tag_clear(world_id);
}

void DepsgraphRelationBuilder::build_rigidbody(Scene *scene)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	
	OperationKey init_key(&scene->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_PLACEHOLDER, "Rigidbody World Rebuild"); // XXX
	OperationKey sim_key(&scene->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_PLACEHOLDER, "Rigidbody World Do Simulation"); // XXX
	
	/* rel between the two sim-nodes */
	add_relation(init_key, sim_key, DEPSREL_TYPE_OPERATION, "Rigidbody [Init -> SimStep]");
	
	/* set up dependencies between these operations and other builtin nodes --------------- */	
	
	/* time dependency */
	TimeSourceKey time_src_key;
	add_relation(time_src_key, init_key, DEPSREL_TYPE_TIME, "TimeSrc -> Rigidbody Reset/Rebuild (Optional)");
	add_relation(time_src_key, sim_key, DEPSREL_TYPE_TIME, "TimeSrc -> Rigidbody Sim Step");
	
	/* objects - simulation participants */
	if (rbw->group) {
		for (GroupObject *go = (GroupObject *)rbw->group->gobject.first; go; go = go->next) {
			Object *ob = go->ob;
			if (!ob || ob->type != OB_MESH)
				continue;
			
			/* hook up evaluation order... 
			 * 1) flushing rigidbody results follows base transforms being applied
			 * 2) rigidbody flushing can only be performed after simulation has been run
			 *
			 * 3) simulation needs to know base transforms to figure out what to do
			 *    XXX: there's probably a difference between passive and active 
			 *         - passive don't change, so may need to know full transform...
			 */
			OperationKey rbo_key(&ob->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_RIGIDBODY);
			
			eDepsOperation_Code trans_opcode = ob->parent ? DEG_OPCODE_TRANSFORM_PARENT : DEG_OPCODE_TRANSFORM_LOCAL;
			OperationKey trans_op(&ob->id, DEPSNODE_TYPE_TRANSFORM, trans_opcode);
			
			add_relation(trans_op, rbo_key, DEPSREL_TYPE_OPERATION, "Base Ob Transform -> RBO Sync");
			add_relation(sim_key, rbo_key, DEPSREL_TYPE_COMPONENT_ORDER, "Rigidbody Sim Eval -> RBO Sync");
			
			OperationKey constraint_key(&ob->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_CONSTRAINTS);
			add_relation(rbo_key, constraint_key, DEPSREL_TYPE_COMPONENT_ORDER, "RBO Sync -> Ob Constraints");
			
			/* needed to get correct base values */
			add_relation(trans_op, sim_key, DEPSREL_TYPE_OPERATION, "Base Ob Transform -> Rigidbody Sim Eval");
		}
	}
	
	/* constraints */
	if (rbw->constraints) {
		for (GroupObject *go = (GroupObject *)rbw->constraints->gobject.first; go; go = go->next) {
			Object *ob = go->ob;
			if (!ob || !ob->rigidbody_constraint)
				continue;
			
			RigidBodyCon *rbc = ob->rigidbody_constraint;
			
			/* final result of the constraint object's transform controls how the
			 * constraint affects the physics sim for these objects 
			 */
			ComponentKey trans_key(&ob->id, DEPSNODE_TYPE_TRANSFORM);
			OperationKey ob1_key(&rbc->ob1->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_RIGIDBODY);
			OperationKey ob2_key(&rbc->ob2->id, DEPSNODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_RIGIDBODY);
			
			/* - constrained-objects sync depends on the constraint-holder */
			add_relation(trans_key, ob1_key, DEPSREL_TYPE_TRANSFORM, "RigidBodyConstraint -> RBC.Object_1");
			add_relation(trans_key, ob2_key, DEPSREL_TYPE_TRANSFORM, "RigidBodyConstraint -> RBC.Object_2");
			
			/* - ensure that sim depends on this constraint's transform */
			add_relation(trans_key, sim_key, DEPSREL_TYPE_TRANSFORM, "RigidBodyConstraint Transform -> RB Simulation");
		}
	}
}

void DepsgraphRelationBuilder::build_particles(Scene *scene, Object *ob)
{
	/* particle systems */
	for (ParticleSystem *psys = (ParticleSystem *)ob->particlesystem.first; psys; psys = psys->next) {
		ParticleSettings *part = psys->part;
		
		/* particle settings */
		build_animdata(&part->id);
		
		/* this particle system */
		OperationKey psys_key(&ob->id, DEPSNODE_TYPE_EVAL_PARTICLES, DEG_OPCODE_PSYS_EVAL);
		
		/* XXX: if particle system is later re-enabled, we must do full rebuild? */
		if (!psys_check_enabled(ob, psys))
			continue;
		
#if 0
		if (ELEM(part->phystype, PART_PHYS_KEYED, PART_PHYS_BOIDS)) {
			ParticleTarget *pt;

			for (pt = psys->targets.first; pt; pt = pt->next) {
				if (pt->ob && BLI_findlink(&pt->ob->particlesystem, pt->psys - 1)) {
					node2 = dag_get_node(dag, pt->ob);
					dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Particle Targets");
				}
			}
		}
		
		if (part->ren_as == PART_DRAW_OB && part->dup_ob) {
			node2 = dag_get_node(dag, part->dup_ob);
			/* note that this relation actually runs in the wrong direction, the problem
			 * is that dupli system all have this (due to parenting), and the render
			 * engine instancing assumes particular ordering of objects in list */
			dag_add_relation(dag, node, node2, DAG_RL_OB_OB, "Particle Object Visualization");
			if (part->dup_ob->type == OB_MBALL)
				dag_add_relation(dag, node, node2, DAG_RL_DATA_DATA, "Particle Object Visualization");
		}
		
		if (part->ren_as == PART_DRAW_GR && part->dup_group) {
			for (go = part->dup_group->gobject.first; go; go = go->next) {
				node2 = dag_get_node(dag, go->ob);
				dag_add_relation(dag, node2, node, DAG_RL_OB_OB, "Particle Group Visualization");
			}
		}
#endif
		
		/* effectors */
		ListBase *effectors = pdInitEffectors(scene, ob, psys, part->effector_weights, false);
		
		if (effectors) {
			for (EffectorCache *eff = (EffectorCache *)effectors->first; eff; eff = eff->next) {
				if (eff->psys) {
					// XXX: DAG_RL_DATA_DATA | DAG_RL_OB_DATA
					ComponentKey eff_key(&eff->ob->id, DEPSNODE_TYPE_GEOMETRY); // xxx: particles instead?
					add_relation(eff_key, psys_key, DEPSREL_TYPE_STANDARD, "Particle Field");
				}
			}
		}
		
		pdEndEffectors(&effectors);
		
		/* boids */
		if (part->boids) {
			BoidRule *rule = NULL;
			BoidState *state = NULL;
			
			for (state = (BoidState *)part->boids->states.first; state; state = state->next) {
				for (rule = (BoidRule *)state->rules.first; rule; rule = rule->next) {
					Object *ruleob = NULL;
					if (rule->type == eBoidRuleType_Avoid)
						ruleob = ((BoidRuleGoalAvoid *)rule)->ob;
					else if (rule->type == eBoidRuleType_FollowLeader)
						ruleob = ((BoidRuleFollowLeader *)rule)->ob;

					if (ruleob) {
						ComponentKey ruleob_key(&ruleob->id, DEPSNODE_TYPE_TRANSFORM);
						add_relation(ruleob_key, psys_key, DEPSREL_TYPE_TRANSFORM, "Boid Rule");
					}
				}
			}
		}
	}
	
	/* pointcache */
	// TODO...
}

/* IK Solver Eval Steps */
void DepsgraphRelationBuilder::build_ik_pose(Object *ob,
                                             bPoseChannel *pchan,
                                             bConstraint *con,
                                             RootPChanMap *root_map)
{
	bKinematicConstraint *data = (bKinematicConstraint *)con->data;
	
	/* attach owner to IK Solver too 
	 * - assume that owner is always part of chain 
	 * - see notes on direction of rel below...
	 */
	bPoseChannel *rootchan = BKE_armature_ik_solver_find_root(pchan, data);
	OperationKey transforms_key = bone_transforms_key(ob, pchan);
	OperationKey solver_key(&ob->id, DEPSNODE_TYPE_EVAL_POSE, rootchan->name, DEG_OPCODE_POSE_IK_SOLVER);
	add_relation(transforms_key, solver_key, DEPSREL_TYPE_TRANSFORM, "IK Solver Owner");

	if (data->tar != NULL) {
		/* TODO(sergey): For until we'll store partial matricies in the depsgraph,
		 * we create dependency bewteen target object and pose eval component.
		 *
		 * This way we ensuring the whole subtree is updated from sctratch without
		 * need of intermediate matricies. This is an overkill, but good enough for
		 * testing IK solver.
		 */
		ComponentKey pose_key(&ob->id, DEPSNODE_TYPE_EVAL_POSE);
		if ((data->tar->type == OB_ARMATURE) && (data->subtarget[0])) {
			/* TODO(sergey): This is only for until granular update stores intermediate result. */
			if (data->tar != ob) {
				ComponentKey target_key(&data->tar->id, DEPSNODE_TYPE_BONE, data->subtarget);
				add_relation(target_key, pose_key, DEPSREL_TYPE_TRANSFORM, con->name);
			}
			else {
				ComponentKey target_key(&data->tar->id, DEPSNODE_TYPE_BONE, data->subtarget);
				add_relation(target_key, solver_key, DEPSREL_TYPE_TRANSFORM, con->name);
			}
		}
		else {
			ComponentKey target_key(&data->tar->id, DEPSNODE_TYPE_TRANSFORM);
			add_relation(target_key, pose_key, DEPSREL_TYPE_TRANSFORM, con->name);
		}
	}
	root_map_add_bone(pchan->name, rootchan->name, root_map);

	bPoseChannel *parchan = pchan;
	/* exclude tip from chain? */
	if (!(data->flag & CONSTRAINT_IK_TIP))
		parchan = pchan->parent;
	
	/* Walk to the chain's root */
	//size_t segcount = 0;
	int segcount = 0;
	
	while (parchan) {
		/* Make IK-solver dependent on this bone's result,
		 * since it can only run after the standard results 
		 * of the bone are know. Validate links step on the 
		 * bone will ensure that users of this bone only
		 * grab the result with IK solver results...
		 */
		if (parchan != pchan) {
			OperationKey parent_key = bone_transforms_key(ob, parchan);
			add_relation(parent_key, solver_key, DEPSREL_TYPE_TRANSFORM, "IK Chain Parent");
		}
		parchan->flag |= POSE_DONE;

		OperationKey final_transforms_key(&ob->id, DEPSNODE_TYPE_BONE, parchan->name, DEG_OPCODE_BONE_DONE);
		add_relation(solver_key, final_transforms_key, DEPSREL_TYPE_TRANSFORM, "IK Solver Result");

		root_map_add_bone(parchan->name, rootchan->name, root_map);

		/* continue up chain, until we reach target number of items... */
		segcount++;
		if ((segcount == data->rootbone) || (segcount > 255)) break;  /* 255 is weak */
		
		parchan  = parchan->parent;
	}

	OperationKey flush_key(&ob->id, DEPSNODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_DONE);
	add_relation(solver_key, flush_key, DEPSREL_TYPE_OPERATION, "PoseEval Result-Bone Link");
}

/* Spline IK Eval Steps */
void DepsgraphRelationBuilder::build_splineik_pose(Object *ob,
                                                   bPoseChannel *pchan,
                                                   bConstraint *con,
                                                   RootPChanMap *root_map)
{
	bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
	bPoseChannel *rootchan = BKE_armature_splineik_solver_find_root(pchan, data);
	OperationKey transforms_key = bone_transforms_key(ob, pchan);
	OperationKey solver_key(&ob->id, DEPSNODE_TYPE_EVAL_POSE, rootchan->name, DEG_OPCODE_POSE_SPLINE_IK_SOLVER);
	
	/* attach owner to IK Solver too 
	 * - assume that owner is always part of chain 
	 * - see notes on direction of rel below...
	 */
	add_relation(transforms_key, solver_key, DEPSREL_TYPE_TRANSFORM, "Spline IK Solver Owner");
	
	/* attach path dependency to solver */
	if (data->tar) {
		/* TODO(sergey): For until we'll store partial matricies in the depsgraph,
		 * we create dependency bewteen target object and pose eval component.
		 * See IK pose for a bit more information.
		 */
		ComponentKey target_key(&data->tar->id, DEPSNODE_TYPE_GEOMETRY);
		ComponentKey pose_key(&ob->id, DEPSNODE_TYPE_EVAL_POSE);
		add_relation(target_key, pose_key, DEPSREL_TYPE_TRANSFORM,"[Curve.Path -> Spline IK] DepsRel");
	}

	pchan->flag |= POSE_DONE;
	OperationKey final_transforms_key(&ob->id, DEPSNODE_TYPE_BONE, pchan->name, "Bone Final Transforms");
	add_relation(solver_key, final_transforms_key, DEPSREL_TYPE_TRANSFORM, "IK Solver Result");

	root_map_add_bone(pchan->name, rootchan->name, root_map);

	/* Walk to the chain's root */
	//size_t segcount = 0;
	int segcount = 0;
	
	for (bPoseChannel *parchan = pchan->parent; parchan; parchan = parchan->parent) {
		/* Make Spline IK solver dependent on this bone's result,
		 * since it can only run after the standard results 
		 * of the bone are know. Validate links step on the 
		 * bone will ensure that users of this bone only
		 * grab the result with IK solver results...
		 */
		if (parchan != pchan) {
			OperationKey parent_key = bone_transforms_key(ob, parchan);
			add_relation(parent_key, solver_key, DEPSREL_TYPE_TRANSFORM, "Spline IK Solver Update");
		}
		parchan->flag |= POSE_DONE;

		OperationKey final_transforms_key(&ob->id, DEPSNODE_TYPE_BONE, parchan->name, DEG_OPCODE_BONE_DONE);
		add_relation(solver_key, final_transforms_key, DEPSREL_TYPE_TRANSFORM, "IK Solver Result");

		root_map_add_bone(parchan->name, rootchan->name, root_map);

		/* continue up chain, until we reach target number of items... */
		segcount++;
		if ((segcount == data->chainlen) || (segcount > 255)) break;  /* 255 is weak */
	}

	OperationKey flush_key(&ob->id, DEPSNODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_DONE);
	add_relation(solver_key, flush_key, DEPSREL_TYPE_OPERATION, "PoseEval Result-Bone Link");
}

/* Pose/Armature Bones Graph */
void DepsgraphRelationBuilder::build_rig(Scene *scene, Object *ob)
{
	bArmature *arm = (bArmature *)ob->data;
	
	/* Armature-Data */
	// TODO: selection status?
	
	/* animation and/or drivers linking posebones to base-armature used to define them */
	// TODO: we need a bit of an exception here to redirect drivers to posebones?
	build_animdata(&arm->id);
	
	/* attach links between base operations */
	OperationKey init_key(&ob->id, DEPSNODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_INIT);
	OperationKey flush_key(&ob->id, DEPSNODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_DONE);
	
	add_relation(init_key, flush_key, DEPSREL_TYPE_COMPONENT_ORDER, "[Pose Init -> Pose Cleanup]");

	if (ob->adt != NULL) {
		ComponentKey animation_key(&ob->id, DEPSNODE_TYPE_ANIMATION);
		add_relation(animation_key, init_key, DEPSREL_TYPE_OPERATION, "Object Animation");
	}

	/* bones */
	for (bPoseChannel *pchan = (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		OperationKey bone_local_key(&ob->id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_LOCAL);
		OperationKey bone_pose_key(&ob->id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_POSE_PARENT);
		OperationKey bone_done_key(&ob->id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_DONE);
		
		pchan->flag &= ~POSE_DONE;
		
		/* bone parent */
		if (pchan->parent == NULL) {
			/* link bone/component to pose "sources" if it doesn't have any obvious dependencies */
			add_relation(init_key, bone_local_key, DEPSREL_TYPE_OPERATION, "PoseEval Source-Bone Link");
		}
		else {
			/* link bone/component to parent bone (see next loop) */
		}
		
		/* local to pose parenting operation */
		add_relation(bone_local_key, bone_pose_key, DEPSREL_TYPE_OPERATION, "Bone Local - PoseSpace Link");
		
		/* constraints */
		if (pchan->constraints.first != NULL) {
			build_constraints(scene, &ob->id, DEPSNODE_TYPE_BONE, pchan->name, &pchan->constraints);
			
			OperationKey constraints_key(&ob->id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_CONSTRAINTS);
			add_relation(bone_pose_key, constraints_key, DEPSREL_TYPE_OPERATION, "Constraints Stack");
		}
		
		/* TODO(sergey): Assume for now that pose flush depends on all the pose channels. */
		add_relation(bone_done_key, flush_key, DEPSREL_TYPE_OPERATION, "PoseEval Result-Bone Link");
	}
	
	/* IK Solvers...
	 * - These require separate processing steps are pose-level
	 *   to be executed between chains of bones (i.e. once the
	 *   base transforms of a bunch of bones is done)
	 *
	 * Unsolved Issues:
	 * - Care is needed to ensure that multi-headed trees work out the same as in ik-tree building
	 * - Animated chain-lengths are a problem...
	 */
	RootPChanMap root_map;
	for (bPoseChannel *pchan = (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		for (bConstraint *con = (bConstraint *)pchan->constraints.first; con; con = con->next) {
			switch (con->type) {
				case CONSTRAINT_TYPE_KINEMATIC:
					build_ik_pose(ob, pchan, con, &root_map);
					break;
					
				case CONSTRAINT_TYPE_SPLINEIK:
					build_splineik_pose(ob, pchan, con, &root_map);
					break;
					
				default:
					break;
			}
		}
	}

	for (bPoseChannel *pchan = (bPoseChannel *)ob->pose->chanbase.first;
	     pchan != NULL;
	     pchan = pchan->next)
	{
		/* bone parent */
		// FIXME: this code is broken
		if (pchan->parent != NULL) {
			OperationKey bone_key(&ob->id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_POSE_PARENT);
			bool has_common_root = false;
			if (pchan->flag & POSE_DONE) {
				has_common_root = pchan_check_common_solver_root(root_map,
				                                                 pchan->name,
				                                                 pchan->parent->name);
			}
			if (has_common_root) {
				fprintf(stderr, "common root: %s (par = %s)\n", pchan->name, pchan->parent->name);
				
				OperationKey parent_transforms_key = bone_transforms_key(ob, pchan->parent); // XXX: does this settle for pre-IK?
				add_relation(parent_transforms_key, bone_key, DEPSREL_TYPE_TRANSFORM, "[Parent Bone -> Child Bone]");
			}
			else {
				fprintf(stderr, "not common root: %s (par = %s)\n", pchan->name, pchan->parent->name);
				
				OperationKey parent_key(&ob->id, DEPSNODE_TYPE_BONE, pchan->parent->name, DEG_OPCODE_BONE_DONE);
				add_relation(parent_key, bone_key, DEPSREL_TYPE_TRANSFORM, "[Parent Bone -> Child Bone]");
			}
		}
		
		OperationKey final_transforms_key(&ob->id, DEPSNODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_DONE);
		if ((pchan->flag & POSE_DONE) == 0) {
			OperationKey transforms_key = bone_transforms_key(ob, pchan);
			add_relation(transforms_key, final_transforms_key, DEPSREL_TYPE_TRANSFORM, "Bone Final Transforms");
		}
	}
}

/* Shapekeys */
void DepsgraphRelationBuilder::build_shapekeys(ID *obdata, Key *key)
{
	build_animdata(&key->id);
	
	/* attach to geometry */
	// XXX: aren't shapekeys now done as a pseudo-modifier on object?
	ComponentKey obdata_key(obdata, DEPSNODE_TYPE_GEOMETRY);
	ComponentKey key_key(&key->id, DEPSNODE_TYPE_GEOMETRY);
	add_relation(key_key, obdata_key, DEPSREL_TYPE_GEOMETRY_EVAL, "Shapekeys");
}

/* ObData Geometry Evaluation */
// XXX: what happens if the datablock is shared!
void DepsgraphRelationBuilder::build_obdata_geom(Scene *scene, Object *ob)
{
	ID *obdata = (ID *)ob->data;
	
	/* get nodes for result of obdata's evaluation, and geometry evaluation on object */
	ComponentKey geom_key(&ob->id, DEPSNODE_TYPE_GEOMETRY);
	ComponentKey obdata_geom_key(obdata, DEPSNODE_TYPE_GEOMETRY);
	OperationKey geom_eval_key(&ob->id, DEPSNODE_TYPE_GEOMETRY, "Geometry Eval");
	
	/* link components to each other */
	add_relation(obdata_geom_key, geom_key, DEPSREL_TYPE_DATABLOCK, "Object Geometry Base Data");

	/* type-specific node/links */
	switch (ob->type) {
		case OB_MESH:
			break;
		
		case OB_MBALL: 
		{
			Object *mom = BKE_mball_basis_find(scene, ob);
			
			/* motherball - mom depends on children! */
			if (mom != ob) {
				/* non-motherball -> cannot be directly evaluated! */
				ComponentKey mom_key(&mom->id, DEPSNODE_TYPE_GEOMETRY);
				add_relation(geom_key, mom_key, DEPSREL_TYPE_GEOMETRY_EVAL, "Metaball Motherball");
			}
		}
		break;
		
		case OB_CURVE:
		case OB_FONT:
		{
			Curve *cu = (Curve *)obdata;
			
			/* curve's dependencies */
			// XXX: these needs geom data, but where is geom stored?
			if (cu->bevobj) {
				ComponentKey bevob_key(&cu->bevobj->id, DEPSNODE_TYPE_GEOMETRY);
				add_relation(bevob_key, geom_key, DEPSREL_TYPE_GEOMETRY_EVAL, "Curve Bevel");
			}
			if (cu->taperobj) {
				ComponentKey taperob_key(&cu->taperobj->id, DEPSNODE_TYPE_GEOMETRY);
				add_relation(taperob_key, geom_key, DEPSREL_TYPE_GEOMETRY_EVAL, "Curve Taper");
			}
			if (ob->type == OB_FONT) {
				if (cu->textoncurve) {
					ComponentKey textoncurve_key(&cu->taperobj->id, DEPSNODE_TYPE_GEOMETRY);
					add_relation(textoncurve_key, geom_key, DEPSREL_TYPE_GEOMETRY_EVAL, "Text on Curve");
				}
			}
		}
		break;
		
		case OB_SURF: /* Nurbs Surface */
		{
		}
		break;
		
		case OB_LATTICE: /* Lattice */
		{
		}
		break;
	}
	
	/* ShapeKeys */
	Key *key = BKE_key_from_object(ob);
	if (key)
		build_shapekeys(obdata, key);
	
	/* Modifiers */
	if (ob->modifiers.first) {
		ModifierData *md;
		OperationKey prev_mod_key;
		
		for (md = (ModifierData *)ob->modifiers.first; md; md = md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo((ModifierType)md->type);
			OperationKey mod_key(&ob->id, DEPSNODE_TYPE_GEOMETRY, DEG_OPCODE_GEOMETRY_MODIFIER, md->name);

			if (md->prev) {
				/* Stack relation: modifier depends on previous modifier in the stack */
				add_relation(prev_mod_key, mod_key, DEPSREL_TYPE_GEOMETRY_EVAL, "Modifier Stack");
			}
			else {
				/* Stack relation: first modifier depends on the geometry. */
				add_relation(geom_eval_key, mod_key, DEPSREL_TYPE_GEOMETRY_EVAL, "Modifier Stack");
			}

			if (mti->updateDepsgraph) {
				DepsNodeHandle handle = create_node_handle(mod_key);
				mti->updateDepsgraph(md, scene, ob, &handle);
			}

			if (modifier_check_depends_on_time(ob, md)) {
				TimeSourceKey time_src_key;
				add_relation(time_src_key, mod_key, DEPSREL_TYPE_TIME, "Time Source");
			}

			prev_mod_key = mod_key;
		}
	}
	
	/* materials */
	if (ob->totcol) {
		int a;
		
		for (a = 1; a <= ob->totcol; a++) {
			Material *ma = give_current_material(ob, a);
			
			if (ma)
				build_material(&ob->id, ma);
		}
	}
	
	/* geometry collision */
	if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_LATTICE)) {
		// add geometry collider relations
	}

	/* Make sure uber update is the last in the dependencies.
	 *
	 * TODO(sergey): Get rid of this node.
	 */
	if (ob->type != OB_ARMATURE) {
		/* Armatures does no longer require uber node. */
		OperationKey obdata_ubereval_key(&ob->id, DEPSNODE_TYPE_GEOMETRY, DEG_OPCODE_GEOMETRY_UBEREVAL);
		if (ob->modifiers.last) {
			ModifierData *md = (ModifierData *)ob->modifiers.last;
			OperationKey mod_key(&ob->id, DEPSNODE_TYPE_GEOMETRY, DEG_OPCODE_GEOMETRY_MODIFIER, md->name);
			add_relation(mod_key, obdata_ubereval_key, DEPSREL_TYPE_OPERATION, "Object Geometry UberEval");
		}
		else {
			add_relation(geom_eval_key, obdata_ubereval_key, DEPSREL_TYPE_OPERATION, "Object Geometry UberEval");
		}
	}
}

/* Cameras */
// TODO: Link scene-camera links in somehow...
void DepsgraphRelationBuilder::build_camera(Object *ob)
{
	Camera *cam = (Camera *)ob->data;
	ComponentKey param_key(&cam->id, DEPSNODE_TYPE_PARAMETERS);
	
	/* DOF */
	if (cam->dof_ob) {
		ComponentKey dof_ob_key(&cam->dof_ob->id, DEPSNODE_TYPE_TRANSFORM);
		add_relation(dof_ob_key, param_key, DEPSREL_TYPE_TRANSFORM, "Camera DOF");
	}
}

/* Lamps */
void DepsgraphRelationBuilder::build_lamp(Object *ob)
{
	Lamp *la = (Lamp *)ob->data;
	ID *lamp_id = &la->id;

	/* Prevent infinite recursion by checking (and tagging the lamp) as having been visited 
	 * already. This assumes la->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (id_is_tagged(lamp_id))
		return;
	id_tag_set(lamp_id);
	
	/* lamp's nodetree */
	if (la->nodetree) {
		build_nodetree(lamp_id, la->nodetree);
	}
	
	/* textures */
	build_texture_stack(lamp_id, la->mtex);
	
	id_tag_clear(lamp_id);
}

void DepsgraphRelationBuilder::build_nodetree(ID *owner, bNodeTree *ntree)
{
	if (!ntree)
		return;
	
	build_animdata(&ntree->id);
	
	/* nodetree's nodes... */
	for (bNode *bnode = (bNode *)ntree->nodes.first; bnode; bnode = bnode->next) {
		if (bnode->id) {
			if (GS(bnode->id->name) == ID_MA) {
				build_material(owner, (Material *)bnode->id);
			}
			else if (bnode->type == ID_TE) {
				build_texture(owner, (Tex *)bnode->id);
			}
			else if (bnode->type == NODE_GROUP) {
				build_nodetree(owner, (bNodeTree *)bnode->id);
			}
		}
	}
	
	// TODO: link from nodetree to owner_component?
}

/* Recursively build graph for material */
void DepsgraphRelationBuilder::build_material(ID *owner, Material *ma)
{
	/* Prevent infinite recursion by checking (and tagging the material) as having been visited 
	 * already. This assumes ma->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	ID *ma_id = &ma->id;
	if (id_is_tagged(ma_id))
		return;
	id_tag_set(ma_id);
	
	build_animdata(ma_id);
	
	/* textures */
	build_texture_stack(owner, ma->mtex);
	
	/* material's nodetree */
	build_nodetree(owner, ma->nodetree);
	
	id_tag_clear(ma_id);
}

/* Recursively build graph for texture */
void DepsgraphRelationBuilder::build_texture(ID *owner, Tex *tex)
{
	/* Prevent infinite recursion by checking (and tagging the texture) as having been visited 
	 * already. This assumes tex->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	ID *tex_id = &tex->id;
	if (id_is_tagged(tex_id))
		return;
	id_tag_set(tex_id);
	
	/* texture itself */
	build_animdata(tex_id);
	
	/* texture's nodetree */
	build_nodetree(owner, tex->nodetree);
	
	id_tag_clear(tex_id);
}

/* Texture-stack attached to some shading datablock */
void DepsgraphRelationBuilder::build_texture_stack(ID *owner, MTex **texture_stack)
{
	int i;
	
	/* for now assume that all texture-stacks have same number of max items */
	for (i = 0; i < MAX_MTEX; i++) {
		MTex *mtex = texture_stack[i];
		if (mtex && mtex->tex)
			build_texture(owner, mtex->tex);
	}
}

void DepsgraphRelationBuilder::build_compositor(Scene *scene)
{
	/* For now, just a plain wrapper? */
	build_nodetree(&scene->id, scene->nodetree);
}
