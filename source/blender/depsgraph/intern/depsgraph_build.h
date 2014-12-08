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
 * Original Author: Lukas Toenne
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __DEPSGRAPH_BUILD_H__
#define __DEPSGRAPH_BUILD_H__

#include <cstddef> /* for std::size_t */
#include <tuple>
#include <type_traits>

#include "depsgraph_types.h"

#include "depsgraph_util_id.h"
#include "depsgraph_util_rna.h"

struct bConstraint;
struct ListBase;
struct ID;
struct FCurve;
struct Group;
struct Key;
struct Main;
struct Material;
struct MTex;
struct bNodeTree;
struct Object;
struct bPoseChannel;
struct Scene;
struct Tex;
struct World;

struct Depsgraph;
struct DepsNode;
struct DepsNodeHandle;
struct RootDepsNode;
struct SubgraphDepsNode;
struct IDDepsNode;
struct TimeSourceDepsNode;
struct ComponentDepsNode;
struct OperationDepsNode;

namespace detail {

using std::forward;
using std::tuple;
using std::tuple_size;
using std::get;
using std::size_t;

template <size_t N>
struct bind_operation_tuple_impl {
	template <typename Func, typename... Args, typename... ArgsTail>
	static DepsEvalOperationCb bind_operation_tuple(Func &&func, tuple<Args...> &&args, ArgsTail... tail)
	{
		typedef decltype(get<N-1>(args)) T;
		T &&head = get<N-1>(args);
		
		return bind_operation_tuple_impl<N-1>::bind_operation_tuple(forward<Func>(func), forward<tuple<Args...>>(args), forward<T>(head), tail...);
	}
};

template <>
struct bind_operation_tuple_impl<0> {
	template <typename Func, typename... Args, typename... ArgsTail>
	static DepsEvalOperationCb bind_operation_tuple(Func &&func, tuple<Args...> &&args, ArgsTail... tail)
	{
		return std::bind(func, tail...);
	}
};

} /* namespace detail */

template <typename Func, typename... Args>
static DepsEvalOperationCb bind_operation(Func func, Args... args)
{
	typedef std::tuple_size<std::tuple<Args...>> args_size;
	
	return detail::bind_operation_tuple_impl<args_size::value>::bind_operation_tuple(func, std::tuple<Args...>(args...));
}

struct DepsgraphNodeBuilder {
	DepsgraphNodeBuilder(Main *bmain, Depsgraph *graph);
	~DepsgraphNodeBuilder();
	
	RootDepsNode *add_root_node();
	IDDepsNode *add_id_node(ID *id);
	TimeSourceDepsNode *add_time_source(ID *id);
	
	ComponentDepsNode *add_component_node(ID *id, eDepsNode_Type comp_type, const string &comp_name = "");
	
	OperationDepsNode *add_operation_node(ComponentDepsNode *comp_node,
	                                      eDepsOperation_Type optype, DepsEvalOperationCb op, const string &description);
	OperationDepsNode *add_operation_node(ID *id, eDepsNode_Type comp_type, const string &comp_name,
	                                      eDepsOperation_Type optype, DepsEvalOperationCb op, const string &description);
	OperationDepsNode *add_operation_node(ID *id, eDepsNode_Type comp_type,
	                                      eDepsOperation_Type optype, DepsEvalOperationCb op, const string &description)
	{
		return add_operation_node(id, comp_type, "", optype, op, description);
	}
	
	void verify_entry_exit_operations();
	
