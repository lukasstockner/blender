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
#  include <unistd.h>
#else
#  include <io.h>
#  include <direct.h>
#endif   
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_fileops_types.h"
#include "BLI_fnmatch.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_stack.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_fileops_types.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "RNA_types.h"

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_icons.h"
#include "BKE_idcode.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BLO_readfile.h"

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
	ListBase *p = MEM_callocN(sizeof(*p), __func__);
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
	folder = MEM_mallocN(sizeof(*folder), __func__);
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
		ListBase *folderlistn = MEM_callocN(sizeof(*folderlistn), __func__);
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

typedef struct FileListIntern {
	/* XXX This will be reworked to keep 'all entries' storage to a minimum memory space! */
	ListBase entries;
	FileDirEntry **filtered;
} FileListIntern;

#define FILELIST_ENTRYCACHESIZE 1024  /* Keep it a power of two! */
typedef struct FileListEntryCache {
	/* Block cache: all entries between start and end index. used for part of the list on diplay. */
	FileDirEntry *block_entries[FILELIST_ENTRYCACHESIZE];
	int block_start_index, block_end_index, block_center_index, block_cursor;

	/* Misc cache: random indices, FIFO behavior.
	 * Note: Not 100% sure we actually need that, time will say. */
	int misc_cursor;
	int misc_entries_indices[FILELIST_ENTRYCACHESIZE];
	GHash *misc_entries;

	/* Previews handling. */
	TaskPool *previews_pool;
	ThreadQueue *previews_todo;
	ThreadQueue *previews_done;
} FileListEntryCache;

typedef struct FileListEntryPreview {
	char path[FILE_MAX];
	unsigned int flags;
	int index;
	ImBuf *img;
} FileListEntryPreview;

typedef struct FileListFilter {
	bool hide_dot;
	bool hide_parent;
	bool hide_lib_dir;
	unsigned int filter;
	unsigned int filter_id;
	char filter_glob[64];
	char filter_search[66];  /* + 2 for heading/trailing implicit '*' wildcards. */
} FileListFilter;

typedef struct FileList {
	FileDirEntryArr filelist;

	AssetEngine *ae;

	short prv_w;
	short prv_h;

	bool force_reset;
	bool force_refresh;
	bool filelist_ready;
	bool filelist_pending;

	short sort;
	bool need_sorting;

	FileListFilter filter_data;
	bool need_filtering;

	struct FileListIntern filelist_intern;

	struct FileListEntryCache filelist_cache;

	short max_recursion;
	short recursion_level;

	struct BlendHandle *libfiledata;

	/* Set given path as root directory, may change given string in place to a valid value. */
	void (*checkdirf)(struct FileList *, char *);

	/* Fill filelist (to be called by read job). */
	void (*read_jobf)(struct FileList *, const char *, short *, short *, float *, ThreadMutex *);

	/* Filter an entry of current filelist. */
	bool (*filterf)(struct FileDirEntry *, const char *, FileListFilter *);
} FileList;

#define SPECIAL_IMG_SIZE 48
#define SPECIAL_IMG_ROWS 4
#define SPECIAL_IMG_COLS 4

enum {
	SPECIAL_IMG_FOLDER      = 0,
	SPECIAL_IMG_PARENT      = 1,
	SPECIAL_IMG_REFRESH     = 2,
	SPECIAL_IMG_BLENDFILE   = 3,
	SPECIAL_IMG_SOUNDFILE   = 4,
	SPECIAL_IMG_MOVIEFILE   = 5,
	SPECIAL_IMG_PYTHONFILE  = 6,
	SPECIAL_IMG_TEXTFILE    = 7,
	SPECIAL_IMG_FONTFILE    = 8,
	SPECIAL_IMG_UNKNOWNFILE = 9,
	SPECIAL_IMG_LOADING     = 10,
	SPECIAL_IMG_BACKUP      = 11,
	SPECIAL_IMG_MAX
};

static ImBuf *gSpecialFileImages[SPECIAL_IMG_MAX];


static void filelist_readjob_main(struct FileList *, const char *, short *, short *, float *, ThreadMutex *);
static void filelist_readjob_lib(struct FileList *, const char *, short *, short *, float *, ThreadMutex *);
static void filelist_readjob_dir(struct FileList *, const char *, short *, short *, float *, ThreadMutex *);

/* helper, could probably go in BKE actually? */
static int groupname_to_code(const char *group);
static unsigned int groupname_to_filter_id(const char *group);

static void filelist_filter_clear(FileList *filelist);
static void filelist_cache_clear(FileListEntryCache *cache);

/* ********** Sort helpers ********** */

static int compare_direntry_generic(const FileDirEntry *entry1, const FileDirEntry *entry2)
{
	/* type is equal to stat.st_mode */

	if (entry1->typeflag & FILE_TYPE_DIR) {
	    if (entry2->typeflag & FILE_TYPE_DIR) {
			/* If both entries are tagged as dirs, we make a 'sub filter' that shows first the real dirs,
			 * then libs (.blend files), then categories in libs. */
			if (entry1->typeflag & FILE_TYPE_BLENDERLIB) {
				if (!(entry2->typeflag & FILE_TYPE_BLENDERLIB)) {
					return 1;
				}
			}
			else if (entry2->typeflag & FILE_TYPE_BLENDERLIB) {
				return -1;
			}
			else if (entry1->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
				if (!(entry2->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP))) {
					return 1;
				}
			}
			else if (entry2->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
				return -1;
			}
		}
		else {
			return -1;
		}
	}
	else if (entry2->typeflag & FILE_TYPE_DIR) {
	    return 1;
	}

	/* We get rid of this, this is OS-specific description of file types, not really useful at our level! */
#if 0
	if (S_ISREG(entry1->entry->type)) {
		if (!S_ISREG(entry2->entry->type)) {
			return -1;
		}
	}
	else if (S_ISREG(entry2->entry->type)) {
		return 1;
	}
	if ((entry1->entry->type & S_IFMT) < (entry2->entry->type & S_IFMT)) return -1;
	if ((entry1->entry->type & S_IFMT) > (entry2->entry->type & S_IFMT)) return 1;
#endif

	/* make sure "." and ".." are always first */
	if (FILENAME_IS_CURRENT(entry1->relpath)) return -1;
	if (FILENAME_IS_CURRENT(entry2->relpath)) return 1;
	if (FILENAME_IS_PARENT(entry1->relpath)) return -1;
	if (FILENAME_IS_PARENT(entry2->relpath)) return 1;
	
	return 0;
}

static int compare_name(void *UNUSED(user_data), const void *a1, const void *a2)
{
	const FileDirEntry *entry1 = a1;
	const FileDirEntry *entry2 = a2;
	char *name1, *name2;
	int ret;

	if ((ret = compare_direntry_generic(entry1, entry2))) {
		return ret;
	}

	name1 = entry1->name;
	name2 = entry2->name;

	return BLI_natstrcmp(name1, name2);
}

static int compare_date(void *UNUSED(user_data), const void *a1, const void *a2)
{
	const FileDirEntry *entry1 = a1;
	const FileDirEntry *entry2 = a2;
	char *name1, *name2;
	int ret;

	if ((ret = compare_direntry_generic(entry1, entry2))) {
		return ret;
	}
	
	if (entry1->entry->time < entry2->entry->time) return 1;
	if (entry1->entry->time > entry2->entry->time) return -1;

	name1 = entry1->name;
	name2 = entry2->name;

	return BLI_natstrcmp(name1, name2);
}

static int compare_size(void *UNUSED(user_data), const void *a1, const void *a2)
{
	const FileDirEntry *entry1 = a1;
	const FileDirEntry *entry2 = a2;
	char *name1, *name2;
	int ret;

	if ((ret = compare_direntry_generic(entry1, entry2))) {
		return ret;
	}
	
	if (entry1->entry->size < entry2->entry->size) return 1;
	if (entry1->entry->size > entry2->entry->size) return -1;

	name1 = entry1->name;
	name2 = entry2->name;

	return BLI_natstrcmp(name1, name2);
}

