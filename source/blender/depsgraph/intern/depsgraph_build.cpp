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

#include <stack>
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
#include "BKE_idcode.h"
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

/* ************************************************* */
/* External Build API */

static eDepsNode_Type deg_build_scene_component_type(eDepsSceneComponentType component)
{
	switch (component) {
		case DEG_SCENE_COMP_PARAMETERS:     return DEPSNODE_TYPE_PARAMETERS;
		case DEG_SCENE_COMP_ANIMATION:      return DEPSNODE_TYPE_ANIMATION;
		case DEG_SCENE_COMP_SEQUENCER:      return DEPSNODE_TYPE_SEQUENCER;
	}
	return DEPSNODE_TYPE_UNDEFINED;
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
	return DEPSNODE_TYPE_UNDEFINED;
}

void DEG_add_scene_relation(DepsNodeHandle *handle, struct Scene *scene, eDepsSceneComponentType component, const char *description)
{
	eDepsNode_Type type = deg_build_scene_component_type(component);
	ComponentKey comp_key(&scene->id, type);
	handle->builder->add_node_handle_relation(comp_key, handle, DEPSREL_TYPE_GEOMETRY_EVAL, string(description));
}

void DEG_add_object_relation(DepsNodeHandle *handle, struct Object *ob, eDepsObjectComponentType component, const char *description)
{
	eDepsNode_Type type = deg_build_object_component_type(component);
	ComponentKey comp_key(&ob->id, type);
	handle->builder->add_node_handle_relation(comp_key, handle, DEPSREL_TYPE_GEOMETRY_EVAL, string(description));
}

/* ************************************************* */
/* Node Builder */

DepsgraphNodeBuilder::DepsgraphNodeBuilder(Main *bmain, Depsgraph *graph) :
    m_bmain(bmain),
    m_graph(graph)
{
}

DepsgraphNodeBuilder::~DepsgraphNodeBuilder()
{
}

RootDepsNode *DepsgraphNodeBuilder::add_root_node()
{
	return m_graph->add_root_node();
}

IDDepsNode *DepsgraphNodeBuilder::add_id_node(ID *id)
{
	const char *idtype_name = BKE_idcode_to_name(GS(id->name));
	return m_graph->add_id_node(id, string(id->name+2) + "[" + idtype_name + "]");
}

TimeSourceDepsNode *DepsgraphNodeBuilder::add_time_source(ID *id)
{
	/* determine which node to attach timesource to */
	if (id) {
#if 0 /* XXX TODO */
		/* get ID node */
		IDDepsNode id_node = m_graph->find_id_node(id);
		
		/* depends on what this is... */
		switch (GS(id->name)) {
			case ID_SCE: /* Scene - Usually sequencer strip causing time remapping... */
			{
				// TODO...
			}
			break;
			
			case ID_GR: /* Group */
			{
				// TODO...
			}
			break;
			
			// XXX: time source...
			
			default:     /* Unhandled */
				printf("%s(): Unhandled ID - %s \n", __func__, id->name);
				break;
		}
#endif
	}
	else {
		/* root-node */
		RootDepsNode *root_node = m_graph->root_node;
		if (root_node) {
			return root_node->add_time_source("Time Source");
		}
	}
	
	return NULL;
}

ComponentDepsNode *DepsgraphNodeBuilder::add_component_node(ID *id, eDepsNode_Type comp_type, const string &comp_name)
{
	IDDepsNode *id_node = add_id_node(id);
	ComponentDepsNode *comp_node = id_node->add_component(comp_type, comp_name);
	comp_node->owner = id_node;
	return comp_node;
}

OperationDepsNode *DepsgraphNodeBuilder::add_operation_node(ComponentDepsNode *comp_node,
                                                            eDepsOperation_Type optype, DepsEvalOperationCb op, 
                                                            eDepsOperation_Code opcode, const string &description)
{
	OperationDepsNode *op_node = comp_node->add_operation(optype, op, opcode, description);
	m_graph->operations.push_back(op_node);
	return op_node;
}

