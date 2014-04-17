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

#include "depsgraph_util_rna.h"
#include "depsgraph_util_string.h"

#include "stubs.h" // XXX: REMOVE THIS INCLUDE ONCE DEPSGRAPH REFACTOR PROJECT IS DONE!!!

/* ************************************************* */
/* Node Builder */

IDDepsNode *DepsgraphNodeBuilder::build_scene(Scene *scene)
{
	IDDepsNode *scene_node = add_id_node(scene);
	/* timesource */
	add_time_source(scene);
	
	/* build subgraph for set, and link this in... */
	// XXX: depending on how this goes, that scene itself could probably store its
	//      own little partial depsgraph?
	if (scene->set) {
		build_scene(scene->set);
	}
	
	/* scene objects */
	for (Base *base = (Base *)scene->base.first; base; base = base->next) {
		Object *ob = base->object;
		
		/* object itself */
		build_object(scene, ob);
		
		/* object that this is a proxy for */
		// XXX: the way that proxies work needs to be completely reviewed!
		if (ob->proxy) {
			build_object(scene, ob->proxy);
		}
		
		/* handled in next loop... 
		 * NOTE: in most cases, setting dupli-group means that we may want
		 *       to instance existing data and/or reuse it with very few
		 *       modifications...
		 */
		if (ob->dup_group) {
			id_tag_set(ob->dup_group);
		}
	}
	
	/* tagged groups */
	for (Group *group = (Group *)m_bmain->group.first; group; group = (Group *)group->id.next) {
		if (id_is_tagged(group)) {
			// TODO: we need to make this group reliant on the object that spawned it...
			build_subgraph(group);
			
			id_tag_clear(group);
		}
	}
	
	/* rigidbody */
	if (scene->rigidbody_world) {
		build_rigidbody(scene_node, scene);
	}
	
	/* scene's animation and drivers */
	if (scene->adt) {
		build_animdata(scene_node);
	}
	
	/* world */
	if (scene->world) {
		build_world(scene->world);
	}
	
	/* compo nodes */
	if (scene->nodetree) {
		build_compositor(scene_node, scene);
	}
	
	/* sequencer */
	// XXX...
	
	return scene_node;
}

/* Build depsgraph for the given group
 * This is usually used for building subgraphs for groups to use
 */
void DepsgraphNodeBuilder::build_group(Group *group)
{
	/* add group objects */
	for (GroupObject *go = (GroupObject *)group->gobject.first; go; go = go->next) {
		/*Object *ob = go->ob;*/
		
		/* Each "group object" is effectively a separate instance of the underlying
		 * object data. When the group is evaluated, the transform results and/or 
		 * some other attributes end up getting overridden by the group
		 */
	}
}

SubgraphDepsNode *DepsgraphNodeBuilder::build_subgraph(Group *group)
{
	/* sanity checks */
	if (!group)
		return NULL;
	
	/* create new subgraph's data */
	Depsgraph *subgraph = DEG_graph_new();
	
	DepsgraphNodeBuilder subgraph_builder(m_bmain, subgraph);
	subgraph_builder.build_group(group);
	
	/* create a node for representing subgraph */
	SubgraphDepsNode *subgraph_node = m_graph->add_subgraph_node(&group->id);
	subgraph_node->graph = subgraph;
	
	/* make a copy of the data this node will need? */
	// XXX: do we do this now, or later?
	// TODO: need API function which queries graph's ID's hash, and duplicates those blocks thoroughly with all outside links removed...
	
	return subgraph_node;
}

