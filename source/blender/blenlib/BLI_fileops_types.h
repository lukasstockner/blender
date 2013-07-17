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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_FILEOPS_TYPES_H__
#define __BLI_FILEOPS_TYPES_H__

/** \file BLI_fileops_types.h
 *  \ingroup bli
 *  \brief Some types for dealing with directories.
 */

#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32

#ifdef _MSC_VER
typedef unsigned int mode_t;
#endif

#  ifdef _USE_32BIT_TIME_T
#    error The _USE_32BIT_TIME_T option and BLI_fileops are not compatible.
#  endif

/* Use stat, wstat, and fstat to make a small and consistent API for
   all Windows build systems.  _stati64 is used because it has 64-bit time and
   file size, and is available in all versions of MSVCRT with 64-bit support */
#  define  stat  _stati64 /* use only as struct, never as function */
#  define wstat _wstati64 /* use only as function, takes UTF16 path */
#  define fstat _fstati64 /* use only as function, takes file descriptor */

#endif


struct ImBuf;

struct direntry {
	mode_t  type;
	char   *relname;
	char   *path;
	struct stat s;
	unsigned int flags;
	char    size[16];
	char    mode1[4];
	char    mode2[4];
	char    mode3[4];
	char    owner[16];
	char    time[8];
	char    date[16];
	char    extra[16];
	void   *poin;
	int     nr;
	struct ImBuf *image;
	unsigned int selflag; /* selection flag */
};

struct dirlink {
	struct dirlink *next, *prev;
	char *name;
};

#endif /* __BLI_FILEOPS_TYPES_H__ */
