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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_file/filelist.c
 *  \ingroup spfile
 */


/* global includes */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include <direct.h>
#endif   
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_fileops_types.h"
#include "BLI_fnmatch.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_fileops_types.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_icons.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BLO_readfile.h"
#include "BKE_idcode.h"

#include "DNA_space_types.h"

#include "ED_datafiles.h"
#include "ED_fileselect.h"
#include "ED_screen.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "PIL_time.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "filelist.h"


/* ----------------- FOLDERLIST (previous/next) -------------- */

typedef struct FolderList {
	struct FolderList *next, *prev;
	char *foldername;
} FolderList;

ListBase *folderlist_new(void)
{
	ListBase *p = MEM_callocN(sizeof(ListBase), "folderlist");
	return p;
}

void folderlist_popdir(struct ListBase *folderlist, char *dir)
{
	const char *prev_dir;
	struct FolderList *folder;
	folder = folderlist->last;

	if (folder) {
		/* remove the current directory */
		MEM_freeN(folder->foldername);
		BLI_freelinkN(folderlist, folder);

		folder = folderlist->last;
		if (folder) {
			prev_dir = folder->foldername;
			BLI_strncpy(dir, prev_dir, FILE_MAXDIR);
		}
	}
	/* delete the folder next or use setdir directly before PREVIOUS OP */
}

void folderlist_pushdir(ListBase *folderlist, const char *dir)
{
	struct FolderList *folder, *previous_folder;
	previous_folder = folderlist->last;

	/* check if already exists */
	if (previous_folder && previous_folder->foldername) {
		if (BLI_path_cmp(previous_folder->foldername, dir) == 0) {
			return;
		}
	}

	/* create next folder element */
	folder = (FolderList *)MEM_mallocN(sizeof(FolderList), "FolderList");
	folder->foldername = BLI_strdup(dir);

	/* add it to the end of the list */
	BLI_addtail(folderlist, folder);
}

const char *folderlist_peeklastdir(ListBase *folderlist)
{
	struct FolderList *folder;

	if (!folderlist->last)
		return NULL;

	folder = folderlist->last;
	return folder->foldername;
}

int folderlist_clear_next(struct SpaceFile *sfile)
{
	struct FolderList *folder;

	/* if there is no folder_next there is nothing we can clear */
	if (!sfile->folders_next)
		return 0;

	/* if previous_folder, next_folder or refresh_folder operators are executed it doesn't clear folder_next */
	folder = sfile->folders_prev->last;
	if ((!folder) || (BLI_path_cmp(folder->foldername, sfile->params->dir) == 0))
		return 0;

	/* eventually clear flist->folders_next */
	return 1;
}

/* not listbase itself */
void folderlist_free(ListBase *folderlist)
{
	if (folderlist) {
		FolderList *folder;
		for (folder = folderlist->first; folder; folder = folder->next)
			MEM_freeN(folder->foldername);
		BLI_freelistN(folderlist);
	}
}

ListBase *folderlist_duplicate(ListBase *folderlist)
{
	
	if (folderlist) {
		ListBase *folderlistn = MEM_callocN(sizeof(ListBase), "copy folderlist");
		FolderList *folder;
		
		BLI_duplicatelist(folderlistn, folderlist);
		
		for (folder = folderlistn->first; folder; folder = folder->next) {
			folder->foldername = MEM_dupallocN(folder->foldername);
		}
		return folderlistn;
	}
	return NULL;
}


/* ------------------FILELIST------------------------ */

struct FileList;

typedef struct FileImage {
	struct FileImage *next, *prev;
	char path[FILE_MAX];
	char relname[FILE_MAX];
	unsigned int flags;
	unsigned int type;
	int index;
	short done;
	ImBuf *img;
	ImBuf *icon;
} FileImage;

typedef struct FileListFilter {
	bool hide_dot;
	bool hide_parent;
	unsigned int filter;
	unsigned int filter_id;
	char filter_glob[64];
	char filter_search[66];  /* + 2 for heading/trailing implicit '*' wildcards. */
} FileListFilter;

typedef struct FileList {
	struct direntry *filelist;
	int numfiles;
	char dir[FILE_MAX];
	short prv_w;
	short prv_h;

	bool force_reset;
	bool force_refresh;
	bool filelist_ready;
	bool filelist_pending;

	short sort;
	bool need_sorting;

	FileListFilter filter_data;
	int *fidx;  /* Also used to detect when we need to filter! */
	int numfiltered;

	bool need_thumbnails;

	bool use_recursion;
	short recursion_level;

	struct BlendHandle *libfiledata;

	/* Set given path as root directory, may change given string in place to a valid value. */
	void (*checkdirf)(struct FileList *, char *);

	/* Fill filelist (to be called by read job). */
	void (*read_jobf)(struct FileList *, const char *, short *, short *, float *, ThreadMutex *);

	/* Filter current filelist. */
	bool (*filterf)(struct direntry *, const char *, FileListFilter *);
} FileList;

#define FILELIST_MAX_RECURSION 3

#define FILENAME_IS_BREADCRUMBS(_n) \
	(((_n)[0] == '.' && (_n)[1] == '\0') || ((_n)[0] == '.' && (_n)[1] == '.' && (_n)[2] == '\0'))

#define SPECIAL_IMG_SIZE 48
#define SPECIAL_IMG_ROWS 4
#define SPECIAL_IMG_COLS 4

#define SPECIAL_IMG_FOLDER 0
#define SPECIAL_IMG_PARENT 1
#define SPECIAL_IMG_REFRESH 2
#define SPECIAL_IMG_BLENDFILE 3
#define SPECIAL_IMG_SOUNDFILE 4
#define SPECIAL_IMG_MOVIEFILE 5
#define SPECIAL_IMG_PYTHONFILE 6
#define SPECIAL_IMG_TEXTFILE 7
#define SPECIAL_IMG_FONTFILE 8
#define SPECIAL_IMG_UNKNOWNFILE 9
#define SPECIAL_IMG_LOADING 10
#define SPECIAL_IMG_BACKUP 11
#define SPECIAL_IMG_MAX SPECIAL_IMG_BACKUP + 1

static ImBuf *gSpecialFileImages[SPECIAL_IMG_MAX];


static void filelist_readjob_main(struct FileList *, const char *, short *, short *, float *, ThreadMutex *);
static void filelist_readjob_lib(struct FileList *, const char *, short *, short *, float *, ThreadMutex *);
static void filelist_readjob_dir(struct FileList *, const char *, short *, short *, float *, ThreadMutex *);

/* helper, could probably go in BKE actually? */
static int groupname_to_code(const char *group);
static unsigned int groupname_to_filter_id(const char *group);

static void filelist_filter_clear(FileList *filelist);

/* ********** Sort helpers ********** */

static bool compare_is_directory(const struct direntry *entry)
{
	/* for library browse .blend files may be treated as directories, but
	 * for sorting purposes they should be considered regular files */
	if (S_ISDIR(entry->type))
		return !(entry->flags & (BLENDERFILE | BLENDERFILE_BACKUP));
	
	return false;
}

static int compare_name(const void *a1, const void *a2)
{
	const struct direntry *entry1 = a1, *entry2 = a2;
	char dir1[FILE_MAX_LIBEXTRA], dir2[FILE_MAX_LIBEXTRA];
	char *name1, *name2;

	/* type is equal to stat.st_mode */

	if (compare_is_directory(entry1)) {
		if (compare_is_directory(entry2) == 0) return (-1);
	}
	else {
		if (compare_is_directory(entry2)) return (1);
	}
	if (S_ISREG(entry1->type)) {
		if (S_ISREG(entry2->type) == 0) return (-1);
	}
	else {
		if (S_ISREG(entry2->type)) return (1);
	}
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);
	
	/* make sure "." and ".." are always first */
	if (strcmp(entry1->relname, ".") == 0) return (-1);
	if (strcmp(entry2->relname, ".") == 0) return (1);
	if (strcmp(entry1->relname, "..") == 0) return (-1);
	if (strcmp(entry2->relname, "..") == 0) return (1);

	name1 = fileentry_uiname(entry1, dir1);
	name2 = fileentry_uiname(entry2, dir2);

	return BLI_natstrcmp(name1, name2);
}

