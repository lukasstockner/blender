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
#include "BLI_utildefines.h"

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

#include "ED_fileselect.h"
#include "ED_datafiles.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "PIL_time.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"

#include "filelist.h"

struct FileList;

typedef struct FileImage {
	struct FileImage *next, *prev;
	char path[FILE_MAX];
	unsigned int flags;
	int index;
	short done;
	ImBuf *img;
} FileImage;

typedef struct ThumbnailJob {
	ListBase loadimages;
	const short *stop;
	const short *do_update;
	struct FileList *filelist;
	ReportList reports;
} ThumbnailJob;

typedef struct FileListFilter {
	bool hide_dot;
	unsigned int filter;
	unsigned int filter_id;
	char filter_glob[64];
	char filter_search[66];  /* + 2 for heading/trailing implicit '*' wildcards. */
} FileListFilter;

typedef struct FileList {
	struct direntry *filelist;
	int *fidx;
	int numfiles;
	int numfiltered;
	char dir[FILE_MAX];
	short prv_w;
	short prv_h;
	short changed;

	FileListFilter filter_data;

	struct BlendHandle *libfiledata;
	short hide_parent;

	void (*readf)(struct FileList *);
	bool (*filterf)(struct direntry *file, const char *dir, FileListFilter *filter);

	bool use_recursion;
	short recursion_level;
} FileList;

#define FILELIST_MAX_RECURSION 3

#define FILENAME_IS_BREADCRUMBS(_n) \
	(((_n)[0] == '.' && (_n)[1] == '\0') || ((_n)[0] == '.' && (_n)[1] == '.' && (_n)[2] == '\0'))

typedef struct FolderList {
	struct FolderList *next, *prev;
	char *foldername;
} FolderList;

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


/* ******************* SORT ******************* */

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


/* -----------------FOLDERLIST (previous/next) -------------- */

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

static void filelist_read_main(struct FileList *filelist);
static void filelist_read_library(struct FileList *filelist);
static void filelist_read_dir(struct FileList *filelist);

static void filelist_from_library(struct FileList *filelist, const bool add_parent, const bool use_filter);

/* helper, could probably go in BKE actually? */
static int groupname_to_code(const char *group);
static unsigned int groupname_to_filter_id(const char *group);

