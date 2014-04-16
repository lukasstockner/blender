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

#include "depsgraph_util_rna.h"
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

IDDepsNode *DepsgraphNodeBuilder::add_id_node(IDPtr id)
{
	const char *idtype_name = BKE_idcode_to_name(GS(id->name));
	return m_graph->add_id_node(id, string_format("%s [%s]", id->name+2, idtype_name));
}

TimeSourceDepsNode *DepsgraphNodeBuilder::add_time_source(IDPtr id)
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

ComponentDepsNode *DepsgraphNodeBuilder::add_component_node(IDDepsNode *id_node, eDepsNode_Type comp_type, const string &subdata)
{
	ComponentDepsNode *comp_node = id_node->add_component(comp_type);
	comp_node->owner = id_node;
	return comp_node;
}

OperationDepsNode *DepsgraphNodeBuilder::add_operation_node(ComponentDepsNode *comp_node, eDepsNode_Type type,
                                                            eDepsOperation_Type optype, DepsEvalOperationCb op, const string &description,
                                                            PointerRNA ptr)
{
	OperationDepsNode *op_node = comp_node->add_operation(type, optype, op, description);
	op_node->ptr = ptr;
	return op_node;
}

OperationDepsNode *DepsgraphNodeBuilder::add_operation_node(IDDepsNode *id_node, eDepsNode_Type type,
                                                            eDepsOperation_Type optype, DepsEvalOperationCb op, const string &description,
                                                            PointerRNA ptr)
{
	DepsNodeFactory *factory = DEG_get_node_factory(type);
	ComponentDepsNode *comp_node = id_node->add_component(factory->component_type());
	OperationDepsNode *op_node = comp_node->add_operation(type, optype, op, description);
	op_node->ptr = ptr;
	return op_node;
}

/* ************************************************* */
/* Relations Builder */

RNAPathKey::RNAPathKey(IDPtr id, const string &path) :
    id(id)
{
	/* create ID pointer for root of path lookup */
	PointerRNA id_ptr = make_rna_id_pointer(id);
	/* try to resolve path... */
	if (!RNA_path_resolve(&id_ptr, path.c_str(), &this->ptr, &this->prop)) {
		this->ptr = PointerRNA_NULL;
		this->prop = NULL;
	}
}

RNAPathKey::RNAPathKey(IDPtr id, const PointerRNA &ptr, PropertyRNA *prop) :
    id(id),
    ptr(ptr),
    prop(prop)
{
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

DepsNode *DepsgraphRelationBuilder::find_node(const RNAPathKey &key) const
{
	return m_graph->find_node_from_pointer(&key.ptr, key.prop);
}

void DepsgraphRelationBuilder::add_node_relation(DepsNode *node_from, DepsNode *node_to,
                                            eDepsRelation_Type type, const string &description)
{
	m_graph->add_new_relation(node_from, node_to, type, description);
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
	relation_builder.add_relation(RootKey(), IDKey(scene), DEPSREL_TYPE_ROOT_TO_ACTIVE, "Root to Active Scene");
	relation_builder.build_scene(scene);
	
#if 0
	/* ensure that all implicit constraints between nodes are satisfied */
	DEG_graph_validate_links(graph);
	
	/* sort nodes to determine evaluation order (in most cases) */
	DEG_graph_sort(graph);
#endif
}

/* ************************************************* */