static int compare_date(const void *a1, const void *a2)	
{
	const struct direntry *entry1 = a1, *entry2 = a2;
	char dir1[FILE_MAX_LIBEXTRA], dir2[FILE_MAX_LIBEXTRA];
	char *name1, *name2;

	/* type is equal to stat.st_mode */

	if (compare_is_directory(entry1)) {
		if (compare_is_directory(entry2) == 0) return (-1);
	}
	else {
		if (compare_is_directory(entry2)) return (1);
	}
	if (S_ISREG(entry1->type)) {
		if (S_ISREG(entry2->type) == 0) return (-1);
	}
	else {
		if (S_ISREG(entry2->type)) return (1);
	}
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);

	/* make sure "." and ".." are always first */
	if (strcmp(entry1->relname, ".") == 0) return (-1);
	if (strcmp(entry2->relname, ".") == 0) return (1);
	if (strcmp(entry1->relname, "..") == 0) return (-1);
	if (strcmp(entry2->relname, "..") == 0) return (1);
	
	if (entry1->s.st_mtime < entry2->s.st_mtime) return 1;
	if (entry1->s.st_mtime > entry2->s.st_mtime) return -1;

	name1 = fileentry_uiname(entry1, dir1);
	name2 = fileentry_uiname(entry2, dir2);

	return BLI_natstrcmp(name1, name2);
}

static int compare_size(const void *a1, const void *a2)	
{
	const struct direntry *entry1 = a1, *entry2 = a2;
	char dir1[FILE_MAX_LIBEXTRA], dir2[FILE_MAX_LIBEXTRA];
	char *name1, *name2;

	/* type is equal to stat.st_mode */

	if (compare_is_directory(entry1)) {
		if (compare_is_directory(entry2) == 0) return (-1);
	}
	else {
		if (compare_is_directory(entry2)) return (1);
	}
	if (S_ISREG(entry1->type)) {
		if (S_ISREG(entry2->type) == 0) return (-1);
	}
	else {
		if (S_ISREG(entry2->type)) return (1);
	}
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);

	/* make sure "." and ".." are always first */
	if (strcmp(entry1->relname, ".") == 0) return (-1);
	if (strcmp(entry2->relname, ".") == 0) return (1);
	if (strcmp(entry1->relname, "..") == 0) return (-1);
	if (strcmp(entry2->relname, "..") == 0) return (1);
	
	if (entry1->s.st_size < entry2->s.st_size) return 1;
	if (entry1->s.st_size > entry2->s.st_size) return -1;

	name1 = fileentry_uiname(entry1, dir1);
	name2 = fileentry_uiname(entry2, dir2);

	return BLI_natstrcmp(name1, name2);
}

static int compare_extension(const void *a1, const void *a2)
{
	const struct direntry *entry1 = a1, *entry2 = a2;
	const char *sufix1, *sufix2;
	const char *nil = "";

	if (!(sufix1 = strstr(entry1->relname, ".blend.gz")))
		sufix1 = strrchr(entry1->relname, '.');
	if (!(sufix2 = strstr(entry2->relname, ".blend.gz")))
		sufix2 = strrchr(entry2->relname, '.');
	if (!sufix1) sufix1 = nil;
	if (!sufix2) sufix2 = nil;

	/* type is equal to stat.st_mode */

	if (compare_is_directory(entry1)) {
		if (compare_is_directory(entry2) == 0) return (-1);
	}
	else {
		if (compare_is_directory(entry2)) return (1);
	}

	if (S_ISREG(entry1->type)) {
		if (S_ISREG(entry2->type) == 0) return (-1);
	}
	else {
		if (S_ISREG(entry2->type)) return (1);
	}
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);

	/* make sure "." and ".." are always first */
	if (strcmp(entry1->relname, ".") == 0) return (-1);
	if (strcmp(entry2->relname, ".") == 0) return (1);
	if (strcmp(entry1->relname, "..") == 0) return (-1);
	if (strcmp(entry2->relname, "..") == 0) return (1);

	if ((entry1->flags & BLENDERLIB) && !(entry2->flags & BLENDERLIB)) return -1;
	if (!(entry1->flags & BLENDERLIB) && (entry2->flags & BLENDERLIB)) return 1;
	if ((entry1->flags & BLENDERLIB) && (entry2->flags & BLENDERLIB)) {
		char lib1[FILE_MAX_LIBEXTRA], lib2[FILE_MAX_LIBEXTRA];
		char *group1, *group2, *name1, *name2;
		int grp_comp;

		BLO_library_path_explode(entry1->path, lib1, &group1, &name1);
		BLO_library_path_explode(entry2->path, lib2, &group2, &name2);

		BLI_assert(group1);
		BLI_assert(group2);

		grp_comp = strcmp(group1, group2);
		if (grp_comp != 0 || (!name1 && !name2)) {
			return grp_comp;
		}

		if (!name1) {
			return -1;
		}
		if (!name2) {
			return 1;
		}
		return BLI_strcasecmp(name1, name2);
	}

	return BLI_strcasecmp(sufix1, sufix2);
}

bool filelist_need_sorting(struct FileList *filelist)
{
	return filelist->need_sorting && (filelist->sort != FILE_SORT_NONE);
}

void filelist_sort(struct FileList *filelist)
{
	if (filelist_need_sorting(filelist)) {
		filelist->need_sorting = false;

		switch (filelist->sort) {
			case FILE_SORT_ALPHA:
				qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_name);
				break;
			case FILE_SORT_TIME:
				qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_date);
				break;
			case FILE_SORT_SIZE:
				qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_size);
				break;
			case FILE_SORT_EXTENSION:
				qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_extension);
				break;
			case FILE_SORT_NONE:  /* Should never reach this point! */
			default:
				BLI_assert(0);
				return;
		}

		filelist_filter_clear(filelist);
	}
}

void filelist_setsorting(struct FileList *filelist, const short sort)
{
	if (filelist->sort != sort) {
		filelist->sort = sort;
		filelist->need_sorting = true;
	}
}

/* ********** Filter helpers ********** */

static bool is_hidden_file(const char *filename, FileListFilter *filter)
{
	char *sep = (char *)BLI_last_slash(filename);
	bool is_hidden = false;

	if (filter->hide_dot) {
		if (filename[0] == '.' && filename[1] != '.' && filename[1] != '\0') {
			is_hidden = true; /* ignore .file */
		}
		else {
			int len = strlen(filename);
			if ((len > 0) && (filename[len - 1] == '~')) {
				is_hidden = true;  /* ignore file~ */
			}
		}
	}
	if (!is_hidden && filter->hide_parent) {
		if (filename[0] == '.' && filename[1] == '.' && filename[2] == '\0') {
			is_hidden = true; /* ignore .. */
		}
	}
	if (!is_hidden && ((filename[0] == '.') && (filename[1] == '\0'))) {
		is_hidden = true; /* ignore . */
	}
	/* filename might actually be a piece of path, in which case we have to check all its parts. */
	if (!is_hidden && sep) {
		char tmp_filename[FILE_MAX_LIBEXTRA];

		BLI_strncpy(tmp_filename, filename, sizeof(tmp_filename));
		sep = tmp_filename + (sep - filename);
		while (sep) {
			BLI_assert(sep[1] != '\0');
			if (is_hidden_file(sep + 1, filter)) {
				is_hidden = true;
				break;
			}
			*sep = '\0';
			sep = (char *)BLI_last_slash(tmp_filename);
		}
	}
	return is_hidden;
}

static bool is_filtered_file(struct direntry *file, const char *UNUSED(root), FileListFilter *filter)
{
	bool is_filtered = !is_hidden_file(file->relname, filter);

	if (is_filtered && filter->filter && !FILENAME_IS_BREADCRUMBS(file->relname)) {
		if ((file->type & S_IFDIR) && !(filter->filter & FOLDERFILE)) {
			is_filtered = false;
		}
		if (!(file->type & S_IFDIR) && !(file->flags & filter->filter)) {
			is_filtered = false;
		}
		if (is_filtered && (filter->filter_search[0] != '\0')) {
			if (fnmatch(filter->filter_search, file->relname, FNM_CASEFOLD) != 0) {
				is_filtered = false;
			}
		}
	}

	return is_filtered;
}

static bool is_filtered_lib(struct direntry *file, const char *root, FileListFilter *filter)
{
	bool is_filtered;
	char path[FILE_MAX_LIBEXTRA], dir[FILE_MAXDIR], *group;

	BLI_join_dirfile(path, sizeof(path), root, file->relname);

	if (BLO_library_path_explode(path, dir, &group, NULL)) {
		is_filtered = !is_hidden_file(file->relname, filter);
		if (is_filtered && filter->filter && !FILENAME_IS_BREADCRUMBS(file->relname)) {
			if ((file->type & S_IFDIR) && !(filter->filter & FOLDERFILE)) {
				is_filtered = false;
			}
			if (is_filtered && (filter->filter_search[0] != '\0')) {
				if (fnmatch(filter->filter_search, file->relname, FNM_CASEFOLD) != 0) {
					is_filtered = false;
				}
			}
			if (is_filtered && group) {
				unsigned int filter_id = groupname_to_filter_id(group);
				if (!(filter_id & filter->filter_id)) {
					is_filtered = false;
				}
			}
		}
	}
	else {
		is_filtered = is_filtered_file(file, root, filter);
	}

	return is_filtered;
}

