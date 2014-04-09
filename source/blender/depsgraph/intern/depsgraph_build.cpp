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

#include "depsgraph_util_string.h"

#include "stubs.h" // XXX: REMOVE THIS INCLUDE ONCE DEPSGRAPH REFACTOR PROJECT IS DONE!!!

/* ************************************************* */
/* External Build API */

struct DepsNodeHandle {
	Depsgraph *graph;
	DepsNode *node;
	const string &default_name;
};

static eDepsNode_Type deg_build_scene_component_type(eDepsSceneComponentType component)
{
	switch (component) {
		case DEG_SCENE_COMP_PARAMETERS:     return DEPSNODE_TYPE_PARAMETERS;
		case DEG_SCENE_COMP_ANIMATION:      return DEPSNODE_TYPE_ANIMATION;
		case DEG_SCENE_COMP_SEQUENCER:      return DEPSNODE_TYPE_SEQUENCER;
	}
}

static eDepsNode_Type deg_build_object_component_type(eDepsObjectComponentType component)
{
	switch (component) {
		case DEG_OB_COMP_PARAMETERS:        return DEPSNODE_TYPE_PARAMETERS;
		case DEG_OB_COMP_PROXY:             return DEPSNODE_TYPE_PROXY;
		case DEG_OB_COMP_ANIMATION:         return DEPSNODE_TYPE_ANIMATION;
		case DEG_OB_COMP_TRANSFORM:         return DEPSNODE_TYPE_TRANSFORM;
		case DEG_OB_COMP_GEOMETRY:          return DEPSNODE_TYPE_GEOMETRY;
		case DEG_OB_COMP_EVAL_POSE:         return DEPSNODE_TYPE_EVAL_POSE;
		case DEG_OB_COMP_BONE:              return DEPSNODE_TYPE_BONE;
		case DEG_OB_COMP_EVAL_PARTICLES:    return DEPSNODE_TYPE_EVAL_PARTICLES;
	}
}

void DEG_add_scene_relation(DepsNodeHandle *handle, struct Scene *scene, eDepsSceneComponentType component, const char *description)
{
	Depsgraph *graph = handle->graph;
	DepsNode *node = handle->node;
	
	eDepsNode_Type type = deg_build_scene_component_type(component);
	DepsNode *comp_node = graph->find_node((ID *)scene, "", type, "");
	if (comp_node)
		graph->add_new_relation(comp_node, node, DEPSREL_TYPE_STANDARD, string(description));
}

void DEG_add_object_relation(DepsNodeHandle *handle, struct Object *ob, eDepsObjectComponentType component, const char *description)
{
	Depsgraph *graph = handle->graph;
	DepsNode *node = handle->node;
	
	eDepsNode_Type type = deg_build_object_component_type(component);
	DepsNode *comp_node = graph->find_node((ID *)ob, "", type, "");
	if (comp_node)
		graph->add_new_relation(comp_node, node, DEPSREL_TYPE_STANDARD, string(description));
}

/* ************************************************* */
/* Node Builder */

static bool is_id_tagged(ConstIDPtr id)
{
	return id->flag & LIB_DOIT;
}

static void id_tag_set(IDPtr id)
{
	id->flag |= LIB_DOIT;
}

static void id_tag_clear(IDPtr id)
{
	id->flag &= ~LIB_DOIT;
}

DepsgraphNodeBuilder::DepsgraphNodeBuilder(Main *bmain, Depsgraph *graph) :
    m_bmain(bmain),
    m_graph(graph)
{
}

DepsgraphNodeBuilder::~DepsgraphNodeBuilder()
{
}

IDDepsNode *DepsgraphNodeBuilder::add_id_node(IDPtr id)
{
	return m_graph->get_id_node(id);
}

TimeSourceDepsNode *DepsgraphNodeBuilder::add_time_source(IDPtr id)
{
	return (TimeSourceDepsNode *)m_graph->get_node(id, "", DEPSNODE_TYPE_TIMESOURCE, string_format("%s Time Source", id->name+2));
}