static bool is_hidden_file(const char *filename, const bool hide_dot)
{
	char *sep = (char *)BLI_last_slash(filename);
	bool is_hidden = false;

	if (hide_dot) {
		if (filename[0] == '.' && filename[1] != '.' && filename[1] != 0) {
			is_hidden = true; /* ignore .file */
		}
		else {
			int len = strlen(filename);
			if ((len > 0) && (filename[len - 1] == '~')) {
				is_hidden = true;  /* ignore file~ */
			}
		}
	}
	if (!is_hidden && ((filename[0] == '.') && (filename[1] == 0))) {
		is_hidden = true; /* ignore . */
	}
	/* filename might actually be a piece of path, in which case we have to check all its parts. */
	if (!is_hidden && sep) {
		char tmp_filename[FILE_MAX_LIBEXTRA];

		BLI_strncpy(tmp_filename, filename, sizeof(tmp_filename));
		sep = tmp_filename + (sep - filename);
		while (sep) {
			BLI_assert(sep[1] != '\0');
			if (is_hidden_file(sep + 1, hide_dot)) {
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
	bool is_filtered = !is_hidden_file(file->relname, filter->hide_dot);

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
		is_filtered = !is_hidden_file(file->relname, filter->hide_dot);
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
	return !is_hidden_file(file->relname, filter->hide_dot);
}

void filelist_filter(FileList *filelist)
{
	int num_filtered = 0;
	int *fidx_tmp;
	int i;

	if (!filelist->filelist) {
		return;
	}

	fidx_tmp = MEM_mallocN(sizeof(*fidx_tmp) * (size_t)filelist->numfiles, __func__);

	/* How many files are left after filter ? */
	for (i = 0; i < filelist->numfiles; ++i) {
		struct direntry *file = &filelist->filelist[i];
		if (filelist->filterf(file, filelist->dir, &filelist->filter_data)) {
			fidx_tmp[num_filtered++] = i;
		}
	}

	if (filelist->fidx) {
		MEM_freeN(filelist->fidx);
		filelist->fidx = NULL;
	}
	/* Note: maybe we could even accept filelist->fidx to be filelist->numfiles -len allocated? */
	filelist->fidx = (int *)MEM_mallocN(sizeof(*filelist->fidx) * (size_t)num_filtered, __func__);
	memcpy(filelist->fidx, fidx_tmp, sizeof(*filelist->fidx) * (size_t)num_filtered);
	filelist->numfiltered = num_filtered;

	MEM_freeN(fidx_tmp);
}

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

FileList *filelist_new(short type)
{
	FileList *p = MEM_callocN(sizeof(FileList), "filelist");

	switch (type) {
		case FILE_MAIN:
			p->readf = filelist_read_main;
			p->filterf = is_filtered_main;
			break;
		case FILE_LOADLIB:
			p->readf = filelist_read_library;
			p->filterf = is_filtered_lib;
			break;
		default:
			p->readf = filelist_read_dir;
			p->filterf = is_filtered_file;
			break;
	}
	return p;
}


void filelist_free(struct FileList *filelist)
{
	if (!filelist) {
		printf("Attempting to delete empty filelist.\n");
		return;
	}
	
	if (filelist->fidx) {
		MEM_freeN(filelist->fidx);
		filelist->fidx = NULL;
	}

	BLI_free_filelist(filelist->filelist, filelist->numfiles);
	filelist->numfiles = 0;
	filelist->filelist = NULL;
	filelist->numfiltered = 0;
	memset(&filelist->filter_data, 0, sizeof(filelist->filter_data));
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

void filelist_setdir(struct FileList *filelist, const char *dir)
{
	BLI_strncpy(filelist->dir, dir, sizeof(filelist->dir));
}

void filelist_setrecursive(struct FileList *filelist, const bool use_recursion)
{
	if (filelist->use_recursion != use_recursion) {
		filelist->use_recursion = use_recursion;

		filelist_freelib(filelist);
		filelist_free(filelist);
	}
}

void filelist_imgsize(struct FileList *filelist, short w, short h)
{
	filelist->prv_w = w;
	filelist->prv_h = h;
}

short filelist_changed(struct FileList *filelist)
{
	return filelist->changed;
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

ImBuf *filelist_geticon_image(struct FileList *filelist, const int index)
{
	ImBuf *ibuf = NULL;
	struct direntry *file = filelist_geticon_get_file(filelist, index);

	if (file->type & S_IFDIR) {
		if (strcmp(file->relname, "..") == 0) {
			ibuf = gSpecialFileImages[SPECIAL_IMG_PARENT];
		}
		else if (strcmp(file->relname, ".") == 0) {
			ibuf = gSpecialFileImages[SPECIAL_IMG_REFRESH];
		}
		else {
			ibuf = gSpecialFileImages[SPECIAL_IMG_FOLDER];
		}
	}
	else {
		ibuf = gSpecialFileImages[SPECIAL_IMG_UNKNOWNFILE];
	}

	if (file->flags & BLENDERFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_BLENDFILE];
	}
	else if (file->flags & (MOVIEFILE | MOVIEFILE_ICON)) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_MOVIEFILE];
	}
	else if (file->flags & SOUNDFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_SOUNDFILE];
	}
	else if (file->flags & PYSCRIPTFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_PYTHONFILE];
	}
	else if (file->flags & FTFONTFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_FONTFILE];
	}
	else if (file->flags & TEXTFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_TEXTFILE];
	}
	else if (file->flags & IMAGEFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_LOADING];
	}
	else if (file->flags & BLENDERFILE_BACKUP) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_BACKUP];
	}

	return ibuf;
}