static bool is_filtered_main(struct direntry *file, const char *UNUSED(dir), FileListFilter *filter)
{
	return !is_hidden_file(file->relname, filter);
}

static void filelist_filter_clear(FileList *filelist)
{
	MEM_SAFE_FREE(filelist->fidx);
	filelist->numfiltered = 0;
}

void filelist_filter(FileList *filelist)
{
	int num_filtered = 0;
	int *fidx_tmp;
	int i;

	if (!filelist->filelist) {
		return;
	}

	if (filelist->fidx) {
		/* Assume it has already been filtered, nothing else to do! */
		return;
	}

	fidx_tmp = MEM_mallocN(sizeof(*fidx_tmp) * (size_t)filelist->numfiles, __func__);

	/* Filter remap & count how many files are left after filter in a single loop. */
	for (i = 0; i < filelist->numfiles; ++i) {
		struct direntry *file = &filelist->filelist[i];

		if (filelist->filterf(file, filelist->dir, &filelist->filter_data)) {
			fidx_tmp[num_filtered++] = i;
		}
	}

	/* Note: maybe we could even accept filelist->fidx to be filelist->numfiles -len allocated? */
	filelist->fidx = (int *)MEM_mallocN(sizeof(*filelist->fidx) * (size_t)num_filtered, __func__);
	memcpy(filelist->fidx, fidx_tmp, sizeof(*filelist->fidx) * (size_t)num_filtered);
	filelist->numfiltered = num_filtered;

	MEM_freeN(fidx_tmp);
}

void filelist_setfilter_options(FileList *filelist, const bool hide_dot, const bool hide_parent, const unsigned int filter,
                                const unsigned int filter_id, const char *filter_glob, const char *filter_search)
{
	if ((filelist->filter_data.hide_dot != hide_dot) ||
	    (filelist->filter_data.hide_parent != hide_parent) ||
	    (filelist->filter_data.filter != filter) ||
	    (filelist->filter_data.filter_id != filter_id) ||
	    !STREQ(filelist->filter_data.filter_glob, filter_glob) ||
	    (BLI_strcmp_ignore_pad(filelist->filter_data.filter_search, filter_search, '*') != 0))
	{
		filelist->filter_data.hide_dot = hide_dot;
		filelist->filter_data.hide_parent = hide_parent;

		filelist->filter_data.filter = filter;
		filelist->filter_data.filter_id = filter_id;
		BLI_strncpy(filelist->filter_data.filter_glob, filter_glob, sizeof(filelist->filter_data.filter_glob));
		BLI_strncpy_ensure_pad(filelist->filter_data.filter_search, filter_search, '*',
		                       sizeof(filelist->filter_data.filter_search));

		/* And now, free filtered data so that we now we have to filter again. */
		filelist_filter_clear(filelist);
	}
}

/* ********** Icon/image helpers ********** */

void filelist_init_icons(void)
{
	short x, y, k;
	ImBuf *bbuf;
	ImBuf *ibuf;

	BLI_assert(G.background == false);

#ifdef WITH_HEADLESS
	bbuf = NULL;
#else
	bbuf = IMB_ibImageFromMemory((unsigned char *)datatoc_prvicons_png, datatoc_prvicons_png_size, IB_rect, NULL, "<splash>");
#endif
	if (bbuf) {
		for (y = 0; y < SPECIAL_IMG_ROWS; y++) {
			for (x = 0; x < SPECIAL_IMG_COLS; x++) {
				int tile = SPECIAL_IMG_COLS * y + x;
				if (tile < SPECIAL_IMG_MAX) {
					ibuf = IMB_allocImBuf(SPECIAL_IMG_SIZE, SPECIAL_IMG_SIZE, 32, IB_rect);
					for (k = 0; k < SPECIAL_IMG_SIZE; k++) {
						memcpy(&ibuf->rect[k * SPECIAL_IMG_SIZE], &bbuf->rect[(k + y * SPECIAL_IMG_SIZE) * SPECIAL_IMG_SIZE * SPECIAL_IMG_COLS + x * SPECIAL_IMG_SIZE], SPECIAL_IMG_SIZE * sizeof(int));
					}
					gSpecialFileImages[tile] = ibuf;
				}
			}
		}
		IMB_freeImBuf(bbuf);
	}
}

void filelist_free_icons(void)
{
	int i;

	BLI_assert(G.background == false);

	for (i = 0; i < SPECIAL_IMG_MAX; ++i) {
		IMB_freeImBuf(gSpecialFileImages[i]);
		gSpecialFileImages[i] = NULL;
	}
}

void filelist_imgsize(struct FileList *filelist, short w, short h)
{
	filelist->prv_w = w;
	filelist->prv_h = h;
}

static struct direntry *filelist_geticon_get_file(struct FileList *filelist, const int index)
{
	BLI_assert(G.background == false);

	return filelist_file(filelist, index);
}

ImBuf *filelist_getimage(struct FileList *filelist, const int index)
{
	struct direntry *file = filelist_geticon_get_file(filelist, index);

	return file->image;
}

static ImBuf *filelist_geticon_image_ex(const unsigned int type, const unsigned int flags, const char *relname)
{
	ImBuf *ibuf = NULL;

	if (type & S_IFDIR) {
		if (strcmp(relname, "..") == 0) {
			ibuf = gSpecialFileImages[SPECIAL_IMG_PARENT];
		}
		else if (strcmp(relname, ".") == 0) {
			ibuf = gSpecialFileImages[SPECIAL_IMG_REFRESH];
		}
		else {
			ibuf = gSpecialFileImages[SPECIAL_IMG_FOLDER];
		}
	}
	else if (flags & (BLENDERFILE)) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_BLENDFILE];
	}
	else if (flags & (BLENDERLIB)) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_UNKNOWNFILE];
	}
	else if (flags & (MOVIEFILE | MOVIEFILE_ICON)) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_MOVIEFILE];
	}
	else if (flags & SOUNDFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_SOUNDFILE];
	}
	else if (flags & PYSCRIPTFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_PYTHONFILE];
	}
	else if (flags & FTFONTFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_FONTFILE];
	}
	else if (flags & TEXTFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_TEXTFILE];
	}
	else if (flags & IMAGEFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_LOADING];
	}
	else if (flags & BLENDERFILE_BACKUP) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_BACKUP];
	}
	else {
		ibuf = gSpecialFileImages[SPECIAL_IMG_UNKNOWNFILE];
	}

	return ibuf;
}

ImBuf *filelist_geticon_image(struct FileList *filelist, const int index)
{
	struct direntry *file = filelist_geticon_get_file(filelist, index);

	return filelist_geticon_image_ex(file->type, file->flags, file->relname);
}

static int filelist_geticon_ex(const unsigned int type, const unsigned int flags, const char *path, const char *relname, const bool ignore_libdir)
{
	if (type & S_IFDIR && !(ignore_libdir && (flags & (BLENDERLIB | BLENDERFILE)))) {
		if (strcmp(relname, "..") == 0) {
			return ICON_FILE_PARENT;
		}
		if (flags & APPLICATIONBUNDLE) {
			return ICON_UGLYPACKAGE;
		}
		if (flags & BLENDERFILE) {
			return ICON_FILE_BLEND;
		}
		return ICON_FILE_FOLDER;
	}
	else if (flags & BLENDERFILE)
		return ICON_FILE_BLEND;
	else if (flags & BLENDERFILE_BACKUP)
		return ICON_FILE_BACKUP;
	else if (flags & IMAGEFILE)
		return ICON_FILE_IMAGE;
	else if (flags & MOVIEFILE)
		return ICON_FILE_MOVIE;
	else if (flags & PYSCRIPTFILE)
		return ICON_FILE_SCRIPT;
	else if (flags & SOUNDFILE)
		return ICON_FILE_SOUND;
	else if (flags & FTFONTFILE)
		return ICON_FILE_FONT;
	else if (flags & BTXFILE)
		return ICON_FILE_BLANK;
	else if (flags & COLLADAFILE)
		return ICON_FILE_BLANK;
	else if (flags & TEXTFILE)
		return ICON_FILE_TEXT;
	else if (flags & BLENDERLIB) {
		char lib[FILE_MAXDIR], *group;

		if (BLO_library_path_explode(path, lib, &group, NULL) && group) {
			int idcode = groupname_to_code(group);

			/* TODO: this should most likely be completed and moved to UI_interface_icons.h ? unless it already exists somewhere... */
			switch (idcode) {
				case ID_AC:
					return ICON_ANIM_DATA;
				case ID_AR:
					return ICON_ARMATURE_DATA;
				case ID_BR:
					return ICON_BRUSH_DATA;
				case ID_CA:
					return ICON_CAMERA_DATA;
				case ID_CU:
					return ICON_CURVE_DATA;
				case ID_GD:
					return ICON_GREASEPENCIL;
				case ID_GR:
					return ICON_GROUP;
				case ID_IM:
					return ICON_IMAGE_DATA;
				case ID_LA:
					return ICON_LAMP_DATA;
				case ID_LS:
					return ICON_LINE_DATA;
				case ID_LT:
					return ICON_LATTICE_DATA;
				case ID_MA:
					return ICON_MATERIAL_DATA;
				case ID_MB:
					return ICON_META_DATA;
				case ID_MC:
					return ICON_CLIP;
				case ID_ME:
					return ICON_MESH_DATA;
				case ID_MSK:
					return ICON_MOD_MASK;  /* TODO! this would need its own icon! */
				case ID_NT:
					return ICON_NODETREE;
				case ID_OB:
					return ICON_OBJECT_DATA;
				case ID_PAL:
					return ICON_COLOR;  /* TODO! this would need its own icon! */
				case ID_PC:
					return ICON_CURVE_BEZCURVE;  /* TODO! this would need its own icon! */
				case ID_SCE:
					return ICON_SCENE_DATA;
				case ID_SPK:
					return ICON_SPEAKER;
				case ID_SO:
					return ICON_SOUND;
				case ID_TE:
					return ICON_TEXTURE_DATA;
				case ID_TXT:
					return ICON_TEXT;
				case ID_VF:
					return ICON_FONT_DATA;
				case ID_WO:
					return ICON_WORLD_DATA;
			}
		}
	}
	return ICON_FILE_BLANK;
}