ComponentDepsNode *DepsgraphNodeBuilder::add_component_node(IDDepsNode *id_node, eDepsNode_Type comp_type, const string &subdata)
{
	ComponentDepsNode *comp_node = id_node->get_component(comp_type);
	comp_node->owner = id_node;
	return comp_node;
}

OperationDepsNode *DepsgraphNodeBuilder::add_operation_node(ComponentDepsNode *comp_node, eDepsNode_Type type,
                                                            eDepsOperation_Type optype, DepsEvalOperationCb op, const string &description)
{
	OperationDepsNode *op_node = comp_node->add_operation(type, optype, op, description);
	BLI_assert(comp_node->owner);
	BLI_assert(comp_node->owner->id);
	RNA_id_pointer_create(comp_node->owner->id, &op_node->ptr);
	return op_node;
}

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
		build_object(ob);
		
		/* object that this is a proxy for */
		// XXX: the way that proxies work needs to be completely reviewed!
		if (ob->proxy) {
			build_object(ob->proxy);
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
		if (is_id_tagged(group)) {
			// TODO: we need to make this group reliant on the object that spawned it...
			build_subgraph(group);
			
			id_tag_clear(group);
		}
	}
	
	/* rigidbody */
	if (scene->rigidbody_world) {
		build_rigidbody(scene);
	}
	
	/* scene's animation and drivers */
	if (scene->adt) {
		build_animdata(scene);
	}
	
	/* world */
	if (scene->world) {
		build_world(scene, scene->world);
	}
	
	/* compo nodes */
	if (scene->nodetree) {
		build_compositor(scene);
	}
	
	/* sequencer */
	// XXX...
	
	return scene_node;
}

SubgraphDepsNode *DepsgraphNodeBuilder::build_subgraph(Group *group)
{
	
}

IDDepsNode *DepsgraphNodeBuilder::build_object(Object *ob)
{
	/* create node for object itself */
	IDDepsNode *ob_node = add_id_node(ob);
	
	/* standard components */
	ComponentDepsNode *params_node = add_component_node(ob_node, DEPSNODE_TYPE_OP_PARAMETER);
	ComponentDepsNode *trans_node = build_object_transform(ob, ob_node);
	
	/* object parent */
	if (ob->parent) {
		add_operation_node(trans_node, DEPSNODE_TYPE_OP_TRANSFORM, 
		                   DEPSOP_TYPE_EXEC, BKE_object_eval_parent,
		                   "BKE_object_eval_parent");
	}
	
	/* object constraints */
	if (ob->constraints.first) {
		build_constraints(trans_node, DEPSNODE_TYPE_OP_TRANSFORM);
	}
	
#if 0
	/* object data */
	if (ob->data) {
		ID *obdata_id = (ID *)ob->data;
		AnimData *data_adt;
		
		/* ob data animation */
		data_adt = BKE_animdata_from_id(obdata_id);
		if (data_adt) {
			deg_build_animdata_graph(graph, scene, obdata_id);
		}
		
		/* type-specific data... */
		switch (ob->type) {
			case OB_MESH:     /* Geometry */
			case OB_CURVE:
			case OB_FONT:
			case OB_SURF:
			case OB_MBALL:
			case OB_LATTICE:
			{
				deg_build_obdata_geom_graph(graph, scene, ob);
			}
			break;
			
			
			case OB_ARMATURE: /* Pose */
				deg_build_rig_graph(graph, scene, ob);
				break;
			
			
			case OB_LAMP:   /* Lamp */
				deg_build_lamp_graph(graph, scene, ob);
				break;
				
			case OB_CAMERA: /* Camera */
				deg_build_camera_graph(graph, scene, ob);
				break;
		}
	}
	
	/* particle systems */
	if (ob->particlesystem.first) {
		deg_build_particles_graph(graph, scene, ob);
	}
	
	/* AnimData */
	if (ob->adt) {
		deg_build_animdata_graph(graph, scene, &ob->id);
	}
#endif
	
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
	                   "BKE_object_eval_local_transform");
	
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
	                   DEPSNODE_OP_NAME_CONSTRAINT_STACK);
}