IDDepsNode *DepsgraphNodeBuilder::build_object(Scene *scene, Object *ob)
{
	/* create node for object itself */
	IDDepsNode *ob_node = add_id_node(ob);
	
	/* standard components */
	/*ComponentDepsNode *params_node =*/ add_component_node(ob_node, DEPSNODE_TYPE_OP_PARAMETER);
	ComponentDepsNode *trans_node = build_object_transform(ob, ob_node);
	
	/* AnimData */
	build_animdata(ob_node);
	
	/* object parent */
	if (ob->parent) {
		add_operation_node(trans_node, DEPSNODE_TYPE_OP_TRANSFORM, 
		                   DEPSOP_TYPE_EXEC, BKE_object_eval_parent,
		                   deg_op_name_object_parent, make_rna_id_pointer(ob));
	}
	
	/* object constraints */
	if (ob->constraints.first) {
		build_constraints(trans_node, DEPSNODE_TYPE_OP_TRANSFORM);
	}
	
	/* object data */
	if (ob->data) {
		ID *obdata_id = (ID *)ob->data;
		IDDepsNode *obdata_node = add_id_node(obdata_id);
		/* ob data animation */
		build_animdata(obdata_node);
		
		/* type-specific data... */
		switch (ob->type) {
			case OB_MESH:     /* Geometry */
			case OB_CURVE:
			case OB_FONT:
			case OB_SURF:
			case OB_MBALL:
			case OB_LATTICE:
			{
				build_obdata_geom(ob_node, obdata_node, scene, ob);
			}
			break;
			
			case OB_ARMATURE: /* Pose */
				build_rig(ob_node, ob);
				break;
			
			case OB_LAMP:   /* Lamp */
				build_lamp(ob_node, obdata_node, ob);
				break;
				
			case OB_CAMERA: /* Camera */
				build_camera(ob_node, obdata_node, ob);
				break;
		}
	}
	
	/* particle systems */
	if (ob->particlesystem.first) {
		build_particles(ob_node, ob);
	}
	
	/* return object node... */
	return ob_node;
}

ComponentDepsNode *DepsgraphNodeBuilder::build_object_transform(Object *ob, IDDepsNode *ob_node)
{
	/* component to hold all transform operations */
	ComponentDepsNode *trans_node = add_component_node(ob_node, DEPSNODE_TYPE_TRANSFORM);
	
	/* init operation */
	add_operation_node(trans_node, DEPSNODE_TYPE_OP_TRANSFORM,
	                   DEPSOP_TYPE_INIT, BKE_object_eval_local_transform,
	                   deg_op_name_object_local_transform, make_rna_id_pointer(ob));
	
	/* return component created */
	return trans_node;
}

void DepsgraphNodeBuilder::build_constraints(ComponentDepsNode *comp_node, eDepsNode_Type constraint_op_type)
{
	/* == Constraints Graph Notes ==
	 * For constraints, we currently only add a operation node to the Transform
	 * or Bone components (depending on whichever type of owner we have).
	 * This represents the entire constraints stack, which is for now just
	 * executed as a single monolithic block. At least initially, this should
	 * be sufficient for ensuring that the porting/refactoring process remains
	 * manageable. 
	 * 
	 * However, when the time comes for developing "node-based" constraints,
	 * we'll need to split this up into pre/post nodes for "constraint stack
	 * evaluation" + operation nodes for each constraint (i.e. the contents
	 * of the loop body used in the current "solve_constraints()" operation).
	 *
	 * -- Aligorith, August 2013 
	 */
	
	/* create node for constraint stack */
	add_operation_node(comp_node, constraint_op_type, 
	                   DEPSOP_TYPE_EXEC, BKE_constraints_evaluate,
	                   deg_op_name_constraint_stack, make_rna_id_pointer(comp_node->owner->id));
}

/* Build graph nodes for AnimData block 
 * < scene_node: Scene that ID-block this lives on belongs to
 * < id: ID-Block which hosts the AnimData
 */
void DepsgraphNodeBuilder::build_animdata(IDDepsNode *id_node)
{
	AnimData *adt = BKE_animdata_from_id(id_node->id);
	if (!adt)
		return;
	
	/* animation */
	if (adt->action || adt->nla_tracks.first || adt->drivers.first) {
		/* create "animation" data node for this block */
		ComponentDepsNode *adt_node = add_component_node(id_node, DEPSNODE_TYPE_ANIMATION);
		
		// XXX: Hook up specific update callbacks for special properties which may need it...
		
		/* drivers */
		for (FCurve *fcu = (FCurve *)adt->drivers.first; fcu; fcu = fcu->next) {
			/* create driver */
			/*OperationDepsNode *driver_node =*/ build_driver(id_node, fcu);
			
			/* hook up update callback associated with F-Curve */
			// ...
		}
	}
}