int filelist_geticon(struct FileList *filelist, const int index)
{
	struct direntry *file = filelist_geticon_get_file(filelist, index);

	return filelist_geticon_ex(file->type, file->flags, file->path, file->relname, false);
}

/* ********** Main ********** */

static void filelist_checkdir_dir(struct FileList *UNUSED(filelist), char *r_dir)
{
	BLI_make_exist(r_dir);
}

static void filelist_checkdir_lib(struct FileList *UNUSED(filelist), char *r_dir)
{
	char dir[FILE_MAXDIR];
	if (!BLO_library_path_explode(r_dir, dir, NULL, NULL)) {
		/* if not a valid library, we need it to be a valid directory! */
		BLI_make_exist(r_dir);
	}
}

static void filelist_checkdir_main(struct FileList *filelist, char *r_dir)
{
	/* TODO */
	filelist_checkdir_lib(filelist, r_dir);
}

FileList *filelist_new(short type)
{
	FileList *p = MEM_callocN(sizeof(FileList), "filelist");

	switch (type) {
		case FILE_MAIN:
			p->checkdirf = filelist_checkdir_main;
			p->read_jobf = filelist_readjob_main;
			p->filterf = is_filtered_main;
			break;
		case FILE_LOADLIB:
			p->checkdirf = filelist_checkdir_lib;
			p->read_jobf = filelist_readjob_lib;
			p->filterf = is_filtered_lib;
			break;
		default:
			p->checkdirf = filelist_checkdir_dir;
			p->read_jobf = filelist_readjob_dir;
			p->filterf = is_filtered_file;
			break;
	}
	return p;
}

void filelist_clear(struct FileList *filelist)
{
	if (!filelist) {
		return;
	}

	if (filelist->fidx) {
		MEM_freeN(filelist->fidx);
		filelist->fidx = NULL;
	}

	BLI_filelist_free(filelist->filelist, filelist->numfiles, NULL);
	filelist->numfiles = 0;
	filelist->filelist = NULL;
	filelist->numfiltered = 0;
}

void filelist_free(struct FileList *filelist)
{
	if (!filelist) {
		printf("Attempting to delete empty filelist.\n");
		return;
	}
	
	MEM_SAFE_FREE(filelist->fidx);
	filelist->numfiltered = 0;
	memset(&filelist->filter_data, 0, sizeof(filelist->filter_data));

	filelist->need_sorting = false;
	filelist->sort = FILE_SORT_NONE;

	filelist->need_thumbnails = false;

	BLI_filelist_free(filelist->filelist, filelist->numfiles, NULL);
	filelist->numfiles = 0;
	filelist->filelist = NULL;
}

void filelist_freelib(struct FileList *filelist)
{
	if (filelist->libfiledata)
		BLO_blendhandle_close(filelist->libfiledata);
	filelist->libfiledata = NULL;
}

BlendHandle *filelist_lib(struct FileList *filelist)
{
	return filelist->libfiledata;
}

int filelist_numfiles(struct FileList *filelist)
{
	return filelist->numfiltered;
}

char *fileentry_uiname(const struct direntry *entry, char *dir)
{
	char *name;

	if (entry->path && entry->flags & BLENDERLIB) {
		char *group;
		BLO_library_path_explode(entry->path, dir, &group, &name);
		if (!name) {
			name = group;
		}
	}
	else if (entry->type & S_IFDIR) {
		name = entry->relname;
	}
	else {
		name = (char *)BLI_path_basename(entry->relname);
	}
	BLI_assert(name);

	return name;
}

const char *filelist_dir(struct FileList *filelist)
{
	return filelist->dir;
}

/**
 * May modifies in place given r_dir, which is expected to be FILE_MAX_LIBEXTRA length.
 */
void filelist_setdir(struct FileList *filelist, char *r_dir)
{
#ifndef NDEBUG
	size_t len = strlen(r_dir);
	BLI_assert((len < FILE_MAX_LIBEXTRA - 1) || r_dir[len - 1] == '/');
#endif

	BLI_cleanup_dir(G.main->name, r_dir);
	BLI_add_slash(r_dir);
	filelist->checkdirf(filelist, r_dir);

	if (!STREQ(filelist->dir, r_dir)) {
		BLI_strncpy(filelist->dir, r_dir, sizeof(filelist->dir));
		filelist->force_reset = true;
	}
}

void filelist_setrecursive(struct FileList *filelist, const bool use_recursion)
{
	if (filelist->use_recursion != use_recursion) {
		filelist->use_recursion = use_recursion;
		filelist->force_reset = true;
	}
}

bool filelist_force_reset(struct FileList *filelist)
{
	return filelist->force_reset;
}

bool filelist_is_ready(struct FileList *filelist)
{
	return filelist->filelist_ready;
}

bool filelist_pending(struct FileList *filelist)
{
	return filelist->filelist_pending;
}

bool filelist_need_refresh(struct FileList *filelist)
{
	return (!filelist->filelist || !filelist->fidx || filelist->force_reset || filelist->force_refresh || filelist->need_sorting);
}

void filelist_clear_refresh(struct FileList *filelist)
{
	filelist->force_refresh = false;
}

struct direntry *filelist_file(struct FileList *filelist, int index)
{
	int fidx = 0;
	
	if ((index < 0) || (index >= filelist->numfiltered)) {
		return NULL;
	}
	fidx = filelist->fidx[index];

	return &filelist->filelist[fidx];
}

int filelist_find(struct FileList *filelist, const char *filename)
{
	int index = -1;
	int i;
	int fidx = -1;
	
	if (!filelist->fidx) 
		return fidx;

	
	for (i = 0; i < filelist->numfiles; ++i) {
		if (strcmp(filelist->filelist[i].relname, filename) == 0) {  /* not dealing with user input so don't need BLI_path_cmp */
			index = i;
			break;
		}
	}

	for (i = 0; i < filelist->numfiltered; ++i) {
		if (filelist->fidx[i] == index) {
			fidx = i;
			break;
		}
	}
	return fidx;
}

/* would recognize .blend as well */
static bool file_is_blend_backup(const char *str)
{
	const size_t a = strlen(str);
	size_t b = 7;
	bool retval = 0;

	if (a == 0 || b >= a) {
		/* pass */
	}
	else {
		const char *loc;
		
		if (a > b + 1)
			b++;
		
		/* allow .blend1 .blend2 .blend32 */
		loc = BLI_strcasestr(str + a - b, ".blend");
		
		if (loc)
			retval = 1;
	}
	
	return (retval);
}

static int path_extension_type(const char *path)
{
	if (BLO_has_bfile_extension(path)) {
		return BLENDERFILE;
	}
	else if (file_is_blend_backup(path)) {
		return BLENDERFILE_BACKUP;
	}
	else if (BLI_testextensie(path, ".app")) {
		return APPLICATIONBUNDLE;
	}
	else if (BLI_testextensie(path, ".py")) {
		return PYSCRIPTFILE;
	}
	else if (BLI_testextensie_n(path, ".txt", ".glsl", ".osl", ".data", NULL)) {
		return TEXTFILE;
	}
	else if (BLI_testextensie_n(path, ".ttf", ".ttc", ".pfb", ".otf", ".otc", NULL)) {
		return FTFONTFILE;
	}
	else if (BLI_testextensie(path, ".btx")) {
		return BTXFILE;
	}
	else if (BLI_testextensie(path, ".dae")) {
		return COLLADAFILE;
	}
	else if (BLI_testextensie_array(path, imb_ext_image) ||
	         (G.have_quicktime && BLI_testextensie_array(path, imb_ext_image_qt)))
	{
		return IMAGEFILE;
	}
	else if (BLI_testextensie(path, ".ogg")) {
		if (IMB_isanim(path)) {
			return MOVIEFILE;
		}
		else {
			return SOUNDFILE;
		}
	}
	else if (BLI_testextensie_array(path, imb_ext_movie)) {
		return MOVIEFILE;
	}
	else if (BLI_testextensie_array(path, imb_ext_audio)) {
		return SOUNDFILE;
	}
	return 0;
}

