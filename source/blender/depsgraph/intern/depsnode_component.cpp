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
 */

#include <string.h>

extern "C" {
#include "DNA_object_types.h"

#include "BKE_action.h"
} /* extern "C" */

#include "depsnode_component.h" /* own include */
#include "depsnode_operation.h"
#include "depsgraph_intern.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

/* ******************************************************** */
/* Outer Nodes */

/* Standard Component Methods ============================= */

ComponentDepsNode::ComponentDepsNode() :
    entry_operation(NULL),
    exit_operation(NULL)
{
}

/* Initialise 'component' node - from pointer data given */
void ComponentDepsNode::init(const ID *id, const string &subdata)
{
	/* hook up eval context? */
	// XXX: maybe this needs a special API?
}

/* Copy 'component' node */
void ComponentDepsNode::copy(DepsgraphCopyContext *dcc, const ComponentDepsNode *src)
{
	/* duplicate list of operation nodes */
	this->operations.clear();
	
	for (OperationMap::const_iterator it = src->operations.begin(); it != src->operations.end(); ++it) {
		const string &pchan_name = it->first;
		OperationDepsNode *src_op = it->second;
		
		/* recursive copy */
		DepsNodeFactory *factory = DEG_node_get_factory(src_op);
		OperationDepsNode *dst_op = (OperationDepsNode *)factory->copy_node(dcc, src_op);
		this->operations[pchan_name] = dst_op;
			
		/* fix links... */
		// ...
	}
	
	/* copy evaluation contexts */
	//
}

/* Free 'component' node */
ComponentDepsNode::~ComponentDepsNode()
{
	clear_operations();
}

OperationDepsNode *ComponentDepsNode::find_operation(const string &name) const
{
	OperationMap::const_iterator it = this->operations.find(name);
	return it != this->operations.end() ? it->second : NULL;
}

OperationDepsNode *ComponentDepsNode::add_operation(eDepsNode_Type type, eDepsOperation_Type optype, 
                                                    DepsEvalOperationCb op, const string &name)
{
	DepsNodeFactory *factory = DEG_get_node_factory(type);
	eDepsNode_Type factory_type = factory->component_type();
	/* make sure only valid operations are added to this component */
	BLI_assert(factory_type == DEPSNODE_TYPE_UNDEFINED || factory_type == this->type);
	
	OperationDepsNode *op_node = find_operation(name);
	if (!op_node) {
		op_node = (OperationDepsNode *)factory->create_node(this->owner->id, "", name);
		
		/* register */
		this->operations[name] = op_node;
		op_node->owner = this;
	}
	
	/* attach extra data */
	op_node->evaluate = op;
	op_node->optype = optype;
	op_node->name = name;
	
	return op_node;
}

void ComponentDepsNode::remove_operation(const string &name)
{
	OperationDepsNode *op_node = find_operation(name);
	if (op_node) {
		/* unregister */
		this->operations.erase(name);
		
		delete op_node;
	}
}

void ComponentDepsNode::clear_operations()
{
	for (OperationMap::const_iterator it = operations.begin(); it != operations.end(); ++it) {
		OperationDepsNode *op_node = it->second;
		delete op_node;
	}
	operations.clear();
}

void ComponentDepsNode::tag_update(Depsgraph *graph)
{
	for (OperationMap::const_iterator it = operations.begin(); it != operations.end(); ++it) {
		OperationDepsNode *op_node = it->second;
		op_node->tag_update(graph);
	}
}

/* Parameter Component Defines ============================ */

DEG_DEPSNODE_DEFINE(ParametersComponentDepsNode, DEPSNODE_TYPE_PARAMETERS, "Parameters Component");
static DepsNodeFactoryImpl<ParametersComponentDepsNode> DNTI_PARAMETERS;

/* Animation Component Defines ============================ */

DEG_DEPSNODE_DEFINE(AnimationComponentDepsNode, DEPSNODE_TYPE_ANIMATION, "Animation Component");
static DepsNodeFactoryImpl<AnimationComponentDepsNode> DNTI_ANIMATION;

/* Transform Component Defines ============================ */

DEG_DEPSNODE_DEFINE(TransformComponentDepsNode, DEPSNODE_TYPE_TRANSFORM, "Transform Component");
static DepsNodeFactoryImpl<TransformComponentDepsNode> DNTI_TRANSFORM;

/* Proxy Component Defines ================================ */

DEG_DEPSNODE_DEFINE(ProxyComponentDepsNode, DEPSNODE_TYPE_PROXY, "Proxy Component");
static DepsNodeFactoryImpl<ProxyComponentDepsNode> DNTI_PROXY;

/* Geometry Component Defines ============================= */

DEG_DEPSNODE_DEFINE(GeometryComponentDepsNode, DEPSNODE_TYPE_GEOMETRY, "Geometry Component");
static DepsNodeFactoryImpl<GeometryComponentDepsNode> DNTI_GEOMETRY;

/* Sequencer Component Defines ============================ */

DEG_DEPSNODE_DEFINE(SequencerComponentDepsNode, DEPSNODE_TYPE_SEQUENCER, "Sequencer Component");
static DepsNodeFactoryImpl<SequencerComponentDepsNode> DNTI_SEQUENCER;

/* Pose Component ========================================= */

DEG_DEPSNODE_DEFINE(PoseComponentDepsNode, DEPSNODE_TYPE_EVAL_POSE, "Pose Eval Component");
static DepsNodeFactoryImpl<PoseComponentDepsNode> DNTI_EVAL_POSE;

/* Bone Component ========================================= */

/* Initialise 'bone component' node - from pointer data given */
void BoneComponentDepsNode::init(const ID *id, const string &subdata)
{
	/* generic component-node... */
	ComponentDepsNode::init(id, subdata);
	
	/* name of component comes is bone name */
	this->name = subdata;
	
	/* bone-specific node data */
	Object *ob = (Object *)id;
	this->pchan = BKE_pose_channel_find_name(ob->pose, subdata.c_str());
}

DEG_DEPSNODE_DEFINE(BoneComponentDepsNode, DEPSNODE_TYPE_BONE, "Bone Component");
static DepsNodeFactoryImpl<BoneComponentDepsNode> DNTI_BONE;

/* Particles Component Defines ============================ */

DEG_DEPSNODE_DEFINE(ParticlesComponentDepsNode, DEPSNODE_TYPE_EVAL_PARTICLES, "Particles Component");
static DepsNodeFactoryImpl<ParticlesComponentDepsNode> DNTI_EVAL_PARTICLES;


void DEG_register_component_depsnodes()
{
	DEG_register_node_typeinfo(&DNTI_PARAMETERS);
	DEG_register_node_typeinfo(&DNTI_PROXY);
	DEG_register_node_typeinfo(&DNTI_ANIMATION);
	DEG_register_node_typeinfo(&DNTI_TRANSFORM);
	DEG_register_node_typeinfo(&DNTI_GEOMETRY);
	DEG_register_node_typeinfo(&DNTI_SEQUENCER);
	
	DEG_register_node_typeinfo(&DNTI_EVAL_POSE);
	DEG_register_node_typeinfo(&DNTI_BONE);
	
	DEG_register_node_typeinfo(&DNTI_EVAL_PARTICLES);
}