/* Build graph node(s) for Driver
 * < id: ID-Block that driver is attached to
 * < fcu: Driver-FCurve
 */
OperationDepsNode *DepsgraphNodeBuilder::build_driver(IDDepsNode *id_node, FCurve *fcurve)
{
	IDPtr id = id_node->id;
	ChannelDriver *driver = fcurve->driver;
	
	/* create data node for this driver ..................................... */
	OperationDepsNode *driver_op = add_operation_node(id_node, DEPSNODE_TYPE_OP_DRIVER,
	                                                  DEPSOP_TYPE_EXEC, BKE_animsys_eval_driver,
	                                                  deg_op_name_driver(driver),
	                                                  make_rna_pointer(id, &RNA_FCurve, fcurve));
	
	/* tag "scripted expression" drivers as needing Python (due to GIL issues, etc.) */
	if (driver->type == DRIVER_TYPE_PYTHON) {
		driver_op->flag |= DEPSOP_FLAG_USES_PYTHON;
	}
	
	/* return driver node created */
	return driver_op;
}

/* Recursively build graph for world */
void DepsgraphNodeBuilder::build_world(World *world)
{
	/* Prevent infinite recursion by checking (and tagging the world) as having been visited 
	 * already. This assumes wo->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (id_is_tagged(world))
		return;
	id_tag_set(world);
	
	/* world itself */
	IDDepsNode *world_node = add_id_node(world); /* world shading/params? */
	
	build_animdata(world_node);
	
	/* TODO: other settings? */
	
	/* textures */
	build_texture_stack(world_node, world->mtex);
	
	/* world's nodetree */
	if (world->nodetree) {
		build_nodetree(world_node, world->nodetree);
	}

	id_tag_clear(world);
}

/* Rigidbody Simulation - Scene Level */
void DepsgraphNodeBuilder::build_rigidbody(IDDepsNode *scene_node, Scene *scene)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	OperationDepsNode *init_node;
	OperationDepsNode *sim_node; // XXX: what happens if we need to split into several groups?
	
	/* == Rigidbody Simulation Nodes == 
	 * There are 3 nodes related to Rigidbody Simulation:
	 * 1) "Initialise/Rebuild World" - this is called sparingly, only when the simulation
	 *    needs to be rebuilt (mainly after file reload, or moving back to start frame)
	 * 2) "Do Simulation" - perform a simulation step - interleaved between the evaluation
	 *    steps for clusters of objects (i.e. between those affected and/or not affected by
	 *    the sim for instance)
	 *
	 * 3) "Pull Results" - grab the specific transforms applied for a specific object -
	 *    performed as part of object's transform-stack building
	 */
	
	/* create nodes ------------------------------------------------------------------------ */
	/* XXX this needs to be reviewed! */
	ComponentDepsNode *scene_trans = scene_node->find_component(DEPSNODE_TYPE_TRANSFORM);
	
	/* init/rebuild operation */
	init_node = add_operation_node(scene_trans, DEPSNODE_TYPE_OP_RIGIDBODY,
	                               DEPSOP_TYPE_REBUILD, BKE_rigidbody_rebuild_sim,
	                               deg_op_name_rigidbody_world_rebuild, PointerRNA_NULL);
	
	/* do-sim operation */
	sim_node = add_operation_node(scene_trans, DEPSNODE_TYPE_OP_RIGIDBODY,
	                              DEPSOP_TYPE_SIM, BKE_rigidbody_eval_simulation,
	                              deg_op_name_rigidbody_world_simulate, PointerRNA_NULL);
	
	/* objects - simulation participants */
	if (rbw->group) {
		for (GroupObject *go = (GroupObject *)rbw->group->gobject.first; go; go = go->next) {
			Object *ob = go->ob;
			
			if (!ob || ob->type != OB_MESH)
				continue;
			
			/* object's transform component - where the rigidbody operation lives
			 * NOTE: since we're doing this step after all objects have been built,
			 *       we can safely assume that all necessary ops we have to play with
			 *       already exist
			 */
			IDDepsNode *ob_node = m_graph->find_id_node(&ob->id);
			ComponentDepsNode *tcomp = ob_node->find_component(DEPSNODE_TYPE_TRANSFORM);
			
			/* 2) create operation for flushing results */
			add_operation_node(tcomp, DEPSNODE_TYPE_OP_TRANSFORM,
			                   DEPSOP_TYPE_EXEC, BKE_rigidbody_object_sync_transforms, /* xxx: function name */
			                   deg_op_name_rigidbody_object_sync, PointerRNA_NULL);
		}
	}
}