OperationDepsNode *DepsgraphNodeBuilder::add_operation_node(ID *id, eDepsNode_Type comp_type, const string &comp_name,
                                                            eDepsOperation_Type optype, DepsEvalOperationCb op,
                                                            eDepsOperation_Code opcode, const string &description)
{
	ComponentDepsNode *comp_node = add_component_node(id, comp_type, comp_name);
	
	OperationDepsNode *op_node = comp_node->add_operation(optype, op, opcode, description);
	m_graph->operations.push_back(op_node);
	
	return op_node;
}

/* ************************************************* */
/* Relations Builder */

RNAPathKey::RNAPathKey(ID *id, const string &path) :
    id(id)
{
	/* create ID pointer for root of path lookup */
	PointerRNA id_ptr;
	RNA_id_pointer_create(id, &id_ptr);
	/* try to resolve path... */
	if (!RNA_path_resolve(&id_ptr, path.c_str(), &this->ptr, &this->prop)) {
		this->ptr = PointerRNA_NULL;
		this->prop = NULL;
	}
}

DepsgraphRelationBuilder::DepsgraphRelationBuilder(Depsgraph *graph) :
    m_graph(graph)
{
}

RootDepsNode *DepsgraphRelationBuilder::find_node(const RootKey &key) const
{
	return m_graph->root_node;
}

TimeSourceDepsNode *DepsgraphRelationBuilder::find_node(const TimeSourceKey &key) const
{
	if (key.id) {
		/* XXX TODO */
		return NULL;
	}
	else {
		return m_graph->root_node->time_source;
	}
}

ComponentDepsNode *DepsgraphRelationBuilder::find_node(const ComponentKey &key) const
{
	IDDepsNode *id_node = m_graph->find_id_node(key.id);
	if (!id_node) {
		fprintf(stderr, "find_node component: Could not find ID\n");
		return NULL;
	}
	
	ComponentDepsNode *node = id_node->find_component(key.type, key.name);
	return node;
}

OperationDepsNode *DepsgraphRelationBuilder::find_node(const OperationKey &key) const
{
	IDDepsNode *id_node = m_graph->find_id_node(key.id);
	if (!id_node) {
		fprintf(stderr, "find_node operation: Could not find ID\n");
		return NULL;
	}
	
	ComponentDepsNode *comp_node = id_node->find_component(key.component_type, key.component_name);
	if (!comp_node) {
		fprintf(stderr, "find_node operation: Could not find component\n");
		return NULL;
	}
	
	OperationDepsNode *op_node = comp_node->find_operation(key.opcode, key.name);
	if (!op_node) {
		fprintf(stderr, "find_node_operation: Failed for (%d, '%s')\n", key.opcode, key.name.c_str());
	}
	return op_node;
}

DepsNode *DepsgraphRelationBuilder::find_node(const RNAPathKey &key) const
{
	return m_graph->find_node_from_pointer(&key.ptr, key.prop);
}

void DepsgraphRelationBuilder::add_time_relation(TimeSourceDepsNode *timesrc, DepsNode *node_to, const string &description)
{
	if (timesrc && node_to) {
		m_graph->add_new_relation(timesrc, node_to, DEPSREL_TYPE_TIME, description);
	}
	else {
		fprintf(stderr, "add_time_relation(%p = %s, %p = %s, %s) Failed\n", 
		        timesrc,   (timesrc) ? timesrc->identifier().c_str() : "<None>",
		        node_to,   (node_to) ? node_to->identifier().c_str() : "<None>",
		        description.c_str());
	}
}

