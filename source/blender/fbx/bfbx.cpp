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
 * Contributor(s): Alexander Gessler
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/assimp/bfbx.cpp
 *  \ingroup fbx
 */

#include <cassert>
#include "../assimp/SceneImporter.h"

#include "bfbx.h"

extern "C"
{
#include "BKE_scene.h"
#include "BKE_context.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"


	void bfbx_import_set_defaults(bfbx_import_settings* defaults_out)
	{
		assert(defaults_out);

		bassimp_import_set_defaults(&defaults_out->assimp_settings);

		defaults_out->all_geo_layers = 1;
		defaults_out->drop_dummy_anims = 1;
		defaults_out->preserve_pivot_nodes = 1;
		defaults_out->strict_mode = 0;
	}


	int bfbx_import(bContext *C, const char *filepath, const bfbx_import_settings* settings)
	{
		assert(C);
		assert(filepath);

		bfbx_import_settings defaults;

		if(!settings) {
			bfbx_import_set_defaults(&defaults);
			settings = &defaults;
		}

		bassimp::SceneImporter imp(filepath,*C,settings->assimp_settings);

		Assimp::Importer& ai_imp = imp.get_importer();
		ai_imp.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_STRICT_MODE,settings->strict_mode);

		ai_imp.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS,settings->all_geo_layers);
		ai_imp.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_OPTIMIZE_EMPTY_ANIMATION_CURVES,settings->drop_dummy_anims);
		ai_imp.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS,settings->preserve_pivot_nodes);

		return imp.import() != 0 && imp.apply() != 0;
	}

	/*
    // export to fbx not currently implemented
	int bfbx_export(Scene *sce, const char *filepath, int selected, int apply_modifiers)
	{
		
		return 0;
	} */
}
