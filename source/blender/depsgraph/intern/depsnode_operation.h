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

#ifndef __DEPSNODE_OPERATION_H__
#define __DEPSNODE_OPERATION_H__

#include "MEM_guardedalloc.h"

extern "C" {
#include "RNA_access.h"
}

#include "depsnode.h"

#include "depsgraph_util_map.h"
#include "depsgraph_util_set.h"

struct ID;

struct Depsgraph;
struct DepsgraphCopyContext;

/* Flags for Depsgraph Nodes */
typedef enum eDepsOperation_Flag {
	/* node needs to be updated */
	DEPSOP_FLAG_NEEDS_UPDATE       = (1 << 0),
	
	/* node was directly modified, causing need for update */
	/* XXX: intention is to make it easier to tell when we just need to take subgraphs */
	DEPSOP_FLAG_DIRECTLY_MODIFIED  = (1 << 1),

	/* Operation is evaluated using CPython; has GIL and security implications... */
	DEPSOP_FLAG_USES_PYTHON   = (1 << 2),
} eDepsOperation_Flag;

/* Atomic Operation - Base type for all operations */
struct OperationDepsNode : public DepsNode {
	typedef unordered_set<DepsRelation *> Relations;
	
	OperationDepsNode();
	~OperationDepsNode();
	
	void tag_update(Depsgraph *graph);
	
	bool is_noop() const { return evaluate == NULL; }
	
	ComponentDepsNode *owner;     /* component that contains the operation */
	
	DepsEvalOperationCb evaluate; /* callback for operation */
	
	PointerRNA ptr;               /* item that operation is to be performed on (optional) */
	
	Relations inlinks;          /* nodes which this one depends on */
	Relations outlinks;         /* nodes which depend on this one */
	
	double start_time;            /* (secs) last timestamp (in seconds) when operation was started */
	double last_time;             /* (seconds) time in seconds that last evaluation took */
	
	uint32_t num_links_pending; /* how many inlinks are we still waiting on before we can be evaluated... */
	float eval_priority;
	
	short optype;                 /* (eDepsOperation_Type) stage of evaluation */
	short flag;                   /* (eDepsOperation_Flag) extra settings affecting evaluation */
	short done;                   /* generic tag for traversal algorithms */
};

/* Macros for common static typeinfo */
#define DEG_DEPSNODE_OP_DEFINE(NodeType, type_, comp_type_, tname_) \
	const DepsNode::TypeInfo NodeType::typeinfo = DepsNode::TypeInfo(type_, tname_, comp_type_)

struct NoopDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct ParametersOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct AnimationOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct ProxyOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct TransformOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct GeometryOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct SequencerOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct UpdateOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct DriverOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct PoseOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct BoneOperationDepsNode : public OperationDepsNode {
	void init(const ID *id, const string &subdata);
	
	DEG_DEPSNODE_DECLARE;
};

struct ParticlesOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct RigidBodyOperationDepsNode : public OperationDepsNode {
	DEG_DEPSNODE_DECLARE;
};

void DEG_register_operation_depsnodes();

#endif // __DEPSNODE_OPERATION_H__
