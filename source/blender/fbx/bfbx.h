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
 * Contributor(s): Alexander Gessler.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file bfbx.h
 *  \ingroup fbx
 */

#ifndef INCLUDED_BFBX_H
#define INCLUDED_BFBX_H

struct bContext;
struct Scene;

#ifdef __cplusplus
extern "C" {
#endif

	/* fbx import settings */
	typedef struct bfbx_import_settings
	{
		/* settings for assimp */
		bassimp_import_settings assimp_settings;

		/* in strict mode, only the 2013 fbx format will be read. In non-strict
		 * mode the importer attempts to make the best out of the data it gets.*/
		int strict_mode;

		/* specifies that the importer will attempt to read all geometry layers
		 * present in the source file. Some fbx files may only contain useful
		 * data in their first geometry channel. */
		int all_geo_layers;

		/* drop dummy/empty animation curves */
		int drop_dummy_anims;

		/* always preserve the pivot points from the input file, even if
		 * this involves creating dummy nodes. */
		int preserve_pivot_nodes;

	} bfbx_import_settings;


	/* obtain default settings for bfbx_import() */
	void bfbx_import_set_defaults(bfbx_import_settings* defaults_out);


	/* import/export functions, settings are optional, bfbx_import_set_defaults()
	 * will be used to get default settings if NULL is specified.
	 * both return 1 on success, 0 on error
	 */
	int bfbx_import(bContext *C, const char *filepath, const bfbx_import_settings* settings);
	//int bassimp_export(Scene *sce, const char *filepath, int selected, int apply_modifiers);
#ifdef __cplusplus
}
#endif

#endif