void DepsgraphNodeBuilder::build_particles(IDDepsNode *ob_node, Object *ob)
{
	/* == Particle Systems Nodes ==
	 * There are two types of nodes associated with representing
	 * particle systems:
	 *  1) Component (EVAL_PARTICLES) - This is the particle-system
	 *     evaluation context for an object. It acts as the container
	 *     for all the nodes associated with a particular set of particle
	 *     systems.
	 *  2) Particle System Eval Operation - This operation node acts as a
	 *     blackbox evaluation step for one particle system referenced by
	 *     the particle systems stack. All dependencies link to this operation.
	 */
	
	/* component for all particle systems */
	ComponentDepsNode *psys_comp = add_component_node(ob_node, DEPSNODE_TYPE_EVAL_PARTICLES);
	
	/* particle systems */
	for (ParticleSystem *psys = (ParticleSystem *)ob->particlesystem.first; psys; psys = psys->next) {
		ParticleSettings *part = psys->part;
		
		/* particle settings */
		// XXX: what if this is used more than once!
		IDDepsNode *part_node = add_id_node(part);
		build_animdata(part_node);
		
		/* this particle system */
		add_operation_node(psys_comp, DEPSNODE_TYPE_OP_PARTICLE,
		                   DEPSOP_TYPE_EXEC, BKE_particle_system_eval, 
		                   deg_op_name_psys_eval, PointerRNA_NULL);
	}
	
	/* pointcache */
	// TODO...
}

/* IK Solver Eval Steps */
void DepsgraphNodeBuilder::build_ik_pose(ComponentDepsNode *bone_node, Object *ob, bPoseChannel *pchan, bConstraint *con)
{
	bKinematicConstraint *data = (bKinematicConstraint *)con->data;
	
	/* find the chain's root */
	bPoseChannel *rootchan = pchan;
	/* exclude tip from chain? */
	if (!(data->flag & CONSTRAINT_IK_TIP)) {
		rootchan = rootchan->parent;
	}
	
	if (rootchan) {
		size_t segcount = 0;
		while (rootchan->parent) {
			/* continue up chain, until we reach target number of items... */
			segcount++;
			if ((segcount == data->rootbone) || (segcount > 255)) break;  /* XXX 255 is weak */
			
			rootchan = rootchan->parent;
		}
	}
	
	/* operation node for evaluating/running IK Solver */
	add_operation_node(bone_node, DEPSNODE_TYPE_OP_POSE,
	                   DEPSOP_TYPE_SIM, BKE_pose_iktree_evaluate, 
	                   deg_op_name_ik_solver, make_rna_pointer(ob, &RNA_PoseBone, rootchan));
}

/* Spline IK Eval Steps */
void DepsgraphNodeBuilder::build_splineik_pose(ComponentDepsNode *bone_node, Object *ob, bPoseChannel *pchan, bConstraint *con)
{
	bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
	
	/* find the chain's root */
	bPoseChannel *rootchan = pchan;
	size_t segcount = 0;
	while (rootchan->parent) {
		/* continue up chain, until we reach target number of items... */
		segcount++;
		if ((segcount == data->chainlen) || (segcount > 255)) break;  /* XXX 255 is weak */
		
		rootchan = rootchan->parent;
	}
	
	/* operation node for evaluating/running IK Solver
	 * store the "root bone" of this chain in the solver, so it knows where to start
	 */
	add_operation_node(bone_node, DEPSNODE_TYPE_OP_POSE,
	                   DEPSOP_TYPE_SIM, BKE_pose_splineik_evaluate, deg_op_name_spline_ik_solver,
	                   make_rna_pointer(ob, &RNA_PoseBone, rootchan));
	// XXX: what sort of ID-data is needed?
}