int filelist_geticon(struct FileList *filelist, const int index)
{
	struct direntry *file = filelist_geticon_get_file(filelist, index);

	if (file->type & S_IFDIR) {
		if (strcmp(file->relname, "..") == 0) {
			return ICON_FILE_PARENT;
		}
		if (file->flags & APPLICATIONBUNDLE) {
			return ICON_UGLYPACKAGE;
		}
		if (file->flags & BLENDERFILE) {
			return ICON_FILE_BLEND;
		}
		return ICON_FILE_FOLDER;
	}
	else if (file->flags & BLENDERFILE)
		return ICON_FILE_BLEND;
	else if (file->flags & BLENDERFILE_BACKUP)
		return ICON_FILE_BACKUP;
	else if (file->flags & IMAGEFILE)
		return ICON_FILE_IMAGE;
	else if (file->flags & MOVIEFILE)
		return ICON_FILE_MOVIE;
	else if (file->flags & PYSCRIPTFILE)
		return ICON_FILE_SCRIPT;
	else if (file->flags & SOUNDFILE)
		return ICON_FILE_SOUND;
	else if (file->flags & FTFONTFILE)
		return ICON_FILE_FONT;
	else if (file->flags & BTXFILE)
		return ICON_FILE_BLANK;
	else if (file->flags & COLLADAFILE)
		return ICON_FILE_BLANK;
	else if (file->flags & TEXTFILE)
		return ICON_FILE_TEXT;
	else {
		char path[FILE_MAX_LIBEXTRA], dir[FILE_MAXDIR], *group;

		BLI_join_dirfile(path, sizeof(path), filelist->dir, file->relname);

		if (BLO_library_path_explode(path, dir, &group, NULL) && group) {
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
					return ICON_FILE_BLANK;  /* TODO! */
				case ID_NT:
					return ICON_NODETREE;
				case ID_OB:
					return ICON_OBJECT_DATA;
				case ID_PAL:
					return ICON_FILE_BLANK;  /* TODO! */
				case ID_PC:
					return ICON_FILE_BLANK;  /* TODO! */
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
		return ICON_FILE_BLANK;
	}
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

void filelist_setfilter_options(FileList *filelist, const bool hide_dot, const unsigned int filter,
                                const unsigned int filter_id, const char *filter_glob, const char *filter_search)
{
	filelist->filter_data.hide_dot = hide_dot;

	filelist->filter_data.filter = filter;
	filelist->filter_data.filter_id = filter_id;
	BLI_strncpy(filelist->filter_data.filter_glob, filter_glob, sizeof(filelist->filter_data.filter_glob));

	{
		int idx = 0;
		const size_t max_search_len = sizeof(filelist->filter_data.filter_search) - 2;
		const size_t slen = (size_t)min_ii((int)strlen(filter_search), (int)max_search_len);

		if (slen == 0) {
			filelist->filter_data.filter_search[0] = '\0';
		}
		else {
			/* Implicitly add heading/trailing wildcards if needed. */
			if (filter_search[idx] != '*') {
				filelist->filter_data.filter_search[idx++] = '*';
			}
			memcpy(&filelist->filter_data.filter_search[idx], filter_search, slen);
			idx += slen;
			if (filelist->filter_data.filter_search[idx - 1] != '*') {
				filelist->filter_data.filter_search[idx++] = '*';
			}
			filelist->filter_data.filter_search[idx] = '\0';
		}
	}
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

static void filelist_setfiletypes(struct FileList *filelist)
{
	struct direntry *file;
	int num;
	
	file = filelist->filelist;
	
	for (num = 0; num < filelist->numfiles; num++, file++) {
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
		file->flags = file_extension_type(filelist->dir, file->relname);

		if (filelist->filter_data.filter_glob[0] &&
		    BLI_testextensie_glob(file->relname, filelist->filter_data.filter_glob))
		{
			file->flags = OPERATORFILE;
		}
		
	}
}

static void filelist_merge_sublist(struct direntry **filelist_buff, int *filelist_buff_size, int *filelist_used_size,
                                   const char *root, struct FileList *sublist)
{
	if (sublist->numfiles) {
		struct direntry *f;
		int new_numfiles = sublist->numfiles + *filelist_used_size;
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
		for (i = *filelist_used_size, j = 0, f = sublist->filelist; j < sublist->numfiles; j++, f++) {
			if (FILENAME_IS_BREADCRUMBS(f->relname)) {
				/* Ignore 'inner' breadcrumbs! */
				new_numfiles--;
				continue;
			}
			BLI_join_dirfile(dir, sizeof(dir), sublist->dir, f->relname);
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

static void filelist_read_dir(struct FileList *filelist)
{
	/* only used if recursing, will contain all non-immediate children then. */
	struct direntry *file;
	struct direntry *new_filelist = NULL;
	int new_filelist_size = 0, new_filelist_buffsize = 0;
	int i;

	if (!filelist) {
		return;
	}

	BLI_assert(filelist->fidx == NULL);
	BLI_assert(filelist->filelist == NULL);

	BLI_cleanup_dir(G.main->name, filelist->dir);
	filelist->numfiles = BLI_dir_contents(filelist->dir, &(filelist->filelist));

	if (filelist->use_recursion && filelist->recursion_level < FILELIST_MAX_RECURSION) {
		FileList *fl = filelist_new(FILE_UNIX);
		file = filelist->filelist;
		for (i = 0; i < filelist->numfiles; i++, file++) {
			char dir[FILE_MAX];

			if (FILENAME_IS_BREADCRUMBS(file->relname) || (file->type & S_IFDIR) == 0) {
				continue;
			}

			fl->use_recursion = true;
			fl->recursion_level = filelist->recursion_level + 1;

			BLI_join_dirfile(dir, sizeof(dir), filelist->dir, file->relname);
			filelist_setdir(fl, dir);
			BLI_cleanup_dir(G.main->name, fl->dir);
			filelist_read_dir(fl);

			filelist_merge_sublist(&new_filelist, &new_filelist_buffsize, &new_filelist_size, filelist->dir, fl);

			filelist_free(fl);
		}
		MEM_freeN(fl);
	}

	if (new_filelist) {
		struct direntry *final_filelist;
		int final_filelist_size = new_filelist_size + filelist->numfiles;

		final_filelist = malloc(sizeof(*new_filelist) * (size_t)final_filelist_size);
		memcpy(final_filelist, filelist->filelist, sizeof(*final_filelist) * (size_t)filelist->numfiles);
		memcpy(&final_filelist[filelist->numfiles], new_filelist, sizeof(*final_filelist) * (size_t)new_filelist_size);

		free(filelist->filelist);
		filelist->filelist = final_filelist;
		filelist->numfiles = final_filelist_size;
	}

	filelist_setfiletypes(filelist);
	filelist_filter(filelist);
}

static void filelist_read_main(struct FileList *filelist)
{
	if (!filelist) return;
	filelist_from_main(filelist);
}

static void filelist_read_library(struct FileList *filelist)
{
	/* only used if recursing, will contain all non-immediate children then. */
	struct direntry *file;
	struct direntry *new_filelist = NULL;
	int new_filelist_size = 0, new_filelist_buffsize = 0;
	int i;

	if (!filelist) {
		return;
	}

	BLI_assert(filelist->fidx == NULL);
	BLI_assert(filelist->filelist == NULL);

	BLI_cleanup_dir(G.main->name, filelist->dir);
	filelist_from_library(filelist, true, false);

	if (!filelist->libfiledata) {
		FileList *fl = filelist_new(FILE_LOADLIB);
		BLI_make_exist(filelist->dir);
		filelist_read_dir(filelist);
		file = filelist->filelist;
		for (i = 0; i < filelist->numfiles; i++, file++) {
			if (BLO_has_bfile_extension(file->relname)) {
				char name[FILE_MAX];

				BLI_join_dirfile(name, sizeof(name), filelist->dir, file->relname);

				/* prevent current file being used as acceptable dir */
				if (BLI_path_cmp(G.main->name, name) != 0) {
					file->type &= ~S_IFMT;
					file->type |= S_IFDIR;

					if (filelist->use_recursion) {
						char dir[FILE_MAX];

						/* Note we do not consider recursion level here, it has no importance in .blend files anyway. */
						fl->use_recursion = true;

						BLI_join_dirfile(dir, sizeof(dir), filelist->dir, file->relname);
						filelist_setdir(fl, dir);
						BLI_cleanup_dir(G.main->name, fl->dir);
						filelist_read_library(fl);

						filelist_merge_sublist(&new_filelist, &new_filelist_buffsize, &new_filelist_size, filelist->dir, fl);

						filelist_freelib(fl);
						filelist_free(fl);
					}
				}
			}
		}
		MEM_freeN(fl);
	}
	else if (filelist->use_recursion) {
		FileList *fl = filelist_new(FILE_LOADLIB);
		char dir[FILE_MAX], *group;

		const bool is_lib = filelist_islibrary(filelist, dir, &group);

		BLI_assert(is_lib);

		if (group) {
			/* We are at lowest possible level, nothing else to do. */
			return;
		}

		file = filelist->filelist;
		for (i = 0; i < filelist->numfiles; i++, file++) {
			char dir[FILE_MAX];

			if (FILENAME_IS_BREADCRUMBS(file->relname)) {
				continue;
			}

			/* Note we do not consider recursion level here, it has no importance in .blend files anyway. */
			/* And no need to set recursion flag here either. */

			BLI_join_dirfile(dir, sizeof(dir), filelist->dir, file->relname);
			filelist_setdir(fl, dir);
			BLI_cleanup_dir(G.main->name, fl->dir);
			filelist_from_library(fl, false, false);

			filelist_merge_sublist(&new_filelist, &new_filelist_buffsize, &new_filelist_size, filelist->dir, fl);

			filelist_freelib(fl);
			filelist_free(fl);
		}
		MEM_freeN(fl);
	}

	if (new_filelist) {
		struct direntry *final_filelist;
		int final_filelist_size = new_filelist_size + filelist->numfiles;

		final_filelist = malloc(sizeof(*new_filelist) * (size_t)final_filelist_size);
		memcpy(final_filelist, filelist->filelist, sizeof(*final_filelist) * (size_t)filelist->numfiles);
		memcpy(&final_filelist[filelist->numfiles], new_filelist, sizeof(*final_filelist) * (size_t)new_filelist_size);

		free(filelist->filelist);
		filelist->filelist = final_filelist;
		filelist->numfiles = final_filelist_size;
	}

	if (filelist->use_recursion) {
		filelist_setfiletypes(filelist);
	}
	filelist_sort(filelist, FILE_SORT_ALPHA);
	filelist_filter(filelist);
}

void filelist_readdir(struct FileList *filelist)
{
	filelist->readf(filelist);
}

int filelist_empty(struct FileList *filelist)
{	
	return filelist->filelist == NULL;
}

void filelist_parent(struct FileList *filelist)
{
	BLI_parent_dir(filelist->dir);
	BLI_make_exist(filelist->dir);
	filelist_readdir(filelist);
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

void filelist_sort(struct FileList *filelist, short sort)
{
	switch (sort) {
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
	}

	filelist_filter(filelist);
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
 
static void filelist_from_library(struct FileList *filelist, const bool add_parent, const bool use_filter)
{
	LinkNode *l, *names, *previews;
	struct ImBuf *ima;
	int ok, i, nprevs, nnames, idcode = 0;
	char filename[FILE_MAX];
	char dir[FILE_MAX], *group;
	
	/* name test */
	ok = filelist_islibrary(filelist, dir, &group);
	if (!ok) {
		/* free */
		if (filelist->libfiledata) BLO_blendhandle_close(filelist->libfiledata);
		filelist->libfiledata = NULL;
		return;
	}
	
	BLI_strncpy(filename, G.main->name, sizeof(filename));

	/* there we go */
	/* for the time being only read filedata when libfiledata==0 */
	if (filelist->libfiledata == NULL) {
		filelist->libfiledata = BLO_blendhandle_from_file(dir, NULL);
		if (filelist->libfiledata == NULL) return;
	}

	/* memory for strings is passed into filelist[i].relname
	 * and freed in freefilelist */
	if (group) {
		idcode = groupname_to_code(group);
		previews = BLO_blendhandle_get_previews(filelist->libfiledata, idcode, &nprevs);
		names = BLO_blendhandle_get_datablock_names(filelist->libfiledata, idcode, &nnames);
		/* ugh, no rewind, need to reopen */
		BLO_blendhandle_close(filelist->libfiledata);
		filelist->libfiledata = BLO_blendhandle_from_file(dir, NULL);
	}
	else {
		previews = NULL;
		nprevs = 0;
		names = BLO_blendhandle_get_linkable_groups(filelist->libfiledata);
		nnames = BLI_linklist_length(names);
	}

	filelist->numfiles = add_parent ? nnames + 1 : nnames;
	filelist->filelist = malloc(filelist->numfiles * sizeof(*filelist->filelist));
	memset(filelist->filelist, 0, filelist->numfiles * sizeof(*filelist->filelist));

	if (add_parent) {
		filelist->filelist[nnames].relname = BLI_strdup("..");
		filelist->filelist[nnames].type |= S_IFDIR;
		filelist->filelist[nnames].flags |= BLENDERLIB;
	}

	for (i = 0, l = names; i < nnames; i++, l = l->next) {
		const char *blockname = l->link;
		struct direntry *file = &filelist->filelist[i];

		file->relname = BLI_strdup(blockname);
		file->path = BLI_strdupcat(filelist->dir, blockname);
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
			
			if (img) {
				unsigned int w = img->w[ICON_SIZE_PREVIEW];
				unsigned int h = img->h[ICON_SIZE_PREVIEW];
				unsigned int *rect = img->rect[ICON_SIZE_PREVIEW];

				/* first allocate imbuf for copying preview into it */
				if (w > 0 && h > 0 && rect) {
					ima = IMB_allocImBuf(w, h, 32, IB_rect);
					memcpy(ima->rect, rect, w * h * sizeof(unsigned int));
					filelist->filelist[i].image = ima;
					filelist->filelist[i].flags = IMAGEFILE;
				}
			}
		}
	}

	BLI_linklist_free(names, free);
	if (previews) BLI_linklist_free(previews, BKE_previewimg_freefunc);

	//~ filelist_sort(filelist, FILE_SORT_ALPHA);

	BLI_strncpy(G.main->name, filename, sizeof(filename));  /* prevent G.main->name to change */

	if (use_filter) {
		filelist->filter_data.filter = 0;
		filelist_filter(filelist);
	}
}

void filelist_hideparent(struct FileList *filelist, short hide)
{
	filelist->hide_parent = hide;
}

void filelist_from_main(struct FileList *filelist)
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
		filelist->numfiles = 25;
#else
		filelist->numfiles = 24;
#endif
		filelist->filelist = (struct direntry *)malloc(filelist->numfiles * sizeof(struct direntry));
		
		for (a = 0; a < filelist->numfiles; a++) {
			memset(&(filelist->filelist[a]), 0, sizeof(struct direntry));
			filelist->filelist[a].type |= S_IFDIR;
		}
		
		filelist->filelist[0].relname = BLI_strdup("..");
		filelist->filelist[2].relname = BLI_strdup("Scene");
		filelist->filelist[3].relname = BLI_strdup("Object");
		filelist->filelist[4].relname = BLI_strdup("Mesh");
		filelist->filelist[5].relname = BLI_strdup("Curve");
		filelist->filelist[6].relname = BLI_strdup("Metaball");
		filelist->filelist[7].relname = BLI_strdup("Material");
		filelist->filelist[8].relname = BLI_strdup("Texture");
		filelist->filelist[9].relname = BLI_strdup("Image");
		filelist->filelist[10].relname = BLI_strdup("Ika");
		filelist->filelist[11].relname = BLI_strdup("Wave");
		filelist->filelist[12].relname = BLI_strdup("Lattice");
		filelist->filelist[13].relname = BLI_strdup("Lamp");
		filelist->filelist[14].relname = BLI_strdup("Camera");
		filelist->filelist[15].relname = BLI_strdup("Ipo");
		filelist->filelist[16].relname = BLI_strdup("World");
		filelist->filelist[17].relname = BLI_strdup("Screen");
		filelist->filelist[18].relname = BLI_strdup("VFont");
		filelist->filelist[19].relname = BLI_strdup("Text");
		filelist->filelist[20].relname = BLI_strdup("Armature");
		filelist->filelist[21].relname = BLI_strdup("Action");
		filelist->filelist[22].relname = BLI_strdup("NodeTree");
		filelist->filelist[23].relname = BLI_strdup("Speaker");
#ifdef WITH_FREESTYLE
		filelist->filelist[24].relname = BLI_strdup("FreestyleLineStyle");
#endif
		filelist_sort(filelist, FILE_SORT_ALPHA);
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
		if (!filelist->hide_parent) filelist->numfiles += 1;
		filelist->filelist = filelist->numfiles > 0 ? (struct direntry *)malloc(filelist->numfiles * sizeof(struct direntry)) : NULL;

		files = filelist->filelist;
		
		if (!filelist->hide_parent) {
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
	filelist->filter_data.filter = 0;
	filelist_filter(filelist);
}

static void thumbnail_joblist_free(ThumbnailJob *tj)
{
	FileImage *limg = tj->loadimages.first;
	
	/* free the images not yet copied to the filelist -> these will get freed with the filelist */
	for (; limg; limg = limg->next) {
		if ((limg->img) && (!limg->done)) {
			IMB_freeImBuf(limg->img);
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
		if (!filelist->filelist[idx].image) {
			if ((filelist->filelist[idx].flags & (IMAGEFILE | MOVIEFILE | BLENDERFILE | BLENDERFILE_BACKUP))) {
				FileImage *limg = MEM_callocN(sizeof(FileImage), "loadimage");
				BLI_strncpy(limg->path, filelist->filelist[idx].path, FILE_MAX);
				limg->index = idx;
				limg->flags = filelist->filelist[idx].flags;
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
	WM_jobs_callbacks(wm_job, thumbnails_startjob, NULL, thumbnails_update, NULL);

	/* start the job */
	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

void thumbnails_stop(wmWindowManager *wm, FileList *filelist)
{
	WM_jobs_kill(wm, filelist, NULL);
}

int thumbnails_running(wmWindowManager *wm, FileList *filelist)
{
	return WM_jobs_test(wm, filelist, WM_JOB_TYPE_FILESEL_THUMBNAIL);
}
