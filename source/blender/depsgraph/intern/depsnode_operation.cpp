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

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_object_types.h"

#include "BKE_action.h"
} /* extern "C" */

#include "depsnode_operation.h" /* own include */
#include "depsnode_component.h"
#include "depsgraph.h"
#include "depsgraph_intern.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

/* ******************************************************** */
/* Inner Nodes */

OperationDepsNode::OperationDepsNode() :
    eval_priority(0.0f)
{
}

OperationDepsNode::~OperationDepsNode()
{
	/* free links
	 * note: deleting relations will remove them from the node relations set,
	 * but only touch the same position as we are using here, which is safe.
	 */
	DEPSNODE_RELATIONS_ITER_BEGIN(this->inlinks, rel)
		delete rel;
	DEPSNODE_RELATIONS_ITER_END;
	
	DEPSNODE_RELATIONS_ITER_BEGIN(this->outlinks, rel)
		delete rel;
	DEPSNODE_RELATIONS_ITER_END;
}

void OperationDepsNode::tag_update(Depsgraph *graph)
{
	/* tag for update, but also not that this was the source of an update */
	flag |= (DEPSOP_FLAG_NEEDS_UPDATE | DEPSOP_FLAG_DIRECTLY_MODIFIED);
	
	graph->add_entry_tag(this);
}

/* No-Op ================================================== */

DEG_DEPSNODE_OP_DEFINE(NoopDepsNode, DEPSNODE_TYPE_OP_NOOP, DEPSNODE_TYPE_UNDEFINED, "NoOp Operation");
static DepsNodeFactoryImpl<NoopDepsNode> DNTI_OP_NOOP;

/* Parameter Operation ==================================== */

DEG_DEPSNODE_OP_DEFINE(ParametersOperationDepsNode, DEPSNODE_TYPE_OP_PARAMETER, DEPSNODE_TYPE_PARAMETERS, "Parameters Operation");
static DepsNodeFactoryImpl<ParametersOperationDepsNode> DNTI_OP_PARAMETERS;

/* Proxy Operation ======================================== */

DEG_DEPSNODE_OP_DEFINE(ProxyOperationDepsNode, DEPSNODE_TYPE_OP_PROXY, DEPSNODE_TYPE_PROXY, "Proxy Operation");
static DepsNodeFactoryImpl<ProxyOperationDepsNode> DNTI_OP_PROXY;

/* Animation Operation ==================================== */

DEG_DEPSNODE_OP_DEFINE(AnimationOperationDepsNode, DEPSNODE_TYPE_OP_ANIMATION, DEPSNODE_TYPE_ANIMATION, "Animation Operation");
static DepsNodeFactoryImpl<AnimationOperationDepsNode> DNTI_OP_ANIMATION;

/* Transform Operation ==================================== */

DEG_DEPSNODE_OP_DEFINE(TransformOperationDepsNode, DEPSNODE_TYPE_OP_TRANSFORM, DEPSNODE_TYPE_TRANSFORM, "Transform Operation");
static DepsNodeFactoryImpl<TransformOperationDepsNode> DNTI_OP_TRANSFORM;

/* Geometry Operation ===================================== */

DEG_DEPSNODE_OP_DEFINE(GeometryOperationDepsNode, DEPSNODE_TYPE_OP_GEOMETRY, DEPSNODE_TYPE_GEOMETRY, "Geometry Operation");
static DepsNodeFactoryImpl<GeometryOperationDepsNode> DNTI_OP_GEOMETRY;

/* Sequencer Operation ==================================== */

DEG_DEPSNODE_OP_DEFINE(SequencerOperationDepsNode, DEPSNODE_TYPE_OP_SEQUENCER, DEPSNODE_TYPE_SEQUENCER, "Sequencer Operation");
static DepsNodeFactoryImpl<SequencerOperationDepsNode> DNTI_OP_SEQUENCER;

