/*
 * Copyright 2013, Blender Foundation.
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

#include <string.h> /* XXX needed for missing type declarations in BLI ... */

#include "util_path.h"

extern "C" {
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_ID.h"
#include "DNA_pointcache_types.h"

#include "BKE_appdir.h"
#include "BKE_global.h"
#include "BKE_main.h"
}

namespace PTC {

/* XXX do we want to use BLI C string functions here? just copied from BKE_pointcache for now */

static int ptc_path(char *filename, const char *path, ID *id, bool is_external, bool ignore_libpath)
{
	Library *lib = id ? id->lib : NULL;
	const char *blendfilename= (lib && !ignore_libpath) ? lib->filepath: G.main->name;

	if (path && is_external) {
		strcpy(filename, path);

		if (BLI_path_is_rel(filename)) {
			BLI_path_abs(filename, blendfilename);
		}
	}
	else if (G.relbase_valid || lib) {
		char file[FILE_MAXFILE]; /* we don't want the dir, only the file */

		BLI_split_file_part(blendfilename, file, sizeof(file));
		BLI_replace_extension(file, sizeof(file), ""); /* remove extension */
		BLI_snprintf(filename, FILE_MAX, "//" PTC_DIRECTORY "%s", file); /* add blend file name to pointcache dir */
		BLI_path_abs(filename, blendfilename);
	}
	else {
		/* use the temp path. this is weak but better then not using point cache at all */
		/* temporary directory is assumed to exist and ALWAYS has a trailing slash */
		BLI_snprintf(filename, FILE_MAX, "%s" PTC_DIRECTORY, BKE_tempdir_session());
	}
	
	return BLI_add_slash(filename); /* new strlen() */
}

static int ptc_filename(char *filename, const char *name, int index, const char *path, ID *id,
                        bool do_path, bool do_ext, bool is_external, bool ignore_libpath)
{
	char *newname;
	int len = 0;
	filename[0] = '\0';
	newname = filename;
	
	if (!G.relbase_valid && !is_external)
		return 0; /* save blend file before using disk pointcache */
	
	/* start with temp dir */
	if (do_path) {
		len = ptc_path(filename, path, id, is_external, ignore_libpath);
		newname += len;
	}
	if (name[0] == '\0' && !is_external) {
		const char *idname = (id->name + 2);
		/* convert chars to hex so they are always a valid filename */
		while ('\0' != *idname) {
			BLI_snprintf(newname, FILE_MAX, "%02X", (char)(*idname++));
			newname += 2;
			len += 2;
		}
	}
	else {
		int namelen = (int)strlen(name); 
		strcpy(newname, name); 
		newname += namelen;
		len += namelen;
	}
	
	if (do_ext) {
		if (index < 0 || !is_external) {
			len += BLI_snprintf(newname, FILE_MAX, PTC_EXTENSION);
		}
		else {
			len += BLI_snprintf(newname, FILE_MAX, "_%02u" PTC_EXTENSION, index);
		}
	}
	
	return len;
}

std::string ptc_archive_path(PointCache *cache, ID *id)
{
	char filename[FILE_MAX];
	ptc_filename(filename, cache->name, cache->index, cache->path, id, true, true, cache->flag & PTC_EXTERNAL, cache->flag & PTC_IGNORE_LIBPATH);
	return std::string(filename);
}

} /* namespace PTC */