static int compare_extension(void *UNUSED(user_data), const void *a1, const void *a2)
{
	const FileDirEntry *entry1 = a1;
	const FileDirEntry *entry2 = a2;
	char *name1, *name2;
	int ret;

	if ((ret = compare_direntry_generic(entry1, entry2))) {
		return ret;
	}

	if ((entry1->typeflag & FILE_TYPE_BLENDERLIB) && !(entry2->typeflag & FILE_TYPE_BLENDERLIB)) return -1;
	if (!(entry1->typeflag & FILE_TYPE_BLENDERLIB) && (entry2->typeflag & FILE_TYPE_BLENDERLIB)) return 1;
	if ((entry1->typeflag & FILE_TYPE_BLENDERLIB) && (entry2->typeflag & FILE_TYPE_BLENDERLIB)) {
		if ((entry1->typeflag & FILE_TYPE_DIR) && !(entry2->typeflag & FILE_TYPE_DIR)) return 1;
		if (!(entry1->typeflag & FILE_TYPE_DIR) && (entry2->typeflag & FILE_TYPE_DIR)) return -1;
		if (entry1->blentype < entry2->blentype) return -1;
		if (entry1->blentype > entry2->blentype) return 1;
	}
	else {
		const char *sufix1, *sufix2;

		if (!(sufix1 = strstr(entry1->relpath, ".blend.gz")))
			sufix1 = strrchr(entry1->relpath, '.');
		if (!(sufix2 = strstr(entry2->relpath, ".blend.gz")))
			sufix2 = strrchr(entry2->relpath, '.');
		if (!sufix1) sufix1 = "";
		if (!sufix2) sufix2 = "";

		if ((ret = BLI_strcasecmp(sufix1, sufix2))) {
			return ret;
		}
	}

	name1 = entry1->name;
	name2 = entry2->name;

	return BLI_natstrcmp(name1, name2);
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
				BLI_listbase_sort_r(&filelist->filelist_intern.entries, NULL, compare_name);
				break;
			case FILE_SORT_TIME:
				BLI_listbase_sort_r(&filelist->filelist_intern.entries, NULL, compare_date);
				break;
			case FILE_SORT_SIZE:
				BLI_listbase_sort_r(&filelist->filelist_intern.entries, NULL, compare_size);
				break;
			case FILE_SORT_EXTENSION:
				BLI_listbase_sort_r(&filelist->filelist_intern.entries, NULL, compare_extension);
				break;
			case FILE_SORT_NONE:  /* Should never reach this point! */
			default:
				BLI_assert(0);
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

static bool is_filtered_file(FileDirEntry *file, const char *UNUSED(root), FileListFilter *filter)
{
	bool is_filtered = !is_hidden_file(file->relpath, filter);

	if (is_filtered && filter->filter && !FILENAME_IS_CURRPAR(file->relpath)) {
		if (file->typeflag & FILE_TYPE_DIR) {
			if (file->typeflag & (FILE_TYPE_BLENDERLIB | FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
				if (!(filter->filter & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP))) {
					is_filtered = false;
				}
			}
			else {
				if (!(filter->filter & FILE_TYPE_FOLDER)) {
					is_filtered = false;
				}
			}
		}
		else {
			if (!(file->typeflag & filter->filter)) {
				is_filtered = false;
			}
		}
		if (is_filtered && (filter->filter_search[0] != '\0')) {
			if (fnmatch(filter->filter_search, file->relpath, FNM_CASEFOLD) != 0) {
				is_filtered = false;
			}
		}
	}

	return is_filtered;
}

static bool is_filtered_lib(FileDirEntry *file, const char *root, FileListFilter *filter)
{
	bool is_filtered;
	char path[FILE_MAX_LIBEXTRA], dir[FILE_MAXDIR], *group, *name;

	BLI_join_dirfile(path, sizeof(path), root, file->relpath);

	if (BLO_library_path_explode(path, dir, &group, &name)) {
		is_filtered = !is_hidden_file(file->relpath, filter);
		if (is_filtered && filter->filter && !FILENAME_IS_CURRPAR(file->relpath)) {
			if (file->typeflag & FILE_TYPE_DIR) {
				if (file->typeflag & (FILE_TYPE_BLENDERLIB | FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
					if (!(filter->filter & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP))) {
						is_filtered = false;
					}
				}
				else {
					if (!(filter->filter & FILE_TYPE_FOLDER)) {
						is_filtered = false;
					}
				}
			}
			if (is_filtered && group) {
				if (!name && filter->hide_lib_dir) {
					is_filtered = false;
				}
				else {
					unsigned int filter_id = groupname_to_filter_id(group);
					if (!(filter_id & filter->filter_id)) {
						is_filtered = false;
					}
				}
			}
			if (is_filtered && (filter->filter_search[0] != '\0')) {
				if (fnmatch(filter->filter_search, file->relpath, FNM_CASEFOLD) != 0) {
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

static bool is_filtered_main(FileDirEntry *file, const char *UNUSED(dir), FileListFilter *filter)
{
	return !is_hidden_file(file->relpath, filter);
}

static void filelist_filter_clear(FileList *filelist)
{
	filelist->need_filtering = true;
}

void filelist_filter(FileList *filelist)
{
	int num_filtered = 0;
	const int num_files = filelist->filelist.nbr_entries;
	FileDirEntry **filtered_tmp, *file;

	if (filelist->filelist.nbr_entries == 0) {
		return;
	}

	if (!filelist->need_filtering) {
		/* Assume it has already been filtered, nothing else to do! */
		return;
	}

	filelist->filter_data.hide_lib_dir = false;
	if (filelist->max_recursion) {
		/* Never show lib ID 'categories' directories when we are in 'flat' mode, unless
		 * root path is a blend file. */
		char dir[FILE_MAXDIR];
		if (!filelist_islibrary(filelist, dir, NULL)) {
			filelist->filter_data.hide_lib_dir = true;
		}
	}

	filtered_tmp = MEM_mallocN(sizeof(*filtered_tmp) * (size_t)num_files, __func__);

	/* Filter remap & count how many files are left after filter in a single loop. */
	for (file = filelist->filelist_intern.entries.first; file; file = file->next) {
		if (filelist->filterf(file, filelist->filelist.root, &filelist->filter_data)) {
			filtered_tmp[num_filtered++] = file;
		}
	}

	if (filelist->filelist_intern.filtered) {
		MEM_freeN(filelist->filelist_intern.filtered);
	}
	filelist->filelist_intern.filtered = MEM_mallocN(sizeof(*filelist->filelist_intern.filtered) * (size_t)num_filtered,
	                                                 __func__);
	memcpy(filelist->filelist_intern.filtered, filtered_tmp,
	       sizeof(*filelist->filelist_intern.filtered) * (size_t)num_filtered);
	filelist->filelist.nbr_entries_filtered = num_filtered;
//	printf("Filetered: %d over %d entries\n", num_filtered, filelist->filelist.nbr_entries);

	filelist_cache_clear(&filelist->filelist_cache);
	filelist->need_filtering = false;

	MEM_freeN(filtered_tmp);
}

void filelist_setfilter_options(FileList *filelist, const bool hide_dot, const bool hide_parent,
                                const unsigned int filter, const unsigned int filter_id,
                                const char *filter_glob, const char *filter_search)
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

static FileDirEntry *filelist_geticon_get_file(struct FileList *filelist, const int index)
{
	BLI_assert(G.background == false);

	return filelist_file(filelist, index);
}

ImBuf *filelist_getimage(struct FileList *filelist, const int index)
{
	FileDirEntry *file = filelist_geticon_get_file(filelist, index);

	return file->image;
}

static ImBuf *filelist_geticon_image_ex(const unsigned int typeflag, const char *relpath)
{
	ImBuf *ibuf = NULL;

	if (typeflag & FILE_TYPE_DIR) {
		if (FILENAME_IS_PARENT(relpath)) {
			ibuf = gSpecialFileImages[SPECIAL_IMG_PARENT];
		}
		else if (FILENAME_IS_CURRENT(relpath)) {
			ibuf = gSpecialFileImages[SPECIAL_IMG_REFRESH];
		}
		else {
			ibuf = gSpecialFileImages[SPECIAL_IMG_FOLDER];
		}
	}
	else if (typeflag & FILE_TYPE_BLENDER) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_BLENDFILE];
	}
	else if (typeflag & FILE_TYPE_BLENDERLIB) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_UNKNOWNFILE];
	}
	else if (typeflag & (FILE_TYPE_MOVIE | FILE_TYPE_MOVIE_ICON)) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_MOVIEFILE];
	}
	else if (typeflag & FILE_TYPE_SOUND) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_SOUNDFILE];
	}
	else if (typeflag & FILE_TYPE_PYSCRIPT) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_PYTHONFILE];
	}
	else if (typeflag & FILE_TYPE_FTFONT) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_FONTFILE];
	}
	else if (typeflag & FILE_TYPE_TEXT) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_TEXTFILE];
	}
	else if (typeflag & FILE_TYPE_IMAGE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_LOADING];
	}
	else if (typeflag & FILE_TYPE_BLENDER_BACKUP) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_BACKUP];
	}
	else {
		ibuf = gSpecialFileImages[SPECIAL_IMG_UNKNOWNFILE];
	}

	return ibuf;
}

ImBuf *filelist_geticon_image(struct FileList *filelist, const int index)
{
	FileDirEntry *file = filelist_geticon_get_file(filelist, index);

	return filelist_geticon_image_ex(file->typeflag, file->relpath);
}