void DepsgraphRelationBuilder::add_operation_relation(OperationDepsNode *node_from, OperationDepsNode *node_to,
                                                      eDepsRelation_Type type, const string &description)
{
	if (node_from && node_to) {
		m_graph->add_new_relation(node_from, node_to, type, description);
	}
	else {
		fprintf(stderr, "add_operation_relation(%p = %s, %p = %s, %d, %s) Failed\n",
		        node_from, (node_from) ? node_from->identifier().c_str() : "<None>",
		        node_to,   (node_to)   ? node_to->identifier().c_str() : "<None>",
		        type, description.c_str());
	}
}

/* -------------------------------------------------- */

/* performs a transitive reduction to remove redundant relations
 * http://en.wikipedia.org/wiki/Transitive_reduction
 * 
 * XXX The current implementation is somewhat naive and has O(V*E) worst case runtime.
 * A more optimized algorithm can be implemented later, e.g.
 * 
 * http://www.sciencedirect.com/science/article/pii/0304397588900321/pdf?md5=3391e309b708b6f9cdedcd08f84f4afc&pid=1-s2.0-0304397588900321-main.pdf
 * 
 * Care has to be taken to make sure the algorithm can handle the cyclic case too!
 * (unless we can to prevent this case early on)
 */

enum {
	OP_VISITED = 1,
	OP_REACHABLE = 2,
};

static void deg_graph_tag_paths_recursive(DepsNode *node)
{
	if (node->done & OP_VISITED)
		return;
	node->done |= OP_VISITED;
	
	for (OperationDepsNode::Relations::const_iterator it = node->inlinks.begin(); it != node->inlinks.end(); ++it) {
		DepsRelation *rel = *it;
		
		deg_graph_tag_paths_recursive(rel->from);
		/* do this only in inlinks loop, so the target node does not get flagged! */
		rel->from->done |= OP_REACHABLE;
	}
}

static void deg_graph_transitive_reduction(Depsgraph *graph)
{
	for (Depsgraph::OperationNodes::const_iterator it_target = graph->operations.begin(); it_target != graph->operations.end(); ++it_target) {
		OperationDepsNode *target = *it_target;
		
		/* clear tags */
		for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin(); it != graph->operations.end(); ++it) {
			OperationDepsNode *node = *it;
			node->done = 0;
		}
		
		/* mark nodes from which we can reach the target
		 * start with children, so the target node and direct children are not flagged
		 */
		target->done |= OP_VISITED;
		for (OperationDepsNode::Relations::const_iterator it = target->inlinks.begin(); it != target->inlinks.end(); ++it) {
			DepsRelation *rel = *it;
			
			deg_graph_tag_paths_recursive(rel->from);
		}
		
		/* remove redundant paths to the target */
		for (DepsNode::Relations::const_iterator it_rel = target->inlinks.begin(); it_rel != target->inlinks.end();) {
			DepsRelation *rel = *it_rel;
			++it_rel; /* increment in advance, so we can safely remove the relation */
			
			if (rel->from->type == DEPSNODE_TYPE_TIMESOURCE) {
				/* HACK: time source nodes don't get "done" flag set/cleared */
				// TODO: there will be other types in future, so iterators above need modifying
			}
			else if (rel->from->done & OP_REACHABLE) {
				OBJECT_GUARDED_DELETE(rel, DepsRelation);
			}
		}
	}
}