/* Update Operation ======================================= */

DEG_DEPSNODE_OP_DEFINE(UpdateOperationDepsNode, DEPSNODE_TYPE_OP_UPDATE, DEPSNODE_TYPE_PARAMETERS, "RNA Update Operation");
static DepsNodeFactoryImpl<UpdateOperationDepsNode> DNTI_OP_UPDATE;

/* Driver Operation ===================================== */
// XXX: some special tweaks may be needed for this one...

DEG_DEPSNODE_OP_DEFINE(DriverOperationDepsNode, DEPSNODE_TYPE_OP_DRIVER, DEPSNODE_TYPE_PARAMETERS, "Driver Operation");
static DepsNodeFactoryImpl<DriverOperationDepsNode> DNTI_OP_DRIVER;

/* Pose Operation ========================================= */

DEG_DEPSNODE_OP_DEFINE(PoseOperationDepsNode, DEPSNODE_TYPE_OP_POSE, DEPSNODE_TYPE_EVAL_POSE, "Pose Operation");
static DepsNodeFactoryImpl<PoseOperationDepsNode> DNTI_OP_POSE;

/* Bone Operation ========================================= */

/* Init local data for bone operation */
void BoneOperationDepsNode::init(const ID *id, const string &subdata)
{
	Object *ob;
	bPoseChannel *pchan;
	
	/* set up RNA Pointer to affected bone */
	ob = (Object *)id;
	pchan = BKE_pose_channel_find_name(ob->pose, subdata.c_str());
	
	RNA_pointer_create((ID *)id, &RNA_PoseBone, pchan, &this->ptr);
}

DEG_DEPSNODE_OP_DEFINE(BoneOperationDepsNode, DEPSNODE_TYPE_OP_BONE, DEPSNODE_TYPE_BONE, "Bone Operation");
static DepsNodeFactoryImpl<BoneOperationDepsNode> DNTI_OP_BONE;

/* Particle Operation ===================================== */

DEG_DEPSNODE_OP_DEFINE(ParticlesOperationDepsNode, DEPSNODE_TYPE_OP_PARTICLE, DEPSNODE_TYPE_EVAL_PARTICLES, "Particles Operation");
static DepsNodeFactoryImpl<ParticlesOperationDepsNode> DNTI_OP_PARTICLES;

/* RigidBody Operation ==================================== */
/* Note: RigidBody Operations are reserved for scene-level rigidbody sim steps */

DEG_DEPSNODE_OP_DEFINE(RigidBodyOperationDepsNode, DEPSNODE_TYPE_OP_RIGIDBODY, DEPSNODE_TYPE_TRANSFORM, "RigidBody Operation");
static DepsNodeFactoryImpl<RigidBodyOperationDepsNode> DNTI_OP_RIGIDBODY;


void DEG_register_operation_depsnodes()
{
	DEG_register_node_typeinfo(&DNTI_OP_NOOP);
	
	DEG_register_node_typeinfo(&DNTI_OP_PARAMETERS);
	DEG_register_node_typeinfo(&DNTI_OP_PROXY);
	DEG_register_node_typeinfo(&DNTI_OP_ANIMATION);
	DEG_register_node_typeinfo(&DNTI_OP_TRANSFORM);
	DEG_register_node_typeinfo(&DNTI_OP_GEOMETRY);
	DEG_register_node_typeinfo(&DNTI_OP_SEQUENCER);
	
	DEG_register_node_typeinfo(&DNTI_OP_UPDATE);
	DEG_register_node_typeinfo(&DNTI_OP_DRIVER);
	
	DEG_register_node_typeinfo(&DNTI_OP_POSE);
	DEG_register_node_typeinfo(&DNTI_OP_BONE);
	
	DEG_register_node_typeinfo(&DNTI_OP_PARTICLES);
	DEG_register_node_typeinfo(&DNTI_OP_RIGIDBODY);
}