void DepsgraphNodeBuilder::build_rigidbody(Scene *scene)
{
	
}

void DepsgraphNodeBuilder::build_animdata(IDPtr id)
{
	
}

void DepsgraphNodeBuilder::build_world(Scene *scene, World *world)
{
	
}

void DepsgraphNodeBuilder::build_compositor(Scene *scene)
{
	
}

/* ************************************************* */
/* Relations Builder */

DepsgraphRelationBuilder::DepsgraphRelationBuilder(Depsgraph *graph) :
    m_graph(graph)
{
}

IDDepsNode *DepsgraphRelationBuilder::find_node(const IDKey &key) const
{
	IDDepsNode *node = m_graph->find_id_node(key.id);
	return node;
}

ComponentDepsNode *DepsgraphRelationBuilder::find_node(const ComponentKey &key) const
{
	IDDepsNode *id_node = m_graph->find_id_node(key.id);
	if (!id_node)
		return NULL;
	
	ComponentDepsNode *node = id_node->find_component(key.type);
	return node;
}

OperationDepsNode *DepsgraphRelationBuilder::find_node(const OperationKey &key) const
{
	IDDepsNode *id_node = m_graph->find_id_node(key.id);
	if (!id_node)
		return NULL;
	
	DepsNodeFactory *factory = DEG_get_node_factory(key.type);
	ComponentDepsNode *comp_node = id_node->find_component(factory->component_type());
	if (!comp_node)
		return NULL;
	
	OperationDepsNode *op_node = comp_node->find_operation(key.name);
	return op_node;
}

void DepsgraphRelationBuilder::add_node_relation(DepsNode *node_from, DepsNode *node_to,
                                            eDepsRelation_Type type, const string &description)
{
	m_graph->add_new_relation(node_from, node_to, type, description);
}

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
	if (ob->parent)
		build_object_parent(ob);
	
	/* object constraints */
	if (ob->constraints.first) {
		build_constraints(scene, ob, DEPSNODE_TYPE_OP_TRANSFORM, &ob->constraints);
	}
	
#if 0
	/* object data */
	if (ob->data) {
		ID *obdata_id = (ID *)ob->data;
		AnimData *data_adt;
		
		/* ob data animation */
		data_adt = BKE_animdata_from_id(obdata_id);
		if (data_adt) {
			deg_build_animdata_graph(graph, scene, obdata_id);
		}
		
		/* type-specific data... */
		switch (ob->type) {
			case OB_MESH:     /* Geometry */
			case OB_CURVE:
			case OB_FONT:
			case OB_SURF:
			case OB_MBALL:
			case OB_LATTICE:
			{
				deg_build_obdata_geom_graph(graph, scene, ob);
			}
			break;
			
			
			case OB_ARMATURE: /* Pose */
				deg_build_rig_graph(graph, scene, ob);
				break;
			
			
			case OB_LAMP:   /* Lamp */
				deg_build_lamp_graph(graph, scene, ob);
				break;
				
			case OB_CAMERA: /* Camera */
				deg_build_camera_graph(graph, scene, ob);
				break;
		}
	}
	
	/* particle systems */
	if (ob->particlesystem.first) {
		deg_build_particles_graph(graph, scene, ob);
	}
	
	/* AnimData */
	if (ob->adt) {
		deg_build_animdata_graph(graph, scene, &ob->id);
	}
	
	/* return object node... */
	return ob_node;
#endif
}

