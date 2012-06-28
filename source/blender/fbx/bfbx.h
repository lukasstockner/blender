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


	/* import/export functions
	 * both return 1 on success, 0 on error
	 */
	int bfbx_import(bContext *C, const char *filepath);
	//int bassimp_export(Scene *sce, const char *filepath, int selected, int apply_modifiers);
#ifdef __cplusplus
}
#endif

#endif