/* Pose/Armature Bones Graph */
void DepsgraphNodeBuilder::build_rig(IDDepsNode *ob_node, Object *ob)
{
	bArmature *arm = (bArmature *)ob->data;
	
	/* Armature-Data */
	IDDepsNode *arm_node = add_id_node(arm);
	
	// TODO: bone names?
	/* animation and/or drivers linking posebones to base-armature used to define them 
	 * NOTE: AnimData here is really used to control animated deform properties, 
	 *       which ideally should be able to be unique across different instances.
	 *       Eventually, we need some type of proxy/isolation mechanism inbetween here
	 *       to ensure that we can use same rig multiple times in same scene...
	 */
	build_animdata(arm_node);
	
	/* == Pose Rig Graph ==
	 * Pose Component:
	 * - Mainly used for referencing Bone components.
	 * - This is where the evaluation operations for init/exec/cleanup
	 *   (ik) solvers live, and are later hooked up (so that they can be
	 *   interleaved during runtime) with bone-operations they depend on/affect.
	 * - init_pose_eval() and cleanup_pose_eval() are absolute first and last
	 *   steps of pose eval process. ALL bone operations must be performed 
	 *   between these two...
	 * 
	 * Bone Component:
	 * - Used for representing each bone within the rig
	 * - Acts to encapsulate the evaluation operations (base matrix + parenting, 
	 *   and constraint stack) so that they can be easily found.
	 * - Everything else which depends on bone-results hook up to the component only
	 *   so that we can redirect those to point at either the the post-IK/
	 *   post-constraint/post-matrix steps, as needed.
	 */
	// TODO: rest pose/editmode handling!
	
	/* pose eval context 
	 * NOTE: init/cleanup steps for this are handled as part of the node's code
	 */
	/*PoseComponentDepsNode *pose_node = (PoseComponentDepsNode *)*/add_component_node(ob_node, DEPSNODE_TYPE_EVAL_POSE);
	
	/* bones */
	for (bPoseChannel *pchan = (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		/* component for hosting bone operations */
		BoneComponentDepsNode *bone_node = (BoneComponentDepsNode *)add_component_node(ob_node, DEPSNODE_TYPE_BONE, pchan->name);
		bone_node->pchan = pchan;
		
		/* node for bone eval */
		add_operation_node(bone_node, DEPSNODE_TYPE_OP_BONE, 
		                   DEPSOP_TYPE_EXEC, BKE_pose_eval_bone,
		                   "Bone Transforms", make_rna_pointer(ob, &RNA_PoseBone, pchan));
		
		/* constraints */
		build_constraints(bone_node, DEPSNODE_TYPE_OP_BONE);
		
		/* IK Solvers...
		 * - These require separate processing steps are pose-level
		 *   to be executed between chains of bones (i.e. once the
		 *   base transforms of a bunch of bones is done)
		 *
		 * Unsolved Issues:
		 * - Care is needed to ensure that multi-headed trees work out the same as in ik-tree building
		 * - Animated chain-lengths are a problem...
		 */
		for (bConstraint *con = (bConstraint *)pchan->constraints.first; con; con = con->next) {
			switch (con->type) {
				case CONSTRAINT_TYPE_KINEMATIC:
					build_ik_pose(bone_node, ob, pchan, con);
					break;
					
				case CONSTRAINT_TYPE_SPLINEIK:
					build_splineik_pose(bone_node, ob, pchan, con);
					break;
					
				default:
					break;
			}
		}
	}
}

/* Shapekeys */
void DepsgraphNodeBuilder::build_shapekeys(Key *key)
{
	/* create node for shapekeys block */
	IDDepsNode *key_node = add_id_node(key);
	build_animdata(key_node);
	
	// XXX: assume geometry - that's where shapekeys get evaluated anyways...
	add_component_node(key_node, DEPSNODE_TYPE_GEOMETRY);
}

/* ObData Geometry Evaluation */
// XXX: what happens if the datablock is shared!
void DepsgraphNodeBuilder::build_obdata_geom(IDDepsNode *ob_node, IDDepsNode *obdata_node, Scene *scene, Object *ob)
{
	ID *obdata = (ID *)ob->data;
	
	/* get nodes for result of obdata's evaluation, and geometry evaluation on object */
	ComponentDepsNode *geom_node = add_component_node(ob_node, DEPSNODE_TYPE_GEOMETRY);
	ComponentDepsNode *obdata_geom_node = add_component_node(obdata_node, DEPSNODE_TYPE_GEOMETRY);
	
	/* type-specific node/links */
	switch (ob->type) {
		case OB_MESH:
		{
			//Mesh *me = (Mesh *)ob->data;
			
			/* evaluation operations */
			add_operation_node(geom_node, DEPSNODE_TYPE_OP_GEOMETRY,
			                   DEPSOP_TYPE_EXEC, BKE_mesh_eval_geometry, 
			                   "Geometry Eval", make_rna_id_pointer(obdata));
		}
		break;
		
		case OB_MBALL: 
		{
			Object *mom = BKE_mball_basis_find(scene, ob);
			
			/* motherball - mom depends on children! */
			if (mom == ob) {
				/* metaball evaluation operations */
				/* NOTE: only the motherball gets evaluated! */
				add_operation_node(geom_node, DEPSNODE_TYPE_OP_GEOMETRY,
				                   DEPSOP_TYPE_EXEC, BKE_mball_eval_geometry, 
				                   "Geometry Eval", make_rna_id_pointer(obdata));
			}
		}
		break;
		
		case OB_CURVE:
		case OB_FONT:
		{
			/* curve evaluation operations */
			/* - calculate curve geometry (including path) */
			add_operation_node(geom_node, DEPSNODE_TYPE_OP_GEOMETRY,
			                   DEPSOP_TYPE_EXEC, BKE_curve_eval_geometry, 
			                   "Geometry Eval", make_rna_id_pointer(obdata));
			
			/* - calculate curve path - this is used by constraints, etc. */
			add_operation_node(obdata_geom_node, DEPSNODE_TYPE_OP_GEOMETRY,
			                   DEPSOP_TYPE_EXEC, BKE_curve_eval_path,
			                   "Path", PointerRNA_NULL);
		}
		break;
		
		case OB_SURF: /* Nurbs Surface */
		{
			/* nurbs evaluation operations */
			add_operation_node(geom_node, DEPSNODE_TYPE_OP_GEOMETRY,
			                   DEPSOP_TYPE_EXEC, BKE_curve_eval_geometry, 
			                   "Geometry Eval", make_rna_id_pointer(obdata));
		}
		break;
		
		case OB_LATTICE: /* Lattice */
		{
			/* lattice evaluation operations */
			add_operation_node(geom_node, DEPSNODE_TYPE_OP_GEOMETRY,
			                   DEPSOP_TYPE_EXEC, BKE_lattice_eval_geometry, 
			                   "Geometry Eval", make_rna_id_pointer(obdata));
		}
		break;
	}
	
	/* ShapeKeys */
	Key *key = BKE_key_from_object(ob);
	if (key)
		build_shapekeys(key);
	
	/* Modifiers */
	if (ob->modifiers.first) {
		ModifierData *md;
		
		for (md = (ModifierData *)ob->modifiers.first; md; md = md->next) {
//			ModifierTypeInfo *mti = modifierType_getInfo((ModifierType)md->type);
			
			add_operation_node(ob_node, DEPSNODE_TYPE_OP_GEOMETRY,
			                   DEPSOP_TYPE_EXEC, BKE_object_eval_modifier,
			                   string_format("Modifier %s", md->name), make_rna_pointer(ob, &RNA_Modifier, md));
		}
	}
	
	/* materials */
	if (ob->totcol) {
		int a;
		
		for (a = 1; a <= ob->totcol; a++) {
			Material *ma = give_current_material(ob, a);
			
			if (ma)
				build_material(geom_node, ma);
		}
	}
	
	/* geometry collision */
	if (ELEM3(ob->type, OB_MESH, OB_CURVE, OB_LATTICE)) {
		// add geometry collider relations
	}
}

/* Cameras */
// TODO: Link scene-camera links in somehow...
void DepsgraphNodeBuilder::build_camera(IDDepsNode *ob_node, IDDepsNode *obdata_node, Object *ob)
{
	/* node for obdata */
	add_component_node(obdata_node, DEPSNODE_TYPE_PARAMETERS);
}

/* Lamps */
void DepsgraphNodeBuilder::build_lamp(IDDepsNode *ob_node, IDDepsNode *obdata_node, Object *ob)
{
	Lamp *la = (Lamp *)ob->data;
	
	/* Prevent infinite recursion by checking (and tagging the lamp) as having been visited 
	 * already. This assumes la->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (id_is_tagged(la))
		return;
	id_tag_set(la);
	
	/* node for obdata */
	ComponentDepsNode *param_node = add_component_node(obdata_node, DEPSNODE_TYPE_PARAMETERS);
	
	/* lamp's nodetree */
	if (la->nodetree) {
		build_nodetree(param_node, la->nodetree);
	}
	
	/* textures */
	build_texture_stack(param_node, la->mtex);
	
	id_tag_clear(la);
}

void DepsgraphNodeBuilder::build_nodetree(DepsNode *owner_node, bNodeTree *ntree)
{
	if (!ntree)
		return;
	
	/* nodetree itself */
	IDDepsNode *ntree_node = add_id_node(ntree);
	
	build_animdata(ntree_node);
	
	/* nodetree's nodes... */
	for (bNode *bnode = (bNode *)ntree->nodes.first; bnode; bnode = bnode->next) {
		if (bnode->id) {
			if (GS(bnode->id->name) == ID_MA) {
				build_material(owner_node, (Material *)bnode->id);
			}
			else if (bnode->type == ID_TE) {
				build_texture(owner_node, (Tex *)bnode->id);
			}
			else if (bnode->type == NODE_GROUP) {
				build_nodetree(owner_node, (bNodeTree *)bnode->id);
			}
		}
	}
	
	// TODO: link from nodetree to owner_component?
}

/* Recursively build graph for material */
void DepsgraphNodeBuilder::build_material(DepsNode *owner_node, Material *ma)
{
	/* Prevent infinite recursion by checking (and tagging the material) as having been visited 
	 * already. This assumes ma->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (id_is_tagged(ma))
		return;
	id_tag_set(ma);
	
	/* material itself */
	IDDepsNode *ma_node = add_id_node(ma);
	
	build_animdata(ma_node);
	
	/* textures */
	build_texture_stack(owner_node, ma->mtex);
	
	/* material's nodetree */
	build_nodetree(owner_node, ma->nodetree);
	
	id_tag_clear(ma);
}

/* Texture-stack attached to some shading datablock */
void DepsgraphNodeBuilder::build_texture_stack(DepsNode *owner_node, MTex **texture_stack)
{
	int i;
	
	/* for now assume that all texture-stacks have same number of max items */
	for (i = 0; i < MAX_MTEX; i++) {
		MTex *mtex = texture_stack[i];
		if (mtex)
			build_texture(owner_node, mtex->tex);
	}
}

/* Recursively build graph for texture */
void DepsgraphNodeBuilder::build_texture(DepsNode *owner_node, Tex *tex)
{
	/* Prevent infinite recursion by checking (and tagging the texture) as having been visited 
	 * already. This assumes tex->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (id_is_tagged(tex))
		return;
	id_tag_set(tex);
	
	IDDepsNode *tex_node = add_id_node(tex);
	
	/* texture itself */
	build_animdata(tex_node);
	
	/* texture's nodetree */
	build_nodetree(owner_node, tex->nodetree);
	
	id_tag_clear(tex);
}

void DepsgraphNodeBuilder::build_compositor(IDDepsNode *scene_node, Scene *scene)
{
	/* For now, just a plain wrapper? */
	// TODO: create compositing component?
	// XXX: component type undefined!
	//graph->get_node(&scene->id, NULL, DEPSNODE_TYPE_COMPOSITING, NULL);
	
	/* for now, nodetrees are just parameters; compositing occurs in internals of renderer... */
	ComponentDepsNode *owner_node = add_component_node(scene_node, DEPSNODE_TYPE_PARAMETERS);
	build_nodetree(owner_node, scene->nodetree);
}
