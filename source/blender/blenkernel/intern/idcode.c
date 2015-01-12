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
 * return info about ID types
 */

/** \file blender/blenkernel/intern/idcode.c
 *  \ingroup bke
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_ID.h"

#include "BLI_utildefines.h"

#include "BKE_idcode.h"

typedef struct {
	unsigned short code;
	const char *name, *plural;
	
	int flags;
#define IDTYPE_FLAGS_ISLINKABLE (1 << 0)
} IDType;

/* plural need to match rna_main.c's MainCollectionDef */
/* WARNING! Keep it in sync with i18n contexts in BLF_translation.h */
static IDType idtypes[] = {
	{ ID_AC,     "Action",           "actions",         IDTYPE_FLAGS_ISLINKABLE },
	{ ID_AR,     "Armature",         "armatures",       IDTYPE_FLAGS_ISLINKABLE },
	{ ID_BR,     "Brush",            "brushes",         IDTYPE_FLAGS_ISLINKABLE },
	{ ID_CA,     "Camera",           "cameras",         IDTYPE_FLAGS_ISLINKABLE },
	{ ID_CU,     "Curve",            "curves",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_GD,     "GPencil",          "grease_pencil",   IDTYPE_FLAGS_ISLINKABLE }, /* rename gpencil */
	{ ID_GR,     "Group",            "groups",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_ID,     "ID",               "ids",             0                       }, /* plural is fake */
	{ ID_IM,     "Image",            "images",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_IP,     "Ipo",              "ipos",            IDTYPE_FLAGS_ISLINKABLE }, /* deprecated */
	{ ID_KE,     "Key",              "shape_keys",      0                       },
	{ ID_LA,     "Lamp",             "lamps",           IDTYPE_FLAGS_ISLINKABLE },
	{ ID_LI,     "Library",          "libraries",       0                       },
	{ ID_LS,     "FreestyleLineStyle", "linestyles",    IDTYPE_FLAGS_ISLINKABLE },
	{ ID_LT,     "Lattice",          "lattices",        IDTYPE_FLAGS_ISLINKABLE },
	{ ID_MA,     "Material",         "materials",       IDTYPE_FLAGS_ISLINKABLE },
	{ ID_MB,     "Metaball",         "metaballs",       IDTYPE_FLAGS_ISLINKABLE },
	{ ID_MC,     "MovieClip",        "movieclips",      IDTYPE_FLAGS_ISLINKABLE },
	{ ID_ME,     "Mesh",             "meshes",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_MSK,    "Mask",             "masks",           IDTYPE_FLAGS_ISLINKABLE },
	{ ID_NT,     "NodeTree",         "node_groups",     IDTYPE_FLAGS_ISLINKABLE },
	{ ID_OB,     "Object",           "objects",         IDTYPE_FLAGS_ISLINKABLE },
	{ ID_PA,     "ParticleSettings", "particles",       0                       },
	{ ID_PAL,    "Palettes",         "palettes",        IDTYPE_FLAGS_ISLINKABLE },
	{ ID_PC,     "PaintCurve",       "paint_curves",    IDTYPE_FLAGS_ISLINKABLE },
	{ ID_SCE,    "Scene",            "scenes",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_SCR,    "Screen",           "screens",         0                       },
	{ ID_SEQ,    "Sequence",         "sequences",       0                       }, /* not actually ID data */
	{ ID_SPK,    "Speaker",          "speakers",        IDTYPE_FLAGS_ISLINKABLE },
	{ ID_SO,     "Sound",            "sounds",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_TE,     "Texture",          "textures",        IDTYPE_FLAGS_ISLINKABLE },
	{ ID_TXT,    "Text",             "texts",           IDTYPE_FLAGS_ISLINKABLE },
	{ ID_VF,     "VFont",            "fonts",           IDTYPE_FLAGS_ISLINKABLE },
	{ ID_WO,     "World",            "worlds",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_WM,     "WindowManager",    "window_managers", 0                       },
};

static IDType *idtype_from_name(const char *str) 
{
	int i = ARRAY_SIZE(idtypes);

	while (i--) {
		if (STREQ(str, idtypes[i].name)) {
			return &idtypes[i];
		}
	}

	return NULL;
}
static IDType *idtype_from_code(int code) 
{
	int i = ARRAY_SIZE(idtypes);

	while (i--)
		if (code == idtypes[i].code)
			return &idtypes[i];
	
	return NULL;
}

