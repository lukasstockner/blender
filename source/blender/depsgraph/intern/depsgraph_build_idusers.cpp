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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Methods for adding the links between datablocks to the depsgraph
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

/* ******************************************** */
/**
 * ID User Builder
 *
 * This builder creates links between ID datablocks to
 * say that datablock B "uses" datablock A.
 *
 * \note This ordering is the *opposite* of the way
 * we typically think of hierarchical relationships.
 * For example, "ob.data" becomes "obdata -> object"
 */

DepsgraphIDUsersBuilder::DepsgraphIDUsersBuilder(Depsgraph *graph) :
    m_graph(graph)
{
}


void DepsgraphIDUsersBuilder::add_relation(const ID *from_id, const ID *to_id,
                                           eDepsRelation_Type type, const char *description)
{
	IDDepsNode *node_from = m_graph->find_id_node(from_id);
	IDDepsNode *node_to = m_graph->find_id_node(to_id);

	if (node_from && node_to) {
		m_graph->add_new_relation(node_from, node_to, type, description);
	}
	else {
		fprintf(stderr, "ID Builder add_relation(%s => %s, %s => %s, %d, %s) Failed\n",
		        (from_id) ? from_id->name : "<No ID>",
		        (node_from) ? node_from->identifier().c_str() : "<None>",
		        (to_id) ? to_id->name : "<No ID>",
		        (node_to)   ? node_to->identifier().c_str() : "<None>",
		        type, description);
	}
}

void DepsgraphIDUsersBuilder::build_scene(Main *UNUSED(bmain), Scene *scene)
{
	/* scene set - do links to other scenes */
	if (scene->set) {
		// XXX: how?
	}

	/* scene objects */
	for (Base *base = (Base *)scene->base.first; base; base = base->next) {
		Object *ob = base->object;
		build_object(scene, ob);
	}

	/* world */
	if (scene->world) {
		//build_world(scene->world);
	}

	/* compo nodes */
	if (scene->nodetree) {
		//build_compositor(scene);
	}

	// XXX: scene's other data
}

void DepsgraphIDUsersBuilder::build_object(Scene *scene, Object *ob)
{
	ID *ob_id = &ob->id;

	/* object -> scene */
	add_relation(ob_id, &scene->id, DEPSREL_TYPE_DATABLOCK, "Scene Object");

	/* object animation */
	//build_animdata(&ob->id);

	/* object datablock */
	if (ob->data) {
		ID *obdata_id = (ID *)ob->data;

		/* obdata -> object */
		add_relation(obdata_id, ob_id, DEPSREL_TYPE_DATABLOCK, "Object Data");

		/* ob data animation */
		//build_animdata(obdata_id);

		/* type-specific data... */
		switch (ob->type) {
			case OB_MESH:     /* Geometry */
			case OB_CURVE:
			case OB_FONT:
			case OB_SURF:
			case OB_MBALL:
			case OB_LATTICE:
			{
				/* obdata -> geometry (i.e. shapekeys) */
				//build_obdata_geom(scene, ob);

				/* materials */

				/* modifiers */
			}
			break;


			case OB_ARMATURE: /* Pose */
				/* rig -> custom shapes? */
				//build_rig(scene, ob);
				break;

			case OB_LAMP:   /* Lamp */
				//build_lamp(ob);
				break;

			case OB_CAMERA: /* Camera */
				//build_camera(ob);
				break;
		}
	}

	/* proxy */

	/* dupligroups? */
}