static int file_extension_type(const char *dir, const char *relname)
{
	char path[FILE_MAX];
	BLI_join_dirfile(path, sizeof(path), dir, relname);
	return path_extension_type(path);
}

int ED_file_extension_icon(const char *path)
{
	int type = path_extension_type(path);
	
	if (type == BLENDERFILE)
		return ICON_FILE_BLEND;
	else if (type == BLENDERFILE_BACKUP)
		return ICON_FILE_BACKUP;
	else if (type == IMAGEFILE)
		return ICON_FILE_IMAGE;
	else if (type == MOVIEFILE)
		return ICON_FILE_MOVIE;
	else if (type == PYSCRIPTFILE)
		return ICON_FILE_SCRIPT;
	else if (type == SOUNDFILE)
		return ICON_FILE_SOUND;
	else if (type == FTFONTFILE)
		return ICON_FILE_FONT;
	else if (type == BTXFILE)
		return ICON_FILE_BLANK;
	else if (type == COLLADAFILE)
		return ICON_FILE_BLANK;
	else if (type == TEXTFILE)
		return ICON_FILE_TEXT;
	
	return ICON_FILE_BLANK;
}

static void filelist_setfiletypes(const char *root, struct direntry *files, const int numfiles, const char *filter_glob)
{
	struct direntry *file;
	int num;

	for (num = 0, file = files; num < numfiles; num++, file++) {
		if (file->flags & BLENDERLIB) {
			continue;
		}
		file->type = file->s.st_mode;  /* restore the mess below */
#ifndef __APPLE__
		/* Don't check extensions for directories, allow in OSX cause bundles have extensions*/
		if (file->type & S_IFDIR) {
			continue;
		}
#endif
		file->flags = file_extension_type(root, file->relname);

		if (filter_glob[0] && BLI_testextensie_glob(file->relname, filter_glob)) {
			file->flags = OPERATORFILE;
		}
	}
}

int filelist_empty(struct FileList *filelist)
{
	return filelist->filelist == NULL;
}

void filelist_select_file(struct FileList *filelist, int index, FileSelType select, unsigned int flag, FileCheckType check)
{
	struct direntry *file = filelist_file(filelist, index);
	if (file != NULL) {
		int check_ok = 0; 
		switch (check) {
			case CHECK_DIRS:
				check_ok = S_ISDIR(file->type);
				break;
			case CHECK_ALL:
				check_ok = 1;
				break;
			case CHECK_FILES:
			default:
				check_ok = !S_ISDIR(file->type);
				break;
		}
		if (check_ok) {
			switch (select) {
				case FILE_SEL_REMOVE:
					file->selflag &= ~flag;
					break;
				case FILE_SEL_ADD:
					file->selflag |= flag;
					break;
				case FILE_SEL_TOGGLE:
					file->selflag ^= flag;
					break;
			}
		}
	}
}

void filelist_select(struct FileList *filelist, FileSelection *sel, FileSelType select, unsigned int flag, FileCheckType check)
{
	/* select all valid files between first and last indicated */
	if ((sel->first >= 0) && (sel->first < filelist->numfiltered) && (sel->last >= 0) && (sel->last < filelist->numfiltered)) {
		int current_file;
		for (current_file = sel->first; current_file <= sel->last; current_file++) {
			filelist_select_file(filelist, current_file, select, flag, check);
		}
	}
}

bool filelist_is_selected(struct FileList *filelist, int index, FileCheckType check)
{
	struct direntry *file = filelist_file(filelist, index);
	if (!file) {
		return 0;
	}
	switch (check) {
		case CHECK_DIRS:
			return S_ISDIR(file->type) && (file->selflag & SELECTED_FILE);
		case CHECK_FILES:
			return S_ISREG(file->type) && (file->selflag & SELECTED_FILE);
		case CHECK_ALL:
		default:
			return (file->selflag & SELECTED_FILE) != 0;
	}
}


bool filelist_islibrary(struct FileList *filelist, char *dir, char **group)
{
	return BLO_library_path_explode(filelist->dir, dir, group, NULL);
}

static int groupname_to_code(const char *group)
{
	char buf[BLO_GROUP_MAX];
	char *lslash;

	BLI_assert(group);

	BLI_strncpy(buf, group, sizeof(buf));
	lslash = (char *)BLI_last_slash(buf);
	if (lslash)
		lslash[0] = '\0';

	return buf[0] ? BKE_idcode_from_name(buf) : 0;
}

static unsigned int groupname_to_filter_id(const char *group)
{
	int id_code = groupname_to_code(group);

	switch (id_code) {
		case ID_AC:
			return FILTER_ID_AC;
		case ID_AR:
			return FILTER_ID_AR;
		case ID_BR:
			return FILTER_ID_BR;
		case ID_CA:
			return FILTER_ID_CA;
		case ID_CU:
			return FILTER_ID_CU;
		case ID_GD:
			return FILTER_ID_GD;
		case ID_GR:
			return FILTER_ID_GR;
		case ID_IM:
			return FILTER_ID_IM;
		case ID_LA:
			return FILTER_ID_LA;
		case ID_LS:
			return FILTER_ID_LS;
		case ID_LT:
			return FILTER_ID_LT;
		case ID_MA:
			return FILTER_ID_MA;
		case ID_MB:
			return FILTER_ID_MB;
		case ID_MC:
			return FILTER_ID_MC;
		case ID_ME:
			return FILTER_ID_ME;
		case ID_MSK:
			return FILTER_ID_MSK;
		case ID_NT:
			return FILTER_ID_NT;
		case ID_OB:
			return FILTER_ID_OB;
		case ID_PAL:
			return FILTER_ID_PAL;
		case ID_PC:
			return FILTER_ID_PC;
		case ID_SCE:
			return FILTER_ID_SCE;
		case ID_SPK:
			return FILTER_ID_SPK;
		case ID_SO:
			return FILTER_ID_SO;
		case ID_TE:
			return FILTER_ID_TE;
		case ID_TXT:
			return FILTER_ID_TXT;
		case ID_VF:
			return FILTER_ID_VF;
		case ID_WO:
			return FILTER_ID_WO;
		default:
			return 0;
	}
}

/*
 * From here, we are in 'Job Context', i.e. have to be careful about sharing stuff between bacground working thread
 * and main one (used by UI among other things).
 */

/* This helper is highly specialized for our needs, it 'transfers' most data (strings/pointers) from subfiles to
 * filelist_buff. Only dirname->relname is actually duplicated.
 */
static void filelist_readjob_merge_sublist(
        struct direntry **filelist_buff, int *filelist_buff_size, int *filelist_used_size,
        const char *root, const char *subdir, struct direntry *subfiles, const int num_subfiles,
        const bool ignore_breadcrumbs)
{
	if (num_subfiles) {
		struct direntry *f;
		int new_numfiles = num_subfiles + *filelist_used_size;
		char dir[FILE_MAX];
		int i, j;

		if (new_numfiles > *filelist_buff_size) {
			struct direntry *new_filelist;

			*filelist_buff_size = new_numfiles * 2;
			new_filelist = malloc(sizeof(*new_filelist) * (size_t)*filelist_buff_size);
			if (*filelist_buff && *filelist_used_size) {
				memcpy(new_filelist, *filelist_buff, sizeof(*new_filelist) * (size_t)*filelist_used_size);
				free(*filelist_buff);
			}
			*filelist_buff = new_filelist;
		}
		for (i = *filelist_used_size, j = 0, f = subfiles; j < num_subfiles; j++, f++) {
			if (ignore_breadcrumbs && FILENAME_IS_BREADCRUMBS(f->relname)) {
				/* Ignore 'inner' breadcrumbs! */
				new_numfiles--;
				continue;
			}
			BLI_join_dirfile(dir, sizeof(dir), subdir, f->relname);
			BLI_cleanup_file(root, dir);
			BLI_path_rel(dir, root);
			(*filelist_buff)[i] = *f;
			(*filelist_buff)[i].relname = BLI_strdup(dir + 2);  /* + 2 to remove '//' added by BLI_path_rel */
			/* those pointers are given to new_filelist... */
			f->path = NULL;
			f->poin = NULL;
			f->image = NULL;
			i++;
		}

		*filelist_used_size = new_numfiles;
	}
}