	void build_scene(Scene *scene);
	SubgraphDepsNode *build_subgraph(Group *group);
	void build_group(Group *group);
	void build_object(Scene *scene, Object *ob);
	void build_object_transform(Scene *scene, Object *ob);
	void build_object_constraints(Scene *scene, Object *ob);
	void build_pose_constraints(Object *ob, bPoseChannel *pchan);
	void build_rigidbody(Scene *scene);
	void build_particles(Object *ob);
	void build_animdata(ID *id);
	OperationDepsNode *build_driver(ID *id, FCurve *fcurve);
	void build_ik_pose(Scene *scene, Object *ob, bPoseChannel *pchan, bConstraint *con);
	void build_splineik_pose(Scene *scene, Object *ob, bPoseChannel *pchan, bConstraint *con);
	void build_rig(Scene *scene, Object *ob);
	void build_shapekeys(Key *key);
	void build_obdata_geom(Scene *scene, Object *ob);
	void build_camera(Object *ob);
	void build_lamp(Object *ob);
	void build_nodetree(DepsNode *owner_node, bNodeTree *ntree);
	void build_material(DepsNode *owner_node, Material *ma);
	void build_texture(DepsNode *owner_node, Tex *tex);
	void build_texture_stack(DepsNode *owner_node, MTex **texture_stack);
	void build_world(World *world);
	void build_compositor(Scene *scene);
	
protected:
	void verify_entry_exit_operations(ComponentDepsNode *node);
	
private:
	Main *m_bmain;
	Depsgraph *m_graph;
};

struct RootKey
{
	RootKey() {}
};

struct TimeSourceKey
{
	TimeSourceKey() : id(NULL) {}
	TimeSourceKey(ID *id) : id(id) {}
	
	ID *id;
};

struct ComponentKey
{
	ComponentKey() : id(NULL), type(DEPSNODE_TYPE_UNDEFINED), name("") {}
	ComponentKey(ID *id, eDepsNode_Type type, const string &name = "") : id(id), type(type), name(name) {}
	
	ID *id;
	eDepsNode_Type type;
	string name;
};

struct OperationKey
{
	OperationKey() : id(NULL), component_type(DEPSNODE_TYPE_UNDEFINED), component_name(""), name("") {}
	OperationKey(ID *id, eDepsNode_Type component_type, const string &name) :
	    id(id), component_type(component_type), component_name(""), name(name)
	{}
	OperationKey(ID *id, eDepsNode_Type component_type, const string &component_name, const string &name) :
	    id(id), component_type(component_type), component_name(component_name), name(name)
	{}
	
	ID *id;
	eDepsNode_Type component_type;
	string component_name;
	string name;
};

struct RNAPathKey
{
	RNAPathKey(ID *id, const string &path);
	RNAPathKey(ID *id, const PointerRNA &ptr, PropertyRNA *prop);
	ID *id;
	PointerRNA ptr;
	PropertyRNA *prop;
};

struct DepsgraphRelationBuilder {
	typedef vector<const char*> RootPChanVector;
	typedef unordered_map<const char*, RootPChanVector> RootPChanMap;

	DepsgraphRelationBuilder(Depsgraph *graph);
	
	template <typename KeyFrom, typename KeyTo>
	void add_relation(const KeyFrom &key_from, const KeyTo &key_to,
	                  eDepsRelation_Type type, const string &description);

	template <typename KeyTo>
	void add_relation(const TimeSourceKey &key_from, const KeyTo &key_to,
	                  eDepsRelation_Type type, const string &description);

	template <typename KeyType>
	void add_node_handle_relation(const KeyType &key_from, const DepsNodeHandle *handle,
	                              eDepsRelation_Type type, const string &description);
	
	void build_scene(Scene *scene);
	void build_object(Scene *scene, Object *ob);
	void build_object_parent(Object *ob);
	void build_constraints(Scene *scene, ID *id, eDepsNode_Type component_type, const string &component_subdata, ListBase *constraints);
	void build_animdata(ID *id);
	void build_driver(ID *id, FCurve *fcurve);
	void build_world(Scene *scene, World *world);
	void build_rigidbody(Scene *scene);
	void build_particles(Scene *scene, Object *ob);
	void build_ik_pose(Object *ob, bPoseChannel *pchan, bConstraint *con, RootPChanMap *root_map);
	void build_splineik_pose(Object *ob, bPoseChannel *pchan, bConstraint *con, RootPChanMap *root_map);
	void build_rig(Scene *scene, Object *ob);
	void build_shapekeys(ID *obdata, Key *key);
	void build_obdata_geom(Scene *scene, Object *ob);
	void build_camera(Object *ob);
	void build_lamp(Object *ob);
	void build_nodetree(ID *owner, bNodeTree *ntree);
	void build_material(ID *owner, Material *ma);
	void build_texture(ID *owner, Tex *tex);
	void build_texture_stack(ID *owner, MTex **texture_stack);
	void build_compositor(Scene *scene);
	
protected:
	RootDepsNode *find_node(const RootKey &key) const;
	TimeSourceDepsNode *find_node(const TimeSourceKey &key) const;
	ComponentDepsNode *find_node(const ComponentKey &key) const;
	OperationDepsNode *find_node(const OperationKey &key) const;
	DepsNode *find_node(const RNAPathKey &key) const;
	
