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
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Datatypes for internal use in the Depsgraph
 *
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#ifndef __DEPSGRAPH_H__
#define __DEPSGRAPH_H__

#include "BLI_threads.h"  /* for SpinLock */

#include "depsgraph_types.h"

#include "depsgraph_util_map.h"
#include "depsgraph_util_set.h"

struct PointerRNA;
struct PropertyRNA;

struct DepsNode;
struct RootDepsNode;
struct TimeSourceDepsNode;
struct IDDepsNode;
struct SubgraphDepsNode;
struct ComponentDepsNode;
struct OperationDepsNode;

/* ************************************* */
/* Relationships Between Nodes */

/* Settings/Tags on Relationship */
typedef enum eDepsRelation_Flag {
	/* "touched" tag is used when filtering, to know which to collect */
	DEPSREL_FLAG_TEMP_TAG   = (1 << 0),

	/* "cyclic" link - when detecting cycles, this relationship was the one
	 * which triggers a cyclic relationship to exist in the graph
	 */
	DEPSREL_FLAG_CYCLIC     = (1 << 1),
} eDepsRelation_Flag;

/* B depends on A (A -> B) */
struct DepsRelation {
	/* the nodes in the relationship (since this is shared between the nodes) */
	DepsNode *from;               /* A */
	DepsNode *to;                 /* B */

	/* relationship attributes */
	string name;                  /* label for debugging */

	eDepsRelation_Type type;      /* type */
	int flag;                     /* (eDepsRelation_Flag) */

	DepsRelation(DepsNode *from,
	             DepsNode *to,
	             eDepsRelation_Type type,
	             const string &description);

	~DepsRelation();
};

/* ************************************* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	typedef unordered_map<const ID *, IDDepsNode *> IDNodeMap;
	typedef unordered_set<SubgraphDepsNode *> Subgraphs;
	typedef unordered_set<OperationDepsNode *> EntryTags;
	typedef vector<OperationDepsNode *> OperationNodes;

	Depsgraph();
	~Depsgraph();

	/* Find node which matches the specified description.
	 *
	 * < id: ID block that is associated with this
	 * < (subdata): identifier used for sub-ID data (e.g. bone)
	 * < type: type of node we're dealing with
	 * < (name): custom identifier assigned to node
	 *
	 * > returns: A node matching the required characteristics if it exists
	 *            OR NULL if no such node exists in the graph
	 */
	DepsNode *find_node(const ID *id,
	                    eDepsNode_Type type,
	                    const string &subdata,
	                    const string &name);

	/* Convenience wrapper to find node given just pointer + property.
	 * < ptr: pointer to the data that node will represent
	 * < (prop): optional property affected - providing this effectively results in inner nodes being returned
	 *
	 * > returns: A node matching the required characteristics if it exists
	 *            OR NULL if no such node exists in the graph
	 */
	DepsNode *find_node_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop) const;

	RootDepsNode *add_root_node();

	TimeSourceDepsNode *find_time_source(const ID *id = NULL) const;

	SubgraphDepsNode *add_subgraph_node(const ID *id);
	void remove_subgraph_node(SubgraphDepsNode *subgraph_node);
	void clear_subgraph_nodes();

	IDDepsNode *find_id_node(const ID *id) const;
	IDDepsNode *add_id_node(ID *id, const string &name = "");
	void remove_id_node(const ID *id);
	void clear_id_nodes();

	/* Add new relationship between two nodes. */
	DepsRelation *add_new_relation(OperationDepsNode *from,
	                               OperationDepsNode *to,
	                               eDepsRelation_Type type,
	                               const string &description);

	DepsRelation *add_new_relation(DepsNode *from,
	                               DepsNode *to,
	                               eDepsRelation_Type type,
	                               const string &description);

	/* Tag a specific node as needing updates. */
	void add_entry_tag(OperationDepsNode *node);

	/* Clear storage used by all nodes. */
	void clear_all_nodes();

	/* Core Graph Functionality ........... */

	/* <ID : IDDepsNode> mapping from ID blocks to nodes representing these blocks
	 * (for quick lookups). */
	IDNodeMap id_hash;

	/* "root" node - the one where all evaluation enters from. */
	RootDepsNode *root_node;

	/* Subgraphs referenced in tree. */
	Subgraphs subgraphs;

	/* Indicates whether relations needs to be updated. */
	bool need_update;

	/* Quick-Access Temp Data ............. */

	/* Nodes which have been tagged as "directly modified". */
	EntryTags entry_tags;

	/* Convenience Data ................... */

	/* XXX: should be collected after building (if actually needed?) */
	/* All operation nodes, sorted in order of single-thread traversal order. */
	OperationNodes operations;

	/* Spin lock for threading-critical operations.
	 * Mainly used by graph evaluation.
	 */
	SpinLock lock;

	/* Layers Visibility .................. */

	/* Tag a specific invisible node as needing updates when becoming visible. */
	void add_invisible_entry_tag(OperationDepsNode *node);

	/* Visible layers bitfield, used for skipping invisible objects updates. */
	int layers;

	/* Invisible nodes which have been tagged as "directly modified".
	 * They're being flushed for update after graph visibility changes.
	 *
	 * TODO(sergey): This is a tempotrary solution for until we'll have storage
	 * associated with the IDs, so we can keep dirty/tagged for update/etc flags
	 * in there. This would also solve issues with accessing .updated ID flag
	 * from python.
	 */
	EntryTags invisible_entry_tags;

	// XXX: additional stuff like eval contexts, mempools for allocating nodes from, etc.
};

/* Helper macros for interating over set of relationship links
 * incident on each node.
 *
 * NOTE: it is safe to perform removal operations here...
 *
 * < relations_set: (DepsNode::Relations) set of relationships (in/out links)
 * > relation:  (DepsRelation *) identifier where DepsRelation that we're
 *              currently accessing comes up
 */
#define DEPSNODE_RELATIONS_ITER_BEGIN(relations_set_, relation_)               \
	{                                                                          \
		OperationDepsNode::Relations::const_iterator __rel_iter = relations_set_.begin();  \
		while (__rel_iter != relations_set_.end()) {                           \
			DepsRelation *relation_ = *__rel_iter;                             \
			++__rel_iter;

			/* ... code for iterator body can be written here ... */

#define DEPSNODE_RELATIONS_ITER_END                                            \
		}                                                                      \
	}

/* ************************************* */

#endif // __DEPSGRAPH_H__