static void filelist_readjob_list_lib(const char *root, struct direntry **files, int *num_files)
{
	LinkNode *l, *names, *previews;
	struct ImBuf *ima;
	int ok, i, nprevs, nnames, idcode = 0;
	char dir[FILE_MAX], *group;

	struct BlendHandle *libfiledata = NULL;

	/* name test */
	ok = BLO_library_path_explode(root, dir, &group, NULL);
	if (!ok) {
		return;
	}

	/* there we go */
	libfiledata = BLO_blendhandle_from_file(dir, NULL);
	if (libfiledata == NULL) {
		return;
	}

	/* memory for strings is passed into filelist[i].relname and freed in freefilelist */
	if (group) {
		idcode = groupname_to_code(group);
		previews = BLO_blendhandle_get_previews(libfiledata, idcode, &nprevs);
		names = BLO_blendhandle_get_datablock_names(libfiledata, idcode, &nnames);
	}
	else {
		previews = NULL;
		nprevs = 0;
		names = BLO_blendhandle_get_linkable_groups(libfiledata);
		nnames = BLI_linklist_length(names);
	}

	BLO_blendhandle_close(libfiledata);

	*num_files = nnames + 1;
	*files = malloc(*num_files * sizeof(**files));
	memset(*files, 0, *num_files * sizeof(**files));

	(*files)[nnames].relname = BLI_strdup("..");
	(*files)[nnames].type |= S_IFDIR;
	(*files)[nnames].flags |= BLENDERLIB;

	for (i = 0, l = names; i < nnames; i++, l = l->next) {
		const char *blockname = l->link;
		struct direntry *file = &(*files)[i];

		file->relname = BLI_strdup(blockname);
		file->path = BLI_strdupcat(root, blockname);
		file->flags |= BLENDERLIB;
		if (group && idcode) {
			file->type |= S_IFREG;
		}
		else {
			file->type |= S_IFDIR;
		}
	}

	if (previews && (nnames != nprevs)) {
		printf("filelist_from_library: error, found %d items, %d previews\n", nnames, nprevs);
	}
	else if (previews) {
		for (i = 0, l = previews; i < nnames; i++, l = l->next) {
			PreviewImage *img = l->link;
			struct direntry *file = &(*files)[i];

			if (img) {
				unsigned int w = img->w[ICON_SIZE_PREVIEW];
				unsigned int h = img->h[ICON_SIZE_PREVIEW];
				unsigned int *rect = img->rect[ICON_SIZE_PREVIEW];

				/* first allocate imbuf for copying preview into it */
				if (w > 0 && h > 0 && rect) {
					ima = IMB_allocImBuf(w, h, 32, IB_rect);
					memcpy(ima->rect, rect, w * h * sizeof(unsigned int));
					file->image = ima;
				}
			}
		}
	}

	BLI_linklist_free(names, free);
	if (previews) {
		BLI_linklist_free(previews, BKE_previewimg_freefunc);
	}
}

#if 0
/* Kept for reference here, in case we want to add back that feature later. We do not need it currently. */
/* Code ***NOT*** updated for job stuff! */
static void filelist_readjob_main_rec(struct FileList *filelist)
{
	ID *id;
	struct direntry *files, *firstlib = NULL;
	ListBase *lb;
	int a, fake, idcode, ok, totlib, totbl;
	
	// filelist->type = FILE_MAIN; // XXXXX TODO: add modes to filebrowser

	if (filelist->dir[0] == '/') filelist->dir[0] = 0;
	
	if (filelist->dir[0]) {
		idcode = groupname_to_code(filelist->dir);
		if (idcode == 0) filelist->dir[0] = 0;
	}
	
	if (filelist->dir[0] == 0) {
		
		/* make directories */
#ifdef WITH_FREESTYLE
		filelist->numfiles = 24;
#else
		filelist->numfiles = 23;
#endif
		filelist->filelist = (struct direntry *)malloc(filelist->numfiles * sizeof(struct direntry));
		
		for (a = 0; a < filelist->numfiles; a++) {
			memset(&(filelist->filelist[a]), 0, sizeof(struct direntry));
			filelist->filelist[a].type |= S_IFDIR;
		}
		
		filelist->filelist[0].relname = BLI_strdup("..");
		filelist->filelist[1].relname = BLI_strdup("Scene");
		filelist->filelist[2].relname = BLI_strdup("Object");
		filelist->filelist[3].relname = BLI_strdup("Mesh");
		filelist->filelist[4].relname = BLI_strdup("Curve");
		filelist->filelist[5].relname = BLI_strdup("Metaball");
		filelist->filelist[6].relname = BLI_strdup("Material");
		filelist->filelist[7].relname = BLI_strdup("Texture");
		filelist->filelist[8].relname = BLI_strdup("Image");
		filelist->filelist[9].relname = BLI_strdup("Ika");
		filelist->filelist[10].relname = BLI_strdup("Wave");
		filelist->filelist[11].relname = BLI_strdup("Lattice");
		filelist->filelist[12].relname = BLI_strdup("Lamp");
		filelist->filelist[13].relname = BLI_strdup("Camera");
		filelist->filelist[14].relname = BLI_strdup("Ipo");
		filelist->filelist[15].relname = BLI_strdup("World");
		filelist->filelist[16].relname = BLI_strdup("Screen");
		filelist->filelist[17].relname = BLI_strdup("VFont");
		filelist->filelist[18].relname = BLI_strdup("Text");
		filelist->filelist[19].relname = BLI_strdup("Armature");
		filelist->filelist[20].relname = BLI_strdup("Action");
		filelist->filelist[21].relname = BLI_strdup("NodeTree");
		filelist->filelist[22].relname = BLI_strdup("Speaker");
#ifdef WITH_FREESTYLE
		filelist->filelist[23].relname = BLI_strdup("FreestyleLineStyle");
#endif
	}
	else {

		/* make files */
		idcode = groupname_to_code(filelist->dir);
		
		lb = which_libbase(G.main, idcode);
		if (lb == NULL) return;
		
		id = lb->first;
		filelist->numfiles = 0;
		while (id) {
			if (!filelist->filter_data.hide_dot || id->name[2] != '.') {
				filelist->numfiles++;
			}
			
			id = id->next;
		}
		
		/* XXXXX TODO: if databrowse F4 or append/link filelist->hide_parent has to be set */
		if (!filelist->filter_data.hide_parent) filelist->numfiles += 1;
		filelist->filelist = filelist->numfiles > 0 ? (struct direntry *)malloc(filelist->numfiles * sizeof(struct direntry)) : NULL;

		files = filelist->filelist;
		
		if (!filelist->filter_data.hide_parent) {
			memset(&(filelist->filelist[0]), 0, sizeof(struct direntry));
			filelist->filelist[0].relname = BLI_strdup("..");
			filelist->filelist[0].type |= S_IFDIR;
		
			files++;
		}
		
		id = lb->first;
		totlib = totbl = 0;
		
		while (id) {
			ok = 1;
			if (ok) {
				if (!filelist->filter_data.hide_dot || id->name[2] != '.') {
					memset(files, 0, sizeof(struct direntry));
					if (id->lib == NULL) {
						files->relname = BLI_strdup(id->name + 2);
					}
					else {
						files->relname = MEM_mallocN(FILE_MAX + (MAX_ID_NAME - 2),     "filename for lib");
						BLI_snprintf(files->relname, FILE_MAX + (MAX_ID_NAME - 2) + 3, "%s | %s", id->lib->name, id->name + 2);
					}
					files->type |= S_IFREG;
#if 0               /* XXXXX TODO show the selection status of the objects */
					if (!filelist->has_func) { /* F4 DATA BROWSE */
						if (idcode == ID_OB) {
							if ( ((Object *)id)->flag & SELECT) files->selflag |= SELECTED_FILE;
						}
						else if (idcode == ID_SCE) {
							if ( ((Scene *)id)->r.scemode & R_BG_RENDER) files->selflag |= SELECTED_FILE;
						}
					}
#endif
					files->nr = totbl + 1;
					files->poin = id;
					fake = id->flag & LIB_FAKEUSER;
					if (idcode == ID_MA || idcode == ID_TE || idcode == ID_LA || idcode == ID_WO || idcode == ID_IM) {
						files->flags |= IMAGEFILE;
					}
					if      (id->lib && fake) BLI_snprintf(files->extra, sizeof(files->extra), "LF %d",    id->us);
					else if (id->lib)         BLI_snprintf(files->extra, sizeof(files->extra), "L    %d",  id->us);
					else if (fake)            BLI_snprintf(files->extra, sizeof(files->extra), "F    %d",  id->us);
					else                      BLI_snprintf(files->extra, sizeof(files->extra), "      %d", id->us);
					
					if (id->lib) {
						if (totlib == 0) firstlib = files;
						totlib++;
					}
					
					files++;
				}
				totbl++;
			}
			
			id = id->next;
		}
		
		/* only qsort of library blocks */
		if (totlib > 1) {
			qsort(firstlib, totlib, sizeof(struct direntry), compare_name);
		}
	}
}
#endif