static void deg_graph_flush_node_layers(Depsgraph *graph)
{
	std::stack<OperationDepsNode*> stack;

	for (Depsgraph::OperationNodes::const_iterator it_op = graph->operations.begin();
	     it_op != graph->operations.end();
	     ++it_op)
	{
		OperationDepsNode *node = *it_op;
		node->done = 0;
		node->num_links_pending = 0;
		for (OperationDepsNode::Relations::const_iterator it_rel = node->inlinks.begin();
		     it_rel != node->inlinks.end();
		     ++it_rel)
		{
			DepsRelation *rel = *it_rel;
			if (rel->from->type == DEPSNODE_TYPE_OPERATION) {
				++node->num_links_pending;
			}
		}
		if (node->num_links_pending == 0) {
			stack.push(node);
		}
	}

	while (!stack.empty()) {
		OperationDepsNode *node = stack.top();
		if (node->done == 0 && node->outlinks.size() != 0) {
			for (OperationDepsNode::Relations::const_iterator it_rel = node->outlinks.begin();
			     it_rel != node->outlinks.end();
			     ++it_rel)
			{
				DepsRelation *rel = *it_rel;
				if (rel->to->type == DEPSNODE_TYPE_OPERATION) {
					OperationDepsNode *to = (OperationDepsNode *)rel->to;
					BLI_assert(to->num_links_pending > 0);
					--to->num_links_pending;
					if (to->num_links_pending == 0) {
						stack.push(to);
					}
				}
			}
			node->done = 1;
		}
		else {
			stack.pop();
			IDDepsNode *id_node = node->owner->owner;
			for (OperationDepsNode::Relations::const_iterator it_rel = node->inlinks.begin();
			     it_rel != node->inlinks.end();
			     ++it_rel)
			{
				DepsRelation *rel = *it_rel;
				if (rel->from->type == DEPSNODE_TYPE_OPERATION) {
					OperationDepsNode *from = (OperationDepsNode *)rel->from;
					IDDepsNode *id_from = from->owner->owner;
					id_node->layers |= id_from->layers;
				}
			}
		}
	}
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
	/* create root node for scene first
	 * - this way it should be the first in the graph,
	 *   reflecting its role as the entrypoint
	 */
	node_builder.add_root_node();
	node_builder.build_scene(scene);
	
	DepsgraphRelationBuilder relation_builder(graph);
	/* hook scene up to the root node as entrypoint to graph */
	/* XXX what does this relation actually mean?
	 * it doesnt add any operations anyway and is not clear what part of the scene is to be connected.
	 */
	//relation_builder.add_relation(RootKey(), IDKey(scene), DEPSREL_TYPE_ROOT_TO_ACTIVE, "Root to Active Scene");
	relation_builder.build_scene(scene);
	
	
	deg_graph_transitive_reduction(graph);
	deg_graph_flush_node_layers(graph);
}

/* Tag relations for update. */
void DEG_graph_tag_relations_update(Depsgraph *graph)
{
	graph->need_update = true;
}

/* Create new graph if didn't exist yet,
 * or update relations if graph was tagged for update.
 */
void DEG_scene_relations_update(Main *bmain, Scene *scene)
{
	if (scene->depsgraph == NULL) {
		/* Rebuild graph from scratch and exit. */
		scene->depsgraph = DEG_graph_new();
		DEG_graph_build_from_scene(scene->depsgraph, bmain, scene);
		return;
	}

	Depsgraph *graph = scene->depsgraph;
	if (!graph->need_update) {
		/* Graph is up to date, nothing to do. */
		return;
	}

	/* Store all oeprations which needs to be re-tagged in new graph. */
	for (Depsgraph::EntryTags::const_iterator it = graph->entry_tags.begin();
	     it != graph->entry_tags.end();
	     ++it)
	{
		OperationDepsNode *node = *it;
		/* TODO(sergey): Ideally we'll need to only re-tag operations,
		 * not the whole ID nodes.
		 */
		graph->add_id_tag(node->owner->owner->id);
	}

	/* Clear all previous nodes and operations. */
	graph->clear_all_nodes();
	graph->operations.clear();
	graph->entry_tags.clear();
	graph->invisible_entry_tags.clear();

	/* Build new nodes and relations. */
	DEG_graph_build_from_scene(graph, bmain, scene);

	for (Depsgraph::IDTags::const_iterator it = graph->id_tags.begin();
	     it != graph->id_tags.end();
	     ++it)
	{
		const ID *id = *it;
		DEG_id_tag_update(graph, id);
	}
	graph->id_tags.clear();

	graph->need_update = false;
}

/* ************************************************* */
