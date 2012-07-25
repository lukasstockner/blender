/*
 * Copyright 2011, Blender Foundation.
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
 */

#include "object.h"

#include "mesh.h"
#include "blender_sync.h"
#include "blender_util.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Utilities */


/* Smoke Sync */

/* Only looking for Smoke domains */
// TODO DG: disable rendering of smoke flow??
bool BlenderSync::object_use_smoke(BL::Object b_ob)
{
	BL::Object::modifiers_iterator b_modifiers;
	for(b_ob.modifiers.begin(b_modifiers); b_modifiers != b_ob.modifiers.end(); ++b_modifiers) {
		BL::Modifier mod = (*b_modifiers);

		if (mod.is_a(&RNA_SmokeModifier)) {
			BL::SmokeModifier smd(mod);

			if(smd.smoke_type() == BL::SmokeModifier::smoke_type_DOMAIN) {
				BL::ID key = (BKE_object_is_modified(b_ob))? b_ob: b_ob.data();
				Mesh *mesh = mesh_map.find(key);
				if (mesh) {
					return mesh->need_attribute(scene, ATTR_STD_SMOKE_DENSITY);
				}
			}
		}
	}

	return false;
}

static BL::SmokeModifier *get_smoke(BL::Object b_ob)
{
	BL::Object::modifiers_iterator b_modifiers;
	for(b_ob.modifiers.begin(b_modifiers); b_modifiers != b_ob.modifiers.end(); ++b_modifiers) {
		BL::Modifier mod = (*b_modifiers);

		if (mod.is_a(&RNA_SmokeModifier)) {
			BL::SmokeModifier *smd = (BL::SmokeModifier *)(&mod);

			if(smd->smoke_type() == BL::SmokeModifier::smoke_type_DOMAIN) {
				return smd;
			}
		}
	}
	
	return NULL;
}

void BlenderSync::sync_smoke(Object *ob, BL::Object b_ob)
{
	BL::SmokeModifier *smd = get_smoke(b_ob);
	BL::SmokeDomainSettings sds = smd->domain_settings();

	ob->grid.clear();
	ob->resolution = get_int3(sds.domain_resolution());

	int length[3];
	int numcells = rna_SmokeModifier_density_get_length(&sds.ptr, length);

	if(numcells != 0)
	{
		vector<float> &grid = ob->grid;
		grid.reserve(numcells);
		grid.resize(numcells);
		rna_SmokeModifier_density_get(&sds.ptr, &grid[0]);
	}
}

CCL_NAMESPACE_END