static void filelist_readjob_dir_lib_rec(
        const bool do_lib, const char *main_name,
        FileList *filelist, int *filelist_buffsize, const char *dir, const char *filter_glob, const int recursion_level,
        short *stop, short *do_update, float *progress, int *done_files, ThreadMutex *lock)
{
	/* only used if recursing, will contain all non-immediate children then. */
	struct direntry *file, *files = NULL;
	bool is_lib = do_lib;
	int num_files = 0;
	int i;

	if (!filelist) {
		return;
	}

	if (do_lib) {
		filelist_readjob_list_lib(dir, &files, &num_files);

		if (!files) {
			is_lib = false;
			num_files = BLI_filelist_dir_contents(dir, &files);
		}
	}
	else {
		num_files = BLI_filelist_dir_contents(dir, &files);
	}

	if (!files) {
		return;
	}

	/* We only set filtypes for our own level, sub ones will be set by subcalls. */
	filelist_setfiletypes(dir, files, num_files, filter_glob);

	if (do_lib) {
		/* Promote blend files from mere file status to prestigious directory one! */
		for (i = 0, file = files; i < num_files; i++, file++) {
			if (BLO_has_bfile_extension(file->relname)) {
				char name[FILE_MAX];

				BLI_join_dirfile(name, sizeof(name), dir, file->relname);

				/* prevent current file being used as acceptable dir */
				if (BLI_path_cmp(main_name, name) != 0) {
					file->type &= ~S_IFMT;
					file->type |= S_IFDIR;
				}
			}
		}
	}

	BLI_mutex_lock(lock);

	filelist_readjob_merge_sublist(&filelist->filelist, filelist_buffsize, &filelist->numfiles, filelist->dir,
	                               dir, files, num_files, recursion_level != 0);

	(*done_files)++;
	*progress = (float)(*done_files) / filelist->numfiles;

	//~ printf("%f (%d / %d)\n", *progress, *done_files, filelist->numfiles);

	BLI_mutex_unlock(lock);

	*do_update = true;

	/* in case it's a lib we don't care anymore about max recursion level... */
	if (!*stop && filelist->use_recursion && ((do_lib && is_lib) || (recursion_level < FILELIST_MAX_RECURSION))) {
		for (i = 0, file = files; i < num_files && !*stop; i++, file++) {
			char subdir[FILE_MAX];

			if (FILENAME_IS_BREADCRUMBS(file->relname)) {
				/* do not increase done_files here, we completly ignore those. */
				continue;
			}
			else if ((file->type & S_IFDIR) == 0) {
				(*done_files)++;
				continue;
			}

			BLI_join_dirfile(subdir, sizeof(subdir), dir, file->relname);
			BLI_cleanup_dir(main_name, subdir);
			filelist_readjob_dir_lib_rec(do_lib, main_name,
			                             filelist, filelist_buffsize, subdir, filter_glob, recursion_level + 1,
			                             stop, do_update, progress, done_files, lock);
		}
	}
	BLI_filelist_free(files, num_files, NULL);
}

static void filelist_readjob_dir(
        FileList *filelist, const char *main_name, short *stop, short *do_update, float *progress, ThreadMutex *lock)
{
	char dir[FILE_MAX];
	char filter_glob[64];  /* TODO should be define! */
	int filelist_buffsize = 0;
	int done_files = 0;

	BLI_assert(filelist->fidx == NULL);
	BLI_assert(filelist->filelist == NULL);

	BLI_mutex_lock(lock);

	BLI_strncpy(dir, filelist->dir, sizeof(dir));
	BLI_strncpy(filter_glob, filelist->filter_data.filter_glob, sizeof(filter_glob));

	BLI_mutex_unlock(lock);

	filelist_readjob_dir_lib_rec(false, main_name, filelist, &filelist_buffsize, dir, filter_glob, 0,
	                             stop, do_update, progress, &done_files, lock);
}

static void filelist_readjob_lib(
        FileList *filelist, const char *main_name, short *stop, short *do_update, float *progress, ThreadMutex *lock)
{
	char dir[FILE_MAX];
	char filter_glob[64];  /* TODO should be define! */
	int filelist_buffsize = 0;
	int done_files = 0;

	BLI_assert(filelist->fidx == NULL);
	BLI_assert(filelist->filelist == NULL);

	BLI_mutex_lock(lock);

	BLI_strncpy(dir, filelist->dir, sizeof(dir));
	BLI_strncpy(filter_glob, filelist->filter_data.filter_glob, sizeof(filter_glob));

	BLI_mutex_unlock(lock);

	BLI_cleanup_dir(main_name, dir);

	filelist_readjob_dir_lib_rec(true, main_name, filelist, &filelist_buffsize, dir, filter_glob, 0,
	                             stop, do_update, progress, &done_files, lock);
}

static void filelist_readjob_main(
        FileList *filelist, const char *main_name, short *stop, short *do_update, float *progress, ThreadMutex *lock)
{
	BLI_mutex_lock(lock);

	/* TODO! */
	filelist_readjob_dir(filelist, main_name, stop, do_update, progress, lock);

	BLI_mutex_unlock(lock);
}


typedef struct FileListReadJob {
	ThreadMutex lock;
	char main_name[FILE_MAX];
	struct FileList *filelist;
	struct FileList *tmp_filelist;
	//~ ReportList reports;
} FileListReadJob;

static void filelist_readjob_startjob(void *flrjv, short *stop, short *do_update, float *progress)
{
	FileListReadJob *flrj = flrjv;

	printf("START filelist reading (%d files, main thread: %d)\n", flrj->filelist->numfiles, BLI_thread_is_main());

	BLI_mutex_lock(&flrj->lock);

	BLI_assert((flrj->tmp_filelist == NULL) && flrj->filelist);

	flrj->tmp_filelist = MEM_dupallocN(flrj->filelist);

	BLI_mutex_unlock(&flrj->lock);

	flrj->tmp_filelist->filelist = NULL;
	flrj->tmp_filelist->fidx = NULL;
	flrj->tmp_filelist->numfiles = 0;
	flrj->tmp_filelist->fidx = 0;
	flrj->tmp_filelist->libfiledata = NULL;

	flrj->tmp_filelist->read_jobf(flrj->tmp_filelist, flrj->main_name, stop, do_update, progress, &flrj->lock);

	printf("END filelist reading (%d files, STOPPED: %d, DO_UPDATE: %d)\n", flrj->filelist->numfiles, *stop, *do_update);
}

static void filelist_readjob_update(void *flrjv)
{
	FileListReadJob *flrj = flrjv;
	struct direntry *new_entries = NULL;
	int num_new_entries = 0;

	BLI_mutex_lock(&flrj->lock);

	if (flrj->tmp_filelist->numfiles != flrj->filelist->numfiles) {
		num_new_entries = flrj->tmp_filelist->numfiles;
		/* This way we are sure we won't share any mem with background job! */
		/* Note direntry->poin is not handled here, should not matter though currently. */
		BLI_filelist_duplicate(&new_entries, flrj->tmp_filelist->filelist, num_new_entries, NULL);
	}

	BLI_mutex_unlock(&flrj->lock);

	if (new_entries) {
		if (flrj->filelist->filelist) {
			BLI_filelist_free(flrj->filelist->filelist, flrj->filelist->numfiles, NULL);
		}
		flrj->filelist->filelist = new_entries;
		flrj->filelist->numfiles = num_new_entries;
		if (flrj->filelist->fidx) {
			MEM_freeN(flrj->filelist->fidx);
			flrj->filelist->fidx = NULL;
			flrj->filelist->numfiltered = 0;
		}

		flrj->filelist->need_sorting = true;
		flrj->filelist->force_refresh = true;
		/* Better be explicit here, since we overwrite filelist->filelist on each run of this update func,
		 * it would be stupid to start thumbnail job! */
		flrj->filelist->need_thumbnails = false;
	}
}

static void filelist_readjob_endjob(void *flrjv)
{
	FileListReadJob *flrj = flrjv;

	flrj->filelist->filelist_pending = false;
	flrj->filelist->filelist_ready = true;
	/* Now we can update thumbnails! */
	flrj->filelist->need_thumbnails = true;
}