static int filelist_geticon_ex(
        const int typeflag, const int blentype, const char *relpath, const bool is_main, const bool ignore_libdir)
{
	if ((typeflag & FILE_TYPE_DIR) && !(ignore_libdir && (typeflag & (FILE_TYPE_BLENDERLIB | FILE_TYPE_BLENDER)))) {
		if (FILENAME_IS_PARENT(relpath)) {
			return is_main ? ICON_FILE_PARENT : ICON_NONE;
		}
		else if (typeflag & FILE_TYPE_APPLICATIONBUNDLE) {
			return ICON_UGLYPACKAGE;
		}
		else if (typeflag & FILE_TYPE_BLENDER) {
			return ICON_FILE_BLEND;
		}
		else if (is_main) {
			/* Do not return icon for folders if icons are not 'main' draw type (e.g. when used over previews). */
			return ICON_FILE_FOLDER;
		}
	}

	if (typeflag & FILE_TYPE_BLENDER)
		return ICON_FILE_BLEND;
	else if (typeflag & FILE_TYPE_BLENDER_BACKUP)
		return ICON_FILE_BACKUP;
	else if (typeflag & FILE_TYPE_IMAGE)
		return ICON_FILE_IMAGE;
	else if (typeflag & FILE_TYPE_MOVIE)
		return ICON_FILE_MOVIE;
	else if (typeflag & FILE_TYPE_PYSCRIPT)
		return ICON_FILE_SCRIPT;
	else if (typeflag & FILE_TYPE_SOUND)
		return ICON_FILE_SOUND;
	else if (typeflag & FILE_TYPE_FTFONT)
		return ICON_FILE_FONT;
	else if (typeflag & FILE_TYPE_BTX)
		return ICON_FILE_BLANK;
	else if (typeflag & FILE_TYPE_COLLADA)
		return ICON_FILE_BLANK;
	else if (typeflag & FILE_TYPE_TEXT)
		return ICON_FILE_TEXT;
	else if (typeflag & FILE_TYPE_BLENDERLIB) {
		/* TODO: this should most likely be completed and moved to UI_interface_icons.h ? unless it already exists somewhere... */
		switch (blentype) {
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
	return is_main ? ICON_FILE_BLANK : ICON_NONE;
}

int filelist_geticon(struct FileList *filelist, const int index, const bool is_main)
{
	FileDirEntry *file = filelist_geticon_get_file(filelist, index);

	return filelist_geticon_ex(file->typeflag, file->blentype, file->relpath, is_main, false);
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

static void filelist_intern_free(FileListIntern *filelist_intern)
{
	FileDirEntry *entry;

	for (entry = filelist_intern->entries.first; entry; entry = entry->next) {
		BKE_filedir_entry_free(entry);
	}
	BLI_freelistN(&filelist_intern->entries);

	MEM_SAFE_FREE(filelist_intern->filtered);
}

static FileDirEntry *filelist_intern_create_entry(FileList *filelist, const int index)
{
	/* Stupid code for now, later we will actually generate a new entry (from mempool)... */
	return filelist->filelist_intern.filtered[index];
}

static void filelist_intern_release_entry(FileList *UNUSED(filelist), FileDirEntry *UNUSED(old))
{
	/* We do nothing here actually, later we'll give back the mem to the mempool... */
}

static void filelist_cache_previewf(TaskPool *pool, void *taskdata, int threadid)
{
	FileListEntryCache *cache = taskdata;
	FileListEntryPreview *preview;

	printf("%s: Start (%d)...\n", __func__, threadid);

	/* Note we wait on queue here. */
	while (!BLI_task_pool_canceled(pool) && (preview = BLI_thread_queue_pop(cache->previews_todo))) {
//		printf("%s: %d - %s - %p\n", __func__, preview->index, preview->path, preview->img);
		if (preview->flags & FILE_TYPE_IMAGE) {
			preview->img = IMB_thumb_manage(preview->path, THB_NORMAL, THB_SOURCE_IMAGE);
		}
		else if (preview->flags & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
			preview->img = IMB_thumb_manage(preview->path, THB_NORMAL, THB_SOURCE_BLEND);
		}
		else if (preview->flags & FILE_TYPE_MOVIE) {
			preview->img = IMB_thumb_manage(preview->path, THB_NORMAL, THB_SOURCE_MOVIE);
			if (!preview->img) {
				/* remember that file can't be loaded via IMB_open_anim */
				preview->flags &= ~FILE_TYPE_MOVIE;
				preview->flags |= FILE_TYPE_MOVIE_ICON;
			}
		}
		BLI_thread_queue_push(cache->previews_done, preview);
	}

	printf("%s: End (%d)...\n", __func__, threadid);
}

static void filelist_cache_previews_clear(FileListEntryCache *cache)
{
	FileListEntryPreview *preview;

	if (cache->previews_pool) {
		while ((preview = BLI_thread_queue_pop_timeout(cache->previews_todo, 0))) {
//			printf("%s: TODO %d - %s - %p\n", __func__, preview->index, preview->path, preview->img);
			MEM_freeN(preview);
		}
		while ((preview = BLI_thread_queue_pop_timeout(cache->previews_done, 0))) {
//			printf("%s: DONE %d - %s - %p\n", __func__, preview->index, preview->path, preview->img);
			if (preview->img) {
				IMB_freeImBuf(preview->img);
			}
			MEM_freeN(preview);
		}
	}
}

static void filelist_cache_previews_free(FileListEntryCache *cache)
{
	if (cache->previews_pool) {
		BLI_thread_queue_nowait(cache->previews_todo);
		BLI_thread_queue_nowait(cache->previews_done);
		BLI_task_pool_cancel(cache->previews_pool);

		filelist_cache_previews_clear(cache);

		BLI_thread_queue_free(cache->previews_done);
		BLI_thread_queue_free(cache->previews_todo);
		BLI_task_pool_free(cache->previews_pool);
		cache->previews_pool = NULL;
		cache->previews_todo = NULL;
		cache->previews_done = NULL;
	}
}

static void filelist_cache_previews_push(FileList *filelist, FileDirEntry *entry, const int index)
{
	FileListEntryCache *cache = &filelist->filelist_cache;

	if (!entry->image &&
		(entry->typeflag & (FILE_TYPE_IMAGE | FILE_TYPE_MOVIE | FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)))
	{
		FileListEntryPreview *preview = MEM_mallocN(sizeof(*preview), __func__);
		BLI_join_dirfile(preview->path, sizeof(preview->path), filelist->filelist.root, entry->relpath);
		preview->index = index;
		preview->flags = entry->typeflag;
		preview->img = NULL;
//		printf("%s: %d - %s - %p\n", __func__, preview->index, preview->path, preview->img);
		BLI_thread_queue_push(cache->previews_todo, preview);
	}
}

static void filelist_cache_init(FileListEntryCache *cache)
{
	cache->block_cursor = cache->block_start_index = cache->block_center_index = cache->block_end_index = 0;

	cache->misc_entries = BLI_ghash_ptr_new_ex(__func__, FILELIST_ENTRYCACHESIZE);
	cache->misc_cursor = 0;
}

static void filelist_cache_free(FileListEntryCache *cache)
{
	filelist_cache_previews_free(cache);

	/* Note we nearly have nothing to do here, entries are just 'borrowed', not owned by cache... */
	if (cache->misc_entries) {
		BLI_ghash_free(cache->misc_entries, NULL, NULL);
		cache->misc_entries = NULL;
	}
}

static void filelist_cache_clear(FileListEntryCache *cache)
{
	filelist_cache_previews_free(cache);

	/* Note we nearly have nothing to do here, entries are just 'borrowed', not owned by cache... */
	cache->block_cursor = cache->block_start_index = cache->block_center_index = cache->block_end_index = 0;

	if (cache->misc_entries) {
		BLI_ghash_clear_ex(cache->misc_entries, NULL, NULL, FILELIST_ENTRYCACHESIZE);
	}
}

FileList *filelist_new(short type)
{
	FileList *p = MEM_callocN(sizeof(*p), __func__);

	filelist_cache_init(&p->filelist_cache);

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

	filelist_filter_clear(filelist);

	BKE_filedir_entryarr_clear(&filelist->filelist);

	filelist_cache_clear(&filelist->filelist_cache);

	filelist_intern_free(&filelist->filelist_intern);
}

void filelist_free(struct FileList *filelist)
{
	if (!filelist) {
		printf("Attempting to delete empty filelist.\n");
		return;
	}
	
	filelist_clear(filelist);
	filelist_cache_free(&filelist->filelist_cache);  /* XXX TODO stupid! */

	if (filelist->ae) {
		BKE_asset_engine_free(filelist->ae);
		filelist->ae = NULL;
	}

	memset(&filelist->filter_data, 0, sizeof(filelist->filter_data));

	filelist->need_sorting = false;
	filelist->sort = FILE_SORT_NONE;
	filelist->need_filtering = false;
}

void filelist_freelib(struct FileList *filelist)
{
	if (filelist->libfiledata)
		BLO_blendhandle_close(filelist->libfiledata);
	filelist->libfiledata = NULL;
}

AssetEngine *filelist_assetengine_get(struct FileList *filelist)
{
	return filelist->ae;
}

BlendHandle *filelist_lib(struct FileList *filelist)
{
	return filelist->libfiledata;
}

int filelist_numfiles(struct FileList *filelist)
{
	return filelist->filelist.nbr_entries_filtered;
}

static const char *fileentry_uiname(const char *root, const FileDirEntry *entry, char *buff)
{
	char *name;

	if (entry->typeflag & FILE_TYPE_BLENDERLIB) {
		char abspath[FILE_MAX_LIBEXTRA];
		char *group;

		BLI_join_dirfile(abspath, sizeof(abspath), root, entry->relpath);
		BLO_library_path_explode(abspath, buff, &group, &name);
		if (!name) {
			name = group;
		}
	}
	else if (entry->typeflag & FILE_TYPE_DIR) {
		name = entry->relpath;
	}
	else {
		name = (char *)BLI_path_basename(entry->relpath);
	}
	BLI_assert(name);

	return name;
}

void filelist_assetengine_set(struct FileList *filelist, struct AssetEngineType *aet)
{
	if (filelist->ae) {
		if (filelist->ae->type == aet) {
			return;
		}
		BKE_asset_engine_free(filelist->ae);
		filelist->ae = NULL;
	}
	else if (!aet) {
		return;
	}

	if (aet) {
		filelist->ae = BKE_asset_engine_create(aet);
	}
	filelist->force_reset = true;
}

AssetEngine *ED_filelist_assetengine_get(SpaceFile *sfile)
{
	return sfile->files->ae;
}

const char *filelist_dir(struct FileList *filelist)
{
	return filelist->filelist.root;
}

/**
 * May modify in place given r_dir, which is expected to be FILE_MAX_LIBEXTRA length.
 */
void filelist_setdir(struct FileList *filelist, char *r_dir)
{
#ifndef NDEBUG
	size_t len = strlen(r_dir);
	BLI_assert((len < FILE_MAX_LIBEXTRA) && r_dir[len - 1] == '/');
#endif

	BLI_cleanup_dir(G.main->name, r_dir);
	BLI_add_slash(r_dir);
	filelist->checkdirf(filelist, r_dir);

	if (!STREQ(filelist->filelist.root, r_dir)) {
		BLI_strncpy(filelist->filelist.root, r_dir, sizeof(filelist->filelist.root));
		filelist->force_reset = true;
	}
}

void filelist_setrecursion(struct FileList *filelist, const int recursion_level)
{
	if (filelist->max_recursion != recursion_level) {
		filelist->max_recursion = recursion_level;
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
	return (BLI_listbase_is_empty(&filelist->filelist.entries) || filelist->need_filtering ||
	        filelist->force_reset || filelist->force_refresh || filelist->need_sorting);
}

void filelist_clear_refresh(struct FileList *filelist)
{
	filelist->force_refresh = false;
}

static FileDirEntry *filelist_file_ex(struct FileList *filelist, const int index, const bool use_request)
{
	FileDirEntry *ret = NULL, *old;
	FileListEntryCache *cache = &filelist->filelist_cache;
	int old_index;

	if ((index < 0) || (index >= filelist->filelist.nbr_entries_filtered)) {
		return ret;
	}

	if (index >= cache->block_start_index && index < cache->block_end_index) {
		const int idx = (index - cache->block_start_index + cache->block_cursor) % FILELIST_ENTRYCACHESIZE;
		return cache->block_entries[idx];
	}

	if ((ret = BLI_ghash_lookup(cache->misc_entries, SET_INT_IN_POINTER(index)))) {
		return ret;
	}

	if (!use_request) {
		return NULL;
	}

	printf("requesting file %d (not yet cached)\n", index);

	/* Else, we have to add new entry to 'misc' cache - and possibly make room for it first! */
	ret = filelist_intern_create_entry(filelist, index);
	old_index = cache->misc_entries_indices[cache->misc_cursor];
	if ((old = BLI_ghash_popkey(cache->misc_entries, SET_INT_IN_POINTER(old_index), NULL))) {
		filelist_intern_release_entry(filelist, old);
	}
	BLI_ghash_insert(cache->misc_entries, SET_INT_IN_POINTER(index), ret);
	cache->misc_entries_indices[cache->misc_cursor] = index;
	cache->misc_cursor = (cache->misc_cursor + 1) % FILELIST_ENTRYCACHESIZE;

#if 0  /* Actually no, only block cached entries should have preview imho. */
	if (cache->previews_pool) {
		filelist_cache_previews_push(filelist, ret, index);
	}
#endif

	return ret;
}

FileDirEntry *filelist_file(struct FileList *filelist, int index)
{
	return filelist_file_ex(filelist, index, true);
}

int filelist_file_findpath(struct FileList *filelist, const char *filename)
{
	int fidx = -1;
	
	if (filelist->filelist.nbr_entries_filtered < 0) {
		return fidx;
	}

	/* XXX TODO Cache could probably use a ghash on paths too? Not really urgent though.
     *          In fact, we may get rid of this func in the end (only used to find again renamed entry afaik). */

	for (fidx = 0; fidx < filelist->filelist.nbr_entries_filtered; fidx++) {
		if (STREQ(filelist->filelist_intern.filtered[fidx]->relpath, filename)) {
			return fidx;
		}
	}

	return -1;
}

/* Helper, low-level, it assumes cursor + size <= FILELIST_ENTRYCACHESIZE */
static bool filelist_file_cache_block_do(struct FileList *filelist, const int start_index, const int size, int cursor)
{
	FileListEntryCache *cache = &filelist->filelist_cache;

	if (filelist->ae) {
		FileDirEntryArr tmp_arr;
		FileDirEntry *tmp_entry;
		int i;

		if (!filelist->ae->type->entries_block_get) {
			printf("%s: Asset Engine %s does not implement 'entries_block_get'...\n", __func__, filelist->ae->type->name);
			return false;
		}

		tmp_arr = filelist->filelist;
		BLI_listbase_clear(&tmp_arr.entries);
		if (!filelist->ae->type->entries_block_get(filelist->ae, start_index, start_index + size, &tmp_arr)) {
			BKE_filedir_entryarr_clear(&tmp_arr);
			return false;
		}

		for (i = 0, tmp_entry = tmp_arr.entries.first; i < size; i++, cursor++, tmp_entry = tmp_entry->next) {
			cache->block_entries[cursor] = tmp_entry;
		}
	}
	else {
		FileListIntern *intern = &filelist->filelist_intern;
		memcpy(&cache->block_entries[cursor], &intern->filtered[start_index],
		       sizeof(cache->block_entries[cursor]) * size);
	}

	return false;
}

/* Load in cache all entries "around" given index (as much as block cache may hold). */
bool filelist_file_cache_block(struct FileList *filelist, const int index)
{
	FileListEntryCache *cache = &filelist->filelist_cache;

	const int nbr_entries = filelist->filelist.nbr_entries_filtered;
	int start_index = max_ii(0, index - (FILELIST_ENTRYCACHESIZE / 2));
	int end_index = min_ii(nbr_entries, index + (FILELIST_ENTRYCACHESIZE / 2));
	const int curr_block_size = cache->block_end_index - cache->block_start_index;
	int i;

	if ((index < 0) || (index >= nbr_entries)) {
		printf("Wrong index %d ([%d:%d])", index, 0, nbr_entries);
		return false;
	}

	/* Maximize cached range! */
	if ((end_index - start_index) < FILELIST_ENTRYCACHESIZE) {
		if (start_index == 0) {
			end_index = min_ii(nbr_entries, start_index + FILELIST_ENTRYCACHESIZE);
		}
		else if (end_index == nbr_entries) {
			start_index = max_ii(0, end_index - FILELIST_ENTRYCACHESIZE);
		}
	}

	BLI_assert((end_index - start_index) <= FILELIST_ENTRYCACHESIZE) ;

	printf("%s: [%d:%d] around index %d (current cache: [%d:%d])\n", __func__,
	       start_index, end_index, index, cache->block_start_index, cache->block_end_index);

	/* If we have something to (re)cache... */
	if ((start_index != cache->block_start_index) || (end_index != cache->block_end_index)) {
		if ((start_index >= cache->block_end_index) || (end_index <= cache->block_start_index)) {
	//		printf("Full Recaching!\n");

			/* New cached block does not overlap existing one, simple. */
			if (!filelist_file_cache_block_do(filelist, start_index, end_index - start_index, 0)) {
				return false;
			}

			if (cache->previews_pool) {
				filelist_cache_previews_clear(cache);
			}

			cache->block_start_index = start_index;
			cache->block_end_index = end_index;
			cache->block_cursor = 0;
		}
		else {
	//		printf("Partial Recaching!\n");

			/* At this point, we know we keep part of currently cached entries, so update previews if needed,
			 * and remove everything from working queue - we'll add all newly needed entries at the end. */
			if (cache->previews_pool) {
				filelist_cache_previews_update(filelist);
				filelist_cache_previews_clear(cache);
			}

	//		printf("\tpreview cleaned up...\n");

			if (end_index > cache->block_end_index) {
				/* Add (request) needed entries after already cached ones. */
				/* Note: We need some index black magic to wrap around (cycle) inside our FILELIST_ENTRYCACHESIZE array... */
				int size1 = end_index - cache->block_end_index;
				int size2 = 0;
				int idx1, idx2;

				idx1 = (cache->block_cursor + curr_block_size) % FILELIST_ENTRYCACHESIZE;
				if ((idx1 + size1) > FILELIST_ENTRYCACHESIZE) {
					size2 = size1;
					size1 = FILELIST_ENTRYCACHESIZE - idx1;
					size2 -= size1;
					idx2 = 0;
				}

				if (size2) {
					if (!filelist_file_cache_block_do(filelist, end_index - size2, size2, idx2)) {
						return false;
					}
				}
				if (!filelist_file_cache_block_do(filelist, end_index - size1 - size2, size1, idx1)) {
					return false;
				}
			}
			cache->block_end_index = end_index;

	//		printf("\tend-extended...\n");

			if (start_index < cache->block_start_index) {
				/* Add (request) needed entries before already cached ones. */
				/* Note: We need some index black magic to wrap around (cycle) inside our FILELIST_ENTRYCACHESIZE array... */
				int size1 = cache->block_start_index - start_index;
				int size2 = 0;
				int idx1, idx2;

				if (size1 > cache->block_cursor) {
					size2 = size1;
					size1 -= cache->block_cursor;
					size2 -= size1;
					idx2 = 0;
					idx1 = FILELIST_ENTRYCACHESIZE - size1;
				}
				else {
					idx1 = cache->block_cursor - size1;
				}

				if (size2) {
					if (!filelist_file_cache_block_do(filelist, start_index + size1, size2, idx2)) {
						return false;
					}
				}
				if (!filelist_file_cache_block_do(filelist, start_index, size1, idx1)) {
					return false;
				}

				cache->block_cursor = idx1;
			}
			else if (start_index > cache->block_start_index) {
				/* We do not free anything, just update start index and cursor. */
				cache->block_cursor = (cache->block_cursor + start_index - cache->block_start_index) % FILELIST_ENTRYCACHESIZE;
			}
			cache->block_start_index = start_index;
	//		printf("\tstart-extended...\n");
		}
	}
	else if (cache->block_center_index != index && cache->previews_pool) {
		/* We try to always preview visible entries first, so 'restart' preview background task. */
		filelist_cache_previews_update(filelist);
		filelist_cache_previews_clear(cache);
	}

//	printf("Re-queueing previews...\n");

	/* Note we try to preview first images around given index - i.e. assumed visible ones. */
	if (cache->previews_pool) {
		for (i = 0; ((index + i) < end_index) || ((index - i) >= start_index); i++) {
			if ((index - i) >= start_index) {
				const int idx = (cache->block_cursor + (index - start_index) - i) % FILELIST_ENTRYCACHESIZE;
				filelist_cache_previews_push(filelist, cache->block_entries[idx], index - i);
			}
			if ((index + i) < end_index) {
				const int idx = (cache->block_cursor + (index - start_index) + i) % FILELIST_ENTRYCACHESIZE;
				filelist_cache_previews_push(filelist, cache->block_entries[idx], index + i);
			}
		}
	}

	cache->block_center_index = index;

	printf("%s Finished!\n", __func__);

	return true;
}

void filelist_cache_previews_set(FileList *filelist, const bool use_previews)
{
	FileListEntryCache *cache = &filelist->filelist_cache;
	if (use_previews == (cache->previews_pool != NULL)) {
		return;
	}
	else if (use_previews) {
		TaskScheduler *scheduler = BLI_task_scheduler_get();
		TaskPool *pool;
		int num_tasks = 4;
		int i;

		BLI_assert((cache->previews_pool == NULL) && (cache->previews_todo == NULL) && (cache->previews_done == NULL));

//		printf("%s: Init Previews...\n", __func__);

		pool = cache->previews_pool = BLI_task_pool_create(scheduler, NULL);
		cache->previews_todo = BLI_thread_queue_init();
		cache->previews_done = BLI_thread_queue_init();

		while (num_tasks--) {
			BLI_task_pool_push(pool, filelist_cache_previewf, cache, false, TASK_PRIORITY_HIGH);
		}

		i = -(cache->block_end_index - cache->block_start_index);
		while (i++) {
			const int idx = (cache->block_cursor - i) % FILELIST_ENTRYCACHESIZE;
			FileDirEntry *entry = cache->block_entries[idx];

			filelist_cache_previews_push(filelist, entry, cache->block_start_index - i);
		}
	}
	else {
//		printf("%s: Clear Previews...\n", __func__);

		filelist_cache_previews_free(cache);
	}
}

bool filelist_cache_previews_update(FileList *filelist)
{
	FileListEntryCache *cache = &filelist->filelist_cache;
	TaskPool *pool = cache->previews_pool;
	bool changed = false;

	if (!pool) {
		return changed;
	}

//	printf("%s: Update Previews...\n", __func__);

	while (BLI_thread_queue_size(cache->previews_done) > 0) {
		FileListEntryPreview *preview = BLI_thread_queue_pop(cache->previews_done);
//		printf("%s: %d - %s - %p\n", __func__, preview->index, preview->path, preview->img);

		if (preview->img) {
			/* entry might have been removed from cache in the mean while, we do not want to cache it again here. */
			FileDirEntry *entry = filelist_file_ex(filelist, preview->index, false);
			if (entry) {
				entry->image = preview->img;
				/* update flag for movie files where thumbnail can't be created */
				if (preview->flags & FILE_TYPE_MOVIE_ICON) {
					entry->typeflag &= ~FILE_TYPE_MOVIE;
					entry->typeflag |= FILE_TYPE_MOVIE_ICON;
				}
				changed = true;
			}
			else {
				IMB_freeImBuf(preview->img);
			}
		}
		MEM_freeN(preview);
	}

	return changed;
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
		return FILE_TYPE_BLENDER;
	}
	else if (file_is_blend_backup(path)) {
		return FILE_TYPE_BLENDER_BACKUP;
	}
	else if (BLI_testextensie(path, ".app")) {
		return FILE_TYPE_APPLICATIONBUNDLE;
	}
	else if (BLI_testextensie(path, ".py")) {
		return FILE_TYPE_PYSCRIPT;
	}
	else if (BLI_testextensie_n(path, ".txt", ".glsl", ".osl", ".data", NULL)) {
		return FILE_TYPE_TEXT;
	}
	else if (BLI_testextensie_n(path, ".ttf", ".ttc", ".pfb", ".otf", ".otc", NULL)) {
		return FILE_TYPE_FTFONT;
	}
	else if (BLI_testextensie(path, ".btx")) {
		return FILE_TYPE_BTX;
	}
	else if (BLI_testextensie(path, ".dae")) {
		return FILE_TYPE_COLLADA;
	}
	else if (BLI_testextensie_array(path, imb_ext_image) ||
	         (G.have_quicktime && BLI_testextensie_array(path, imb_ext_image_qt)))
	{
		return FILE_TYPE_IMAGE;
	}
	else if (BLI_testextensie(path, ".ogg")) {
		if (IMB_isanim(path)) {
			return FILE_TYPE_MOVIE;
		}
		else {
			return FILE_TYPE_SOUND;
		}
	}
	else if (BLI_testextensie_array(path, imb_ext_movie)) {
		return FILE_TYPE_MOVIE;
	}
	else if (BLI_testextensie_array(path, imb_ext_audio)) {
		return FILE_TYPE_SOUND;
	}
	return 0;
}

static int file_extension_type(const char *dir, const char *relpath)
{
	char path[FILE_MAX];
	BLI_join_dirfile(path, sizeof(path), dir, relpath);
	return path_extension_type(path);
}

int ED_file_extension_icon(const char *path)
{
	int type = path_extension_type(path);
	
	if (type == FILE_TYPE_BLENDER)
		return ICON_FILE_BLEND;
	else if (type == FILE_TYPE_BLENDER_BACKUP)
		return ICON_FILE_BACKUP;
	else if (type == FILE_TYPE_IMAGE)
		return ICON_FILE_IMAGE;
	else if (type == FILE_TYPE_MOVIE)
		return ICON_FILE_MOVIE;
	else if (type == FILE_TYPE_PYSCRIPT)
		return ICON_FILE_SCRIPT;
	else if (type == FILE_TYPE_SOUND)
		return ICON_FILE_SOUND;
	else if (type == FILE_TYPE_FTFONT)
		return ICON_FILE_FONT;
	else if (type == FILE_TYPE_BTX)
		return ICON_FILE_BLANK;
	else if (type == FILE_TYPE_COLLADA)
		return ICON_FILE_BLANK;
	else if (type == FILE_TYPE_TEXT)
		return ICON_FILE_TEXT;
	
	return ICON_FILE_BLANK;
}

int filelist_empty(struct FileList *filelist)
{
	return (filelist->filelist.nbr_entries == 0);
}

void filelist_select_file(FileList *filelist, int index, FileSelType select, unsigned int flag, FileCheckType check)
{
	FileDirEntry *entry = filelist_file(filelist, index);
	if (entry) {
		bool check_ok = false;
		switch (check) {
			case CHECK_DIRS:
				check_ok = ((entry->typeflag & FILE_TYPE_DIR) != 0);
				break;
			case CHECK_ALL:
				check_ok = true;
				break;
			case CHECK_FILES:
			default:
				check_ok = ((entry->typeflag & FILE_TYPE_DIR) == 0);
				break;
		}
		if (check_ok) {
			switch (select) {
				case FILE_SEL_REMOVE:
					entry->selflag &= ~flag;
					break;
				case FILE_SEL_ADD:
					entry->selflag |= flag;
					break;
				case FILE_SEL_TOGGLE:
					entry->selflag ^= flag;
					break;
			}
		}
	}
}

void filelist_select(FileList *filelist, FileSelection *sel, FileSelType select, unsigned int flag, FileCheckType check)
{
	/* select all valid files between first and last indicated */
	if ((sel->first >= 0) && (sel->first < filelist->filelist.nbr_entries_filtered) &&
	    (sel->last >= 0) && (sel->last < filelist->filelist.nbr_entries_filtered))
	{
		int current_file;
		for (current_file = sel->first; current_file <= sel->last; current_file++) {
			filelist_select_file(filelist, current_file, select, flag, check);
		}
	}
}

bool filelist_is_selected(FileList *filelist, int index, FileCheckType check)
{
	FileDirEntry *entry = filelist_file(filelist, index);
	if (entry) {
		return BKE_filedir_entry_is_selected(entry, check);
	}

	return false;
}

/**
 * Returns a list of selected entries, if is_virtual is false also calls asset engine's load_pre callback.
 * Note first item of returned list shall be used as 'active' file.
 */
FileDirEntryArr *filelist_selection_get(FileList *filelist, FileCheckType check, const char *name, const bool use_ae)
{
	FileDirEntryArr *selection;
	int i, totfiles = filelist->filelist.nbr_entries_filtered;
	bool done_name = false;

	selection = MEM_mallocN(sizeof(*selection), __func__);
	*selection = filelist->filelist;
	selection->nbr_entries = 0;
	BLI_listbase_clear(&selection->entries);

#if 0  /* XXX Needs changes in how selection is handled! */
	for (i = 0; i < totfiles; i++) {
		FileDirEntry *entry_org = filelist->filtered[i];

		/* Always include 'name' (i.e. given relpath) */
		if (!done_name && STREQ(entry_org->relpath, name)) {
			FileDirEntry *entry_new = BKE_filedir_entry_copy(entry_org);

			/* We add it in head - first entry in this list is always considered 'active' one. */
			BLI_addhead(&selection->entries, entry_new);
			selection->nbr_entries++;
			done_name = true;
		}
		else if (BKE_filedir_entry_is_selected(entry_org, check)) {
			FileDirEntry *entry_new = BKE_filedir_entry_copy(entry_org);
			BLI_addtail(&selection->entries, entry_new);
			selection->nbr_entries++;
		}
	}

	if (use_ae && filelist->ae) {
		/* This will 'rewrite' selection list, returned paths are expected to be valid! */
		BKE_asset_engine_load_pre(filelist->ae, selection);
	}
#endif
	return selection;
}

bool filelist_islibrary(struct FileList *filelist, char *dir, char **group)
{
	return BLO_library_path_explode(filelist->filelist.root, dir, group, NULL);
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

	return BKE_idcode_to_idfilter(id_code);
}

/*
 * From here, we are in 'Job Context', i.e. have to be careful about sharing stuff between bacground working thread
 * and main one (used by UI among other things).
 */

typedef struct TodoDir {
	int level;
	char *dir;
} TodoDir;

static int filelist_readjob_list_dir(
        const char *root, ListBase *entries, const char *filter_glob,
        const bool do_lib, const char *main_name, const bool skip_currpar)
{
	struct direntry *files;
	int nbr_files, nbr_entries = 0;

	nbr_files = BLI_filelist_dir_contents(root, &files);
	if (files) {
		int i = nbr_files;
		while (i--) {
			FileDirEntry *entry;
			FileDirEntryRevision *rev;

			if (skip_currpar && FILENAME_IS_CURRPAR(files[i].relname)) {
				continue;
			}

			entry = MEM_callocN(sizeof(*entry), __func__);
			rev = entry->entry = MEM_callocN(sizeof(*rev), __func__);
			entry->relpath = MEM_dupallocN(files[i].relname);
			if (S_ISDIR(files[i].s.st_mode)) {
				entry->typeflag |= FILE_TYPE_DIR;
			}
			rev->size = (uint64_t)files[i].s.st_size;
			rev->time = (int64_t)files[i].s.st_mtime;
			/* TODO rather use real values from direntry.s!!! */
			memcpy(rev->size_str, files[i].size, sizeof(rev->size_str));
//			memcpy(rev->mode1, files[i].mode1, sizeof(rev->mode1));
//			memcpy(rev->mode2, files[i].mode2, sizeof(rev->mode2));
//			memcpy(rev->mode3, files[i].mode3, sizeof(rev->mode3));
//			memcpy(rev->owner, files[i].owner, sizeof(rev->owner));
			memcpy(rev->time_str, files[i].time, sizeof(rev->time_str));
			memcpy(rev->date_str, files[i].date, sizeof(rev->date_str));

			/* Set file type. */
			/* If we are considering .blend files as libs, promote them to directory status! */
			if (do_lib && BLO_has_bfile_extension(entry->relpath)) {
				char name[FILE_MAX];

				entry->typeflag = FILE_TYPE_BLENDER;

				BLI_join_dirfile(name, sizeof(name), root, entry->relpath);

				/* prevent current file being used as acceptable dir */
				if (BLI_path_cmp(main_name, name) != 0) {
					entry->typeflag |= FILE_TYPE_DIR;
				}
			}
			/* Otherwise, do not check extensions for directories! */
			else if (!(entry->typeflag & FILE_TYPE_DIR)) {
				if (filter_glob[0] && BLI_testextensie_glob(entry->relpath, filter_glob)) {
					entry->typeflag = FILE_TYPE_OPERATOR;
				}
				else {
					entry->typeflag = file_extension_type(root, entry->relpath);
				}
			}

			BLI_addtail(entries, entry);
			nbr_entries++;
		}
		BLI_filelist_free(files, nbr_files);
	}
	return nbr_entries;
}

static int filelist_readjob_list_lib(const char *root, ListBase *entries, const bool skip_currpar)
{
	FileDirEntry *entry;
	FileDirEntryRevision *rev;
	LinkNode *ln, *names, *lp, *previews;
	struct ImBuf *ima;
	int i, nprevs, nnames, idcode = 0, nbr_entries = 0;
	char dir[FILE_MAX], *group;
	bool ok;

	struct BlendHandle *libfiledata = NULL;

	/* name test */
	ok = BLO_library_path_explode(root, dir, &group, NULL);
	if (!ok) {
		return nbr_entries;
	}

	/* there we go */
	libfiledata = BLO_blendhandle_from_file(dir, NULL);
	if (libfiledata == NULL) {
		return nbr_entries;
	}

	/* memory for strings is passed into filelist[i].entry->relpath and freed in BKE_filedir_entry_free. */
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

	if (!skip_currpar) {
		entry = MEM_callocN(sizeof(*entry), __func__);
		/*rev = */entry->entry = MEM_callocN(sizeof(*rev), __func__);
		entry->relpath = BLI_strdup(FILENAME_PARENT);
		entry->typeflag |= (FILE_TYPE_BLENDERLIB | FILE_TYPE_DIR);
		BLI_addtail(entries, entry);
		nbr_entries++;
	}

	if (previews && (nnames != nprevs)) {
		printf("filelist_from_library: error, found %d items, %d previews\n", nnames, nprevs);
		BLI_linklist_free(previews, BKE_previewimg_freefunc);
		previews = NULL;
	}

	for (i = 0, ln = names, lp = previews; i < nnames; i++, ln = ln->next) {
		const char *blockname = ln->link;

		entry = MEM_callocN(sizeof(*entry), __func__);
		/*rev = */entry->entry = MEM_callocN(sizeof(*rev), __func__);  /* Todo: set date/time from blend file one? */
		entry->relpath = BLI_strdup(blockname);
		entry->typeflag |= FILE_TYPE_BLENDERLIB;
		if (!(group && idcode)) {
			entry->typeflag |= FILE_TYPE_DIR;
			entry->blentype = groupname_to_code(blockname);
		}
		else {
			entry->blentype = idcode;
		}
		if (lp) {
			PreviewImage *img = lp->link;
			if (img) {
				unsigned int w = img->w[ICON_SIZE_PREVIEW];
				unsigned int h = img->h[ICON_SIZE_PREVIEW];
				unsigned int *rect = img->rect[ICON_SIZE_PREVIEW];

				if (w > 0 && h > 0 && rect) {
					/* first allocate imbuf for copying preview into it */
					ima = IMB_allocImBuf(w, h, 32, IB_rect);
					memcpy(ima->rect, rect, w * h * sizeof(unsigned int));
					entry->image = ima;
				}
			}
			lp = lp->next;
		}

		BLI_addtail(entries, entry);
		nbr_entries++;
	}

	BLI_linklist_free(names, free);
	if (previews) {
		BLI_linklist_free(previews, BKE_previewimg_freefunc);
	}

	return nbr_entries;
}

#if 0
/* Kept for reference here, in case we want to add back that feature later. We do not need it currently. */
/* Code ***NOT*** updated for job stuff! */
static void filelist_readjob_main_rec(struct FileList *filelist)
{
	ID *id;
	FileDirEntry *files, *firstlib = NULL;
	ListBase *lb;
	int a, fake, idcode, ok, totlib, totbl;
	
	// filelist->type = FILE_MAIN; // XXX TODO: add modes to filebrowser

	BLI_assert(filelist->filelist.entries == NULL);

	if (filelist->filelist.root[0] == '/') filelist->filelist.root[0] = '\0';

	if (filelist->filelist.root[0]) {
		idcode = groupname_to_code(filelist->filelist.root);
		if (idcode == 0) filelist->filelist.root[0] = '\0';
	}

	if (filelist->dir[0] == 0) {
		/* make directories */
#ifdef WITH_FREESTYLE
		filelist->filelist.nbr_entries = 24;
#else
		filelist->filelist.nbr_entries = 23;
#endif
		filelist_resize(filelist, filelist->filelist.nbr_entries);

		for (a = 0; a < filelist->filelist.nbr_entries; a++) {
			filelist->filelist.entries[a].typeflag |= FILE_TYPE_DIR;
		}

		filelist->filelist.entries[0].entry->relpath = BLI_strdup(FILENAME_PARENT);
		filelist->filelist.entries[1].entry->relpath = BLI_strdup("Scene");
		filelist->filelist.entries[2].entry->relpath = BLI_strdup("Object");
		filelist->filelist.entries[3].entry->relpath = BLI_strdup("Mesh");
		filelist->filelist.entries[4].entry->relpath = BLI_strdup("Curve");
		filelist->filelist.entries[5].entry->relpath = BLI_strdup("Metaball");
		filelist->filelist.entries[6].entry->relpath = BLI_strdup("Material");
		filelist->filelist.entries[7].entry->relpath = BLI_strdup("Texture");
		filelist->filelist.entries[8].entry->relpath = BLI_strdup("Image");
		filelist->filelist.entries[9].entry->relpath = BLI_strdup("Ika");
		filelist->filelist.entries[10].entry->relpath = BLI_strdup("Wave");
		filelist->filelist.entries[11].entry->relpath = BLI_strdup("Lattice");
		filelist->filelist.entries[12].entry->relpath = BLI_strdup("Lamp");
		filelist->filelist.entries[13].entry->relpath = BLI_strdup("Camera");
		filelist->filelist.entries[14].entry->relpath = BLI_strdup("Ipo");
		filelist->filelist.entries[15].entry->relpath = BLI_strdup("World");
		filelist->filelist.entries[16].entry->relpath = BLI_strdup("Screen");
		filelist->filelist.entries[17].entry->relpath = BLI_strdup("VFont");
		filelist->filelist.entries[18].entry->relpath = BLI_strdup("Text");
		filelist->filelist.entries[19].entry->relpath = BLI_strdup("Armature");
		filelist->filelist.entries[20].entry->relpath = BLI_strdup("Action");
		filelist->filelist.entries[21].entry->relpath = BLI_strdup("NodeTree");
		filelist->filelist.entries[22].entry->relpath = BLI_strdup("Speaker");
#ifdef WITH_FREESTYLE
		filelist->filelist.entries[23].entry->relpath = BLI_strdup("FreestyleLineStyle");
#endif
	}
	else {
		/* make files */
		idcode = groupname_to_code(filelist->filelist.root);

		lb = which_libbase(G.main, idcode);
		if (lb == NULL) return;

		filelist->filelist.nbr_entries = 0;
		for (id = lb->first; id; id = id->next) {
			if (!filelist->filter_data.hide_dot || id->name[2] != '.') {
				filelist->filelist.nbr_entries++;
			}
		}

		/* XXX TODO: if databrowse F4 or append/link filelist->hide_parent has to be set */
		if (!filelist->filter_data.hide_parent) filelist->filelist.nbr_entries++;

		if (filelist->filelist.nbr_entries > 0) {
			filelist_resize(filelist, filelist->filelist.nbr_entries);
		}

		files = filelist->filelist.entries;
		
		if (!filelist->filter_data.hide_parent) {
			files->entry->relpath = BLI_strdup(FILENAME_PARENT);
			files->typeflag |= FILE_TYPE_DIR;

			files++;
		}

		totlib = totbl = 0;
		for (id = lb->first; id; id = id->next) {
			ok = 1;
			if (ok) {
				if (!filelist->filter_data.hide_dot || id->name[2] != '.') {
					if (id->lib == NULL) {
						files->entry->relpath = BLI_strdup(id->name + 2);
					}
					else {
						files->entry->relpath = MEM_mallocN(sizeof(*files->relpath) * (FILE_MAX + (MAX_ID_NAME - 2)), __func__);
						BLI_snprintf(files->entry->relpath, FILE_MAX + (MAX_ID_NAME - 2) + 3, "%s | %s", id->lib->name, id->name + 2);
					}
//					files->type |= S_IFREG;
#if 0               /* XXX TODO show the selection status of the objects */
					if (!filelist->has_func) { /* F4 DATA BROWSE */
						if (idcode == ID_OB) {
							if ( ((Object *)id)->flag & SELECT) files->entry->selflag |= FILE_SEL_SELECTED;
						}
						else if (idcode == ID_SCE) {
							if ( ((Scene *)id)->r.scemode & R_BG_RENDER) files->entry->selflag |= FILE_SEL_SELECTED;
						}
					}
#endif
//					files->entry->nr = totbl + 1;
					files->entry->poin = id;
					fake = id->flag & LIB_FAKEUSER;
					if (idcode == ID_MA || idcode == ID_TE || idcode == ID_LA || idcode == ID_WO || idcode == ID_IM) {
						files->typeflag |= FILE_TYPE_IMAGE;
					}
//					if      (id->lib && fake) BLI_snprintf(files->extra, sizeof(files->entry->extra), "LF %d",    id->us);
//					else if (id->lib)         BLI_snprintf(files->extra, sizeof(files->entry->extra), "L    %d",  id->us);
//					else if (fake)            BLI_snprintf(files->extra, sizeof(files->entry->extra), "F    %d",  id->us);
//					else                      BLI_snprintf(files->extra, sizeof(files->entry->extra), "      %d", id->us);

					if (id->lib) {
						if (totlib == 0) firstlib = files;
						totlib++;
					}

					files++;
				}
				totbl++;
			}
		}

		/* only qsort of library blocks */
		if (totlib > 1) {
			qsort(firstlib, totlib, sizeof(*files), compare_name);
		}
	}
}
#endif

static void filelist_readjob_do(
        const bool do_lib,
        FileList *filelist, const char *main_name, short *stop, short *do_update, float *progress, ThreadMutex *lock)
{
	ListBase entries = {0};
	BLI_Stack *todo_dirs;
	TodoDir *td_dir;
	char dir[FILE_MAX_LIBEXTRA];
	char filter_glob[64];  /* TODO should be define! */
	const char *root = filelist->filelist.root;
	const int max_recursion = filelist->max_recursion;
	int nbr_done_dirs = 0, nbr_todo_dirs = 1;

//	BLI_assert(filelist->filtered == NULL);
	BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) && (filelist->filelist.nbr_entries == 0));

	todo_dirs = BLI_stack_new(sizeof(*td_dir), __func__);
	td_dir = BLI_stack_push_r(todo_dirs);
	td_dir->level = 1;

	BLI_strncpy(dir, filelist->filelist.root, sizeof(dir));
	BLI_strncpy(filter_glob, filelist->filter_data.filter_glob, sizeof(filter_glob));

	BLI_cleanup_dir(main_name, dir);
	td_dir->dir = BLI_strdup(dir);

	while (!BLI_stack_is_empty(todo_dirs) && !(*stop)) {
		FileDirEntry *entry;
		int nbr_entries = 0;
		bool is_lib = do_lib;

		char *subdir;
		int recursion_level;
		bool skip_currpar;

		td_dir = BLI_stack_peek(todo_dirs);
		subdir = td_dir->dir;
		recursion_level = td_dir->level;
		skip_currpar = (recursion_level > 1);

		BLI_stack_discard(todo_dirs);

		if (do_lib) {
			nbr_entries = filelist_readjob_list_lib(subdir, &entries, skip_currpar);
		}
		if (!nbr_entries) {
			is_lib = false;
			nbr_entries = filelist_readjob_list_dir(subdir, &entries, filter_glob, do_lib, main_name, skip_currpar);
		}

		for (entry = entries.first; entry; entry = entry->next) {
			BLI_join_dirfile(dir, sizeof(dir), subdir, entry->relpath);
			BLI_cleanup_file(root, dir);
			BLI_path_rel(dir, root);
			/* Only thing we change in direntry here, so we need to free it first. */
			MEM_freeN(entry->relpath);
			entry->relpath = BLI_strdup(dir + 2);  /* + 2 to remove '//' added by BLI_path_rel */
			entry->name = BLI_strdup(fileentry_uiname(root, entry, dir));

			/* Here we decide whether current filedirentry is to be listed too, or not. */
			if (max_recursion && (is_lib || (recursion_level <= max_recursion))) {
				if (((entry->typeflag & FILE_TYPE_DIR) == 0) || FILENAME_IS_CURRPAR(entry->relpath)) {
					/* Skip... */
				}
				else if (!is_lib && (recursion_level >= max_recursion) &&
						 ((entry->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) == 0))
				{
					/* Do not recurse in real directories in this case, only in .blend libs. */
				}
				else {
					/* We have a directory we want to list, add it to todo list! */
					BLI_join_dirfile(dir, sizeof(dir), root, entry->relpath);
					BLI_cleanup_dir(main_name, dir);
					td_dir = BLI_stack_push_r(todo_dirs);
					td_dir->level = recursion_level + 1;
					td_dir->dir = BLI_strdup(dir);
					nbr_todo_dirs++;
				}
			}
		}

		if (nbr_entries) {
			BLI_mutex_lock(lock);

			BLI_movelisttolist(&filelist->filelist.entries, &entries);
			filelist->filelist.nbr_entries += nbr_entries;

			BLI_mutex_unlock(lock);
		}

		nbr_done_dirs++;
		*progress = (float)nbr_done_dirs / (float)nbr_todo_dirs;
		*do_update = true;
		MEM_freeN(subdir);
	}

	/* If we were interrupted by stop, stack may not be empty and we need to free pending dir paths. */
	while (!BLI_stack_is_empty(todo_dirs)) {
		td_dir = BLI_stack_peek(todo_dirs);
		MEM_freeN(td_dir->dir);
		BLI_stack_discard(todo_dirs);
	}
	BLI_stack_free(todo_dirs);
}

static void filelist_readjob_dir(
        FileList *filelist, const char *main_name, short *stop, short *do_update, float *progress, ThreadMutex *lock)
{
	filelist_readjob_do(false, filelist, main_name, stop, do_update, progress, lock);
}

static void filelist_readjob_lib(
        FileList *filelist, const char *main_name, short *stop, short *do_update, float *progress, ThreadMutex *lock)
{
	filelist_readjob_do(true, filelist, main_name, stop, do_update, progress, lock);
}

static void filelist_readjob_main(
        FileList *filelist, const char *main_name, short *stop, short *do_update, float *progress, ThreadMutex *lock)
{
	/* TODO! */
	filelist_readjob_dir(filelist, main_name, stop, do_update, progress, lock);
}


typedef struct FileListReadJob {
	ThreadMutex lock;
	char main_name[FILE_MAX];
	struct FileList *filelist;
	struct FileList *tmp_filelist;

	int ae_job_id;
	float *progress;
	short *stop;
	//~ ReportList reports;
} FileListReadJob;

static void filelist_readjob_startjob(void *flrjv, short *stop, short *do_update, float *progress)
{
	FileListReadJob *flrj = flrjv;

	if (flrj->filelist->ae) {
		flrj->progress = progress;
		flrj->stop = stop;
		/* When using AE engine, worker thread here is just sleeping! */
		while (flrj->filelist->filelist_pending && !*stop) {
			PIL_sleep_ms(10);
			*do_update = true;
		}
	}
	else {
	    printf("START filelist reading (%d files, main thread: %d)\n",
	           flrj->filelist->filelist.nbr_entries, BLI_thread_is_main());

		BLI_mutex_lock(&flrj->lock);

		BLI_assert((flrj->tmp_filelist == NULL) && flrj->filelist);

		flrj->tmp_filelist = MEM_dupallocN(flrj->filelist);

		BLI_mutex_unlock(&flrj->lock);

		BLI_listbase_clear(&flrj->tmp_filelist->filelist.entries);
		flrj->tmp_filelist->filelist.nbr_entries = 0;
		flrj->tmp_filelist->filelist_intern.filtered = NULL;
		BLI_listbase_clear(&flrj->tmp_filelist->filelist_intern.entries);
		flrj->tmp_filelist->libfiledata = NULL;
		memset(&flrj->tmp_filelist->filelist_cache, 0, sizeof(FileListEntryCache));

		flrj->tmp_filelist->read_jobf(flrj->tmp_filelist, flrj->main_name, stop, do_update, progress, &flrj->lock);
	}
}

static void filelist_readjob_update(void *flrjv)
{
	FileListReadJob *flrj = flrjv;

	if (flrj->filelist->force_reset) {
		*flrj->stop = true;
	}
	else if (flrj->filelist->ae) {
		/* We only communicate with asset engine from main thread! */
		AssetEngine *ae = flrj->filelist->ae;
		FileDirEntry *entry;

		flrj->ae_job_id = ae->type->list_dir(ae, flrj->ae_job_id, &flrj->filelist->filelist);
		flrj->filelist->need_sorting = true;
		flrj->filelist->need_filtering = true;
		flrj->filelist->force_refresh = true;

		for (entry = flrj->filelist->filelist.entries.first; entry; entry = entry->next) {
			BLI_assert(!BLI_listbase_is_empty(&entry->variants) && entry->nbr_variants);
			BLI_assert(entry->act_variant < entry->nbr_variants);
			if (!entry->name) {
				char buff[FILE_MAX_LIBEXTRA];
				entry->name = BLI_strdup(fileentry_uiname(flrj->filelist->filelist.root, entry, buff));
			}
			if (!entry->entry) {
				FileDirEntryVariant *variant = BLI_findlink(&entry->variants, entry->act_variant);
				BLI_assert(!BLI_listbase_is_empty(&variant->revisions) && variant->nbr_revisions);
				BLI_assert(variant->act_revision < variant->nbr_revisions);
				entry->entry = BLI_findlink(&variant->revisions, variant->act_revision);
				BLI_assert(entry->entry);
			}
		}

		*flrj->progress = ae->type->progress(ae, flrj->ae_job_id);
		if ((ae->type->status(ae, flrj->ae_job_id) & (AE_STATUS_RUNNING | AE_STATUS_VALID)) != (AE_STATUS_RUNNING | AE_STATUS_VALID)) {
			*flrj->stop = true;
		}
	}
	else {
		FileListIntern *fl_intern = &flrj->filelist->filelist_intern;
		ListBase new_entries = {NULL};
		int nbr_entries, new_nbr_entries = 0;

		BLI_movelisttolist(&new_entries, &fl_intern->entries);
		nbr_entries = flrj->filelist->filelist.nbr_entries;

		BLI_mutex_lock(&flrj->lock);

		if (flrj->tmp_filelist->filelist.nbr_entries) {
			/* We just move everything out of 'thread context' into final list. */
			new_nbr_entries = flrj->tmp_filelist->filelist.nbr_entries;
			BLI_movelisttolist(&new_entries, &flrj->tmp_filelist->filelist.entries);
			flrj->tmp_filelist->filelist.nbr_entries = 0;
		}

		BLI_mutex_unlock(&flrj->lock);

		if (new_nbr_entries) {
			filelist_clear(flrj->filelist);

			flrj->filelist->need_sorting = true;
			flrj->filelist->need_filtering = true;
			flrj->filelist->force_refresh = true;
		}

		/* if no new_nbr_entries, this is NOP */
		BLI_movelisttolist(&fl_intern->entries, &new_entries);
		flrj->filelist->filelist.nbr_entries = nbr_entries + new_nbr_entries;
	}
}

static void filelist_readjob_endjob(void *flrjv)
{
	FileListReadJob *flrj = flrjv;

	flrj->filelist->filelist_pending = false;
	flrj->filelist->filelist_ready = true;

	if (flrj->filelist->ae) {
		AssetEngine *ae = flrj->filelist->ae;
		ae->type->kill(ae, flrj->ae_job_id);
	}
}

static void filelist_readjob_free(void *flrjv)
{
	FileListReadJob *flrj = flrjv;

	printf("END filelist reading (%d files)\n", flrj->filelist->filelist.nbr_entries);

	if (flrj->tmp_filelist) {
		/* tmp_filelist shall never ever be filtered! */
		BLI_assert(flrj->tmp_filelist->filelist.nbr_entries == 0);
		BLI_assert(BLI_listbase_is_empty(&flrj->tmp_filelist->filelist.entries));

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
	flrj = MEM_callocN(sizeof(*flrj), __func__);
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