/**
 * Return if the ID code is a valid ID code.
 *
 * \param code The code to check.
 * \return Boolean, 0 when invalid.
 */
bool BKE_idcode_is_valid(int code)
{
	return idtype_from_code(code) ? true : false;
}

/**
 * Return non-zero when an ID type is linkable.
 *
 * \param code The code to check.
 * \return Boolean, 0 when non linkable.
 */
bool BKE_idcode_is_linkable(int code)
{
	IDType *idt = idtype_from_code(code);
	BLI_assert(idt);
	return idt ? ((idt->flags & IDTYPE_FLAGS_ISLINKABLE) != 0) : false;
}

/**
 * Convert an idcode into a name.
 *
 * \param code The code to convert.
 * \return A static string representing the name of
 * the code.
 */
const char *BKE_idcode_to_name(int code) 
{
	IDType *idt = idtype_from_code(code);
	BLI_assert(idt);
	return idt ? idt->name : NULL;
}

/**
 * Convert a name into an idcode (ie. ID_SCE)
 *
 * \param name The name to convert.
 * \return The code for the name, or 0 if invalid.
 */
int BKE_idcode_from_name(const char *name) 
{
	IDType *idt = idtype_from_name(name);
	BLI_assert(idt);
	return idt ? idt->code : 0;
}

/**
 * Convert an idcode into a name (plural).
 *
 * \param code The code to convert.
 * \return A static string representing the name of
 * the code.
 */
const char *BKE_idcode_to_name_plural(int code) 
{
	IDType *idt = idtype_from_code(code);
	BLI_assert(idt);
	return idt ? idt->plural : NULL;
}

/**
 * Convert an idcode into an idfilter (e.g. ID_OB -> FILTER_ID_OB).
 */
int BKE_idcode_to_idfilter(const int idcode)
{
	switch (idcode) {
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

/**
 * Convert an idfilter into an idcode (e.g. FILTER_ID_OB -> ID_OB).
 */
int BKE_idcode_from_idfilter(const int idfilter)
{
	switch (idfilter) {
		case FILTER_ID_AC:
			return ID_AC;
		case FILTER_ID_AR:
			return ID_AR;
		case FILTER_ID_BR:
			return ID_BR;
		case FILTER_ID_CA:
			return ID_CA;
		case FILTER_ID_CU:
			return ID_CU;
		case FILTER_ID_GD:
			return ID_GD;
		case FILTER_ID_GR:
			return ID_GR;
		case FILTER_ID_IM:
			return ID_IM;
		case FILTER_ID_LA:
			return ID_LA;
		case FILTER_ID_LS:
			return ID_LS;
		case FILTER_ID_LT:
			return ID_LT;
		case FILTER_ID_MA:
			return ID_MA;
		case FILTER_ID_MB:
			return ID_MB;
		case FILTER_ID_MC:
			return ID_MC;
		case FILTER_ID_ME:
			return ID_ME;
		case FILTER_ID_MSK:
			return ID_MSK;
		case FILTER_ID_NT:
			return ID_NT;
		case FILTER_ID_OB:
			return ID_OB;
		case FILTER_ID_PAL:
			return ID_PAL;
		case FILTER_ID_PC:
			return ID_PC;
		case FILTER_ID_SCE:
			return ID_SCE;
		case FILTER_ID_SPK:
			return ID_SPK;
		case FILTER_ID_SO:
			return ID_SO;
		case FILTER_ID_TE:
			return ID_TE;
		case FILTER_ID_TXT:
			return ID_TXT;
		case FILTER_ID_VF:
			return ID_VF;
		case FILTER_ID_WO:
			return ID_WO;
		default:
			return 0;
	}
}

/**
 * Return an ID code and steps the index forward 1.
 *
 * \param index start as 0.
 * \return the code, 0 when all codes have been returned.
 */
int BKE_idcode_iter_step(int *index)
{
	return (*index < ARRAY_SIZE(idtypes)) ? idtypes[(*index)++].code : 0;
}