static void filelist_readjob_free(void *flrjv)
{
	FileListReadJob *flrj = flrjv;

	if (flrj->tmp_filelist) {
		/* tmp_filelist shall never ever be filtered! */
		BLI_assert(flrj->tmp_filelist->fidx == NULL);

		filelist_freelib(flrj->tmp_filelist);
		filelist_free(flrj->tmp_filelist);
		MEM_freeN(flrj->tmp_filelist);
	}

	BLI_mutex_end(&flrj->lock);

	MEM_freeN(flrj);
}

void filelist_readjob_start(FileList *filelist, const bContext *C)
{
	wmJob *wm_job;
	FileListReadJob *flrj;

	/* prepare job data */
	flrj = MEM_callocN(sizeof(FileListReadJob), __func__);
	flrj->filelist = filelist;
	BLI_strncpy(flrj->main_name, G.main->name, sizeof(flrj->main_name));

	filelist->force_reset = false;
	filelist->filelist_ready = false;
	filelist->filelist_pending = true;

	BLI_mutex_init(&flrj->lock);

	//~ BKE_reports_init(&tj->reports, RPT_PRINT);

	/* setup job */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), CTX_wm_area(C), "Listing Dirs...",
	                     WM_JOB_PROGRESS, WM_JOB_TYPE_FILESEL_READDIR);
	WM_jobs_customdata_set(wm_job, flrj, filelist_readjob_free);
	WM_jobs_timer(wm_job, 0.01, NC_SPACE | ND_SPACE_FILE_LIST, NC_SPACE | ND_SPACE_FILE_LIST);
	WM_jobs_callbacks(wm_job, filelist_readjob_startjob, NULL, filelist_readjob_update, filelist_readjob_endjob);

	/* start the job */
	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

void filelist_readjob_stop(wmWindowManager *wm, FileList *filelist)
{
	WM_jobs_kill_type(wm, filelist, WM_JOB_TYPE_FILESEL_READDIR);
}

int filelist_readjob_running(wmWindowManager *wm, FileList *filelist)
{
	return WM_jobs_test(wm, filelist, WM_JOB_TYPE_FILESEL_READDIR);
}

/* ********** Thumbnails job ********** */

typedef struct ThumbnailJob {
	ListBase loadimages;
	ImBuf *static_icons_buffers[BIFICONID_LAST];
	const short *stop;
	const short *do_update;
	struct FileList *filelist;
	ReportList reports;
} ThumbnailJob;

bool filelist_need_thumbnails(FileList *filelist)
{
	return filelist->need_thumbnails;
}

static void thumbnail_joblist_free(ThumbnailJob *tj)
{
	FileImage *limg = tj->loadimages.first;
	
	/* free the images not yet copied to the filelist -> these will get freed with the filelist */
	for (; limg; limg = limg->next) {
		if ((limg->img) && (!limg->done)) {
			IMB_freeImBuf(limg->img);
		}
		if (limg->icon) {
			IMB_freeImBuf(limg->icon);
		}
	}
	BLI_freelistN(&tj->loadimages);
}

static void thumbnails_startjob(void *tjv, short *stop, short *do_update, float *UNUSED(progress))
{
	ThumbnailJob *tj = tjv;
	FileImage *limg = tj->loadimages.first;

	tj->stop = stop;
	tj->do_update = do_update;

	while ((*stop == 0) && (limg)) {
		if (limg->flags & IMAGEFILE) {
			limg->img = IMB_thumb_manage(limg->path, THB_NORMAL, THB_SOURCE_IMAGE);
		}
		else if (limg->flags & (BLENDERFILE | BLENDERFILE_BACKUP)) {
			limg->img = IMB_thumb_manage(limg->path, THB_NORMAL, THB_SOURCE_BLEND);
		}
		else if (limg->flags & BLENDERLIB) {
			if (!limg->img) {
				limg->img = IMB_dupImBuf(filelist_geticon_image_ex(limg->type, limg->flags, limg->relname));
			}
			if (limg->img && limg->icon) {
				IMB_rectblend(limg->img, limg->img, limg->icon, NULL, NULL, NULL, 0.0f,
				              limg->img->x - limg->icon->x, limg->img->y - limg->icon->y, 0, 0, 0, 0,
				              limg->icon->x, limg->icon->y, IMB_BLEND_MIX, false);
			}
		}
		else if (limg->flags & MOVIEFILE) {
			limg->img = IMB_thumb_manage(limg->path, THB_NORMAL, THB_SOURCE_MOVIE);
			if (!limg->img) {
				/* remember that file can't be loaded via IMB_open_anim */
				limg->flags &= ~MOVIEFILE;
				limg->flags |= MOVIEFILE_ICON;
			}
		}
		*do_update = true;
		PIL_sleep_ms(10);
		limg = limg->next;
	}
}

static void thumbnails_update(void *tjv)
{
	ThumbnailJob *tj = tjv;

	if (tj->filelist && tj->filelist->filelist) {
		FileImage *limg = tj->loadimages.first;
		while (limg) {
			if (!limg->done && limg->img) {
				tj->filelist->filelist[limg->index].image = limg->img;
				/* update flag for movie files where thumbnail can't be created */
				if (limg->flags & MOVIEFILE_ICON) {
					tj->filelist->filelist[limg->index].flags &= ~MOVIEFILE;
					tj->filelist->filelist[limg->index].flags |= MOVIEFILE_ICON;
				}
				limg->done = true;
			}
			limg = limg->next;
		}
	}
}

static void thumbnails_endjob(void *tjv)
{
	ThumbnailJob *tj = tjv;

	if (!*tj->stop) {
		tj->filelist->need_thumbnails = false;
	}
}

static void thumbnails_free(void *tjv)
{
	ThumbnailJob *tj = tjv;
	thumbnail_joblist_free(tj);
	MEM_freeN(tj);
}


void thumbnails_start(FileList *filelist, const bContext *C)
{
	wmJob *wm_job;
	ThumbnailJob *tj;
	int idx;

	/* prepare job data */
	tj = MEM_callocN(sizeof(ThumbnailJob), "thumbnails\n");
	tj->filelist = filelist;
	for (idx = 0; idx < filelist->numfiles; idx++) {
		if (!filelist->filelist[idx].path) {
			continue;
		}
		/* for blenlib items we overlay the ID type's icon... */
		if (!filelist->filelist[idx].image || (filelist->filelist[idx].flags & BLENDERLIB)) {
			if ((filelist->filelist[idx].flags & (IMAGEFILE | MOVIEFILE | BLENDERFILE | BLENDERFILE_BACKUP | BLENDERLIB))) {
				FileImage *limg = MEM_callocN(sizeof(FileImage), "loadimage");
				BLI_strncpy(limg->path, filelist->filelist[idx].path, sizeof(limg->path));
				BLI_strncpy(limg->relname, filelist->filelist[idx].relname, sizeof(limg->relname));
				if (filelist->filelist[idx].image) {
					limg->img = IMB_dupImBuf(filelist->filelist[idx].image);
				}
				limg->index = idx;
				limg->flags = filelist->filelist[idx].flags;
				limg->type = filelist->filelist[idx].type;
				if (filelist->filelist[idx].flags & BLENDERLIB) {
					/* XXX We have to do this here, this is not threadsafe. */
					int icon_id = filelist_geticon_ex(limg->type, limg->flags, limg->path, limg->relname, true);

					/* We cache static icons! */
					if (icon_id < BIFICONID_LAST) {
						if (!tj->static_icons_buffers[icon_id]) {
							tj->static_icons_buffers[icon_id] = UI_icon_to_imbuf(icon_id);
						}
						else {
							/* increment refcount! */
							IMB_refImBuf(tj->static_icons_buffers[icon_id]);
						}
						limg->icon = tj->static_icons_buffers[icon_id];
					}
					else {
						limg->icon = UI_icon_to_imbuf(icon_id);
					}
				}
				else {
					limg->icon = NULL;
				}
				BLI_addtail(&tj->loadimages, limg);
			}
		}
	}

	BKE_reports_init(&tj->reports, RPT_PRINT);

	/* setup job */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), filelist, "Thumbnails",
	                     0, WM_JOB_TYPE_FILESEL_THUMBNAIL);
	WM_jobs_customdata_set(wm_job, tj, thumbnails_free);
	WM_jobs_timer(wm_job, 0.5, NC_WINDOW, NC_WINDOW);
	WM_jobs_callbacks(wm_job, thumbnails_startjob, NULL, thumbnails_update, thumbnails_endjob);

	/* start the job */
	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

void thumbnails_stop(wmWindowManager *wm, FileList *filelist)
{
	WM_jobs_kill_type(wm, filelist, WM_JOB_TYPE_FILESEL_THUMBNAIL);
}

int thumbnails_running(wmWindowManager *wm, FileList *filelist)
{
	return WM_jobs_test(wm, filelist, WM_JOB_TYPE_FILESEL_THUMBNAIL);
}