void DepsgraphRelationBuilder::build_object_parent(Object *ob)
{
	IDKey ob_key(ob);
	
	/* type-specific links */
	switch (ob->partype) {
		case PARSKEL:  /* Armature Deform (Virtual Modifier) */
		{
			ComponentKey parent_key(ob->parent, DEPSNODE_TYPE_TRANSFORM);
			add_relation(parent_key, ob_key, DEPSREL_TYPE_STANDARD, "Armature Deform Parent");
		}
		break;
			
		case PARVERT1: /* Vertex Parent */
		case PARVERT3:
		{
			ComponentKey parent_key(ob->parent, DEPSNODE_TYPE_GEOMETRY);
			add_relation(parent_key, ob_key, DEPSREL_TYPE_GEOMETRY_EVAL, "Vertex Parent");
			/* XXX not sure what this is for or how you could be done properly - lukas */
			//parent_node->customdata_mask |= CD_MASK_ORIGINDEX;
		}
		break;
			
		case PARBONE: /* Bone Parent */
		{
			ComponentKey parent_key(ob->parent, DEPSNODE_TYPE_BONE, ob->parsubstr);
			add_relation(parent_key, ob_key, DEPSREL_TYPE_TRANSFORM, "Bone Parent");
		}
		break;
			
		default:
		{
			if (ob->parent->type == OB_LATTICE) {
				/* Lattice Deform Parent - Virtual Modifier */
				ComponentKey parent_key(ob->parent, DEPSNODE_TYPE_TRANSFORM);
				add_relation(parent_key, ob_key, DEPSREL_TYPE_STANDARD, "Lattice Deform Parent");
			}
			else if (ob->parent->type == OB_CURVE) {
				Curve *cu = (Curve *)ob->parent->data;
				
				if (cu->flag & CU_PATH) {
					/* Follow Path */
					ComponentKey parent_key(ob->parent, DEPSNODE_TYPE_GEOMETRY);
					add_relation(parent_key, ob_key, DEPSREL_TYPE_TRANSFORM, "Curve Follow Parent");
					// XXX: link to geometry or object? both are needed?
					// XXX: link to timesource too?
				}
				else {
					/* Standard Parent */
					ComponentKey parent_key(ob->parent, DEPSNODE_TYPE_TRANSFORM);
					add_relation(parent_key, ob_key, DEPSREL_TYPE_TRANSFORM, "Curve Parent");
				}
			}
			else {
				/* Standard Parent */
				ComponentKey parent_key(ob->parent, DEPSNODE_TYPE_TRANSFORM);
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

void DepsgraphRelationBuilder::build_constraints(Scene *scene, IDPtr id, eDepsNode_Type constraint_op_type,
                                                 ListBase *constraints)
{
	OperationKey constraint_op_key(id, constraint_op_type, DEPSNODE_OP_NAME_CONSTRAINT_STACK);
	
	/* add dependencies for each constraint in turn */
	for (bConstraint *con = (bConstraint *)constraints->first; con; con = con->next) {
		bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(con);
		/* invalid constraint type... */
		if (cti == NULL)
			continue;
		
		/* special case for camera tracking -- it doesn't use targets to define relations */
		// TODO: we can now represent dependencies in a much richer manner, so review how this is done...
		if (ELEM3(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_CAMERASOLVER, CONSTRAINT_TYPE_OBJECTSOLVER)) {
			bool depends_on_camera = false;
			
			if (cti->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
				bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;

				if (((data->clip) || (data->flag & FOLLOWTRACK_ACTIVECLIP)) && data->track[0])
					depends_on_camera = true;
				
				if (data->depth_ob) {
					// DAG_RL_DATA_OB | DAG_RL_OB_OB
					ComponentKey depth_key(data->depth_ob, DEPSNODE_TYPE_TRANSFORM);
					add_relation(depth_key, constraint_op_key, DEPSREL_TYPE_TRANSFORM, cti->name);
				}
			}
			else if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
				depends_on_camera = true;
			}

			if (depends_on_camera && scene->camera) {
				// DAG_RL_DATA_OB | DAG_RL_OB_OB
				ComponentKey camera_key(scene->camera, DEPSNODE_TYPE_TRANSFORM);
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
					ComponentKey target_key(ct->tar, DEPSNODE_TYPE_GEOMETRY);
					add_relation(target_key, constraint_op_key, DEPSREL_TYPE_GEOMETRY_EVAL, cti->name); // XXX: type = geom_transform
				}
				else if ((ct->tar->type == OB_ARMATURE) && (ct->subtarget[0])) {
					/* bone */
					ComponentKey target_key(ct->tar, DEPSNODE_TYPE_BONE);
					add_relation(target_key, constraint_op_key, DEPSREL_TYPE_TRANSFORM, cti->name);
				}
				else if (ELEM(ct->tar->type, OB_MESH, OB_LATTICE) && (ct->subtarget[0])) {
					/* vertex group */
					/* NOTE: for now, we don't need to represent vertex groups separately... */
					ComponentKey target_key(ct->tar, DEPSNODE_TYPE_GEOMETRY);
					add_relation(target_key, constraint_op_key, DEPSREL_TYPE_GEOMETRY_EVAL, cti->name);
					
					if (ct->tar->type == OB_MESH) {
						//node2->customdata_mask |= CD_MASK_MDEFORMVERT;
					}
				}
				else {
					/* standard object relation */
					// TODO: loc vs rot vs scale?
					ComponentKey target_key(ct->tar, DEPSNODE_TYPE_TRANSFORM);
					add_relation(target_key, constraint_op_key, DEPSREL_TYPE_TRANSFORM, cti->name);
				}
			}
			
			if (cti->flush_constraint_targets)
				cti->flush_constraint_targets(con, &targets, 1);
		}
	}
}

void DepsgraphRelationBuilder::build_rigidbody(Scene *scene)
{
	
}

void DepsgraphRelationBuilder::build_animdata(IDPtr id)
{
	
}

void DepsgraphRelationBuilder::build_world(Scene *scene, World *world)
{
	
}

void DepsgraphRelationBuilder::build_compositor(Scene *scene)
{
	
}

/* -------------------------------------------------- */

/* Build depsgraph for the given scene, and dump results in given graph container */
// XXX: assume that this is called from outside, given the current scene as the "main" scene 
void DEG_graph_build_from_scene(Depsgraph *graph, Main *bmain, Scene *scene)
{
	/* clear "LIB_DOIT" flag from all materials, etc. 
	 * to prevent infinite recursion problems later [#32017] 
	 */
	BKE_main_id_tag_idcode(bmain, ID_MA, false);
	BKE_main_id_tag_idcode(bmain, ID_LA, false);
	BKE_main_id_tag_idcode(bmain, ID_WO, false);
	BKE_main_id_tag_idcode(bmain, ID_TE, false);
	
	DepsgraphNodeBuilder node_builder(bmain, graph);
	node_builder.build_scene(scene);

	DepsgraphRelationBuilder relation_builder(graph);
	relation_builder.build_scene(scene);

#if 0
	/* create root node for scene first
	 * - this way it should be the first in the graph,
	 *   reflecting its role as the entrypoint
	 */
	graph->root_node = (RootDepsNode *)graph->get_node(NULL, "", DEPSNODE_TYPE_ROOT, "Root (Scene)");
	
	/* build graph for scene and all attached data */
	DepsNode *scene_node = deg_build_scene_graph(graph, bmain, scene);
	
	/* hook this up to a "root" node as entrypoint to graph... */
	graph->add_new_relation(graph->root_node, scene_node, 
	                     DEPSREL_TYPE_ROOT_TO_ACTIVE, "Root to Active Scene");
	
	/* ensure that all implicit constraints between nodes are satisfied */
	DEG_graph_validate_links(graph);
	
	/* sort nodes to determine evaluation order (in most cases) */
	DEG_graph_sort(graph);
#endif
}

/* ************************************************* */