	void add_operation_relation(OperationDepsNode *node_from, OperationDepsNode *node_to,
	                            eDepsRelation_Type type, const string &description);
	
	template <typename KeyType>
	DepsNodeHandle create_node_handle(const KeyType &key, const string &default_name = "");
	
private:
	Depsgraph *m_graph;
};

struct DepsNodeHandle {
	DepsNodeHandle(DepsgraphRelationBuilder *builder, OperationDepsNode *node, const string &default_name = "") :
	    builder(builder),
	    node(node),
	    default_name(default_name)
	{}
	
	DepsgraphRelationBuilder *builder;
	OperationDepsNode *node;
	const string &default_name;
};

/* Inline Function Templates -------------------------------------------------- */

#include "depsnode_component.h"

template <class NodeType>
BLI_INLINE OperationDepsNode *get_entry_operation(NodeType *node)
{ return NULL; }

template <class NodeType>
BLI_INLINE OperationDepsNode *get_exit_operation(NodeType *node)
{ return NULL; }

BLI_INLINE OperationDepsNode *get_entry_operation(OperationDepsNode *node)
{ return node; }

BLI_INLINE OperationDepsNode *get_exit_operation(OperationDepsNode *node)
{ return node; }

BLI_INLINE OperationDepsNode *get_entry_operation(ComponentDepsNode *node)
{ return node ? node->entry_operation : NULL; }

BLI_INLINE OperationDepsNode *get_exit_operation(ComponentDepsNode *node)
{ return node ? node->exit_operation : NULL; }

template <typename KeyFrom, typename KeyTo>
void DepsgraphRelationBuilder::add_relation(const KeyFrom &key_from, const KeyTo &key_to,
                                            eDepsRelation_Type type, const string &description)
{
	OperationDepsNode *op_from = get_exit_operation(find_node(key_from));
	OperationDepsNode *op_to = get_entry_operation(find_node(key_to));
	if (op_from && op_to) {
		add_operation_relation(op_from, op_to, type, description);
	}
	else {
		if (!op_from) {
			/* XXX TODO handle as error or report if needed */
			fprintf(stderr, "add_relation(%d, %s) - Could not find op_from\n",
			        type, description.c_str());
		}
		if (!op_to) {
			/* XXX TODO handle as error or report if needed */
			fprintf(stderr, "add_relation(%d, %s) - Could not find op_to\n",
			        type, description.c_str());
		}
	}
}

template <typename KeyTo>
void DepsgraphRelationBuilder::add_relation(const TimeSourceKey &key_from, const KeyTo &key_to,
                                            eDepsRelation_Type type, const string &description)
{
	BLI_assert(type == DEPSREL_TYPE_TIME);
	TimeSourceDepsNode *time_from = find_node(key_from);
	OperationDepsNode *op_to = get_entry_operation(find_node(key_to));
	if (time_from && op_to) {
		/* TODO(sergey): Store description as well. */
		time_from->add_new_relation(op_to);
	}
}

template <typename KeyType>
void DepsgraphRelationBuilder::add_node_handle_relation(const KeyType &key_from, const DepsNodeHandle *handle,
                                                        eDepsRelation_Type type, const string &description)
{
	OperationDepsNode *op_from = get_exit_operation(find_node(key_from));
	OperationDepsNode *op_to = get_entry_operation(handle->node);
	if (op_from && op_to) {
		add_operation_relation(op_from, op_to, type, description);
	}
	else {
		if (!op_from) {
			/* XXX TODO handle as error or report if needed */
		}
		if (!op_to) {
			/* XXX TODO handle as error or report if needed */
		}
	}
}

template <typename KeyType>
DepsNodeHandle DepsgraphRelationBuilder::create_node_handle(const KeyType &key, const string &default_name)
{
	return DepsNodeHandle(this, find_node(key), default_name);
}


#endif // __DEPSGRAPH_BUILD_H__
