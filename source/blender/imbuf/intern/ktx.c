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

/** \file blender/imbuf/intern/ktx.c
 *  \ingroup imbuf
 */

#define KTX_OPENGL 1
#define KTX_USE_GETPROC 1

#include "ktx.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_filetype.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_sys_types.h"

#include <string.h>
#include <stdlib.h>

static char KTX_HEAD[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};


void imb_initktx(void)
{

}

int check_ktx(const unsigned char *mem)
{
	return memcmp(KTX_HEAD, mem, sizeof(KTX_HEAD)) ? 0 : 1;
}

int imb_is_a_ktx(const char *filename)
{
	const char *ctx_extension[] = {
		".ktx",
		NULL
	};

	return BLI_testextensie_array(filename, ctx_extension);
}
struct ImBuf *imb_loadktx(const unsigned char *mem, size_t size, int flags, char * UNUSED(colorspace))
{
	GLuint texture = 0;
	GLenum target;
	GLenum glerror;
	GLboolean isMipmapped;
	KTX_error_code ktxerror;
	unsigned int numKeys;
	unsigned char *keys;

	/* thumbnails are run from a thread so opengl generation below will fail */
	if (flags & IB_thumbnail) {
		return NULL;
	}

	ktxerror = ktxLoadTextureM(mem, size, &texture, &target, NULL, &isMipmapped, &glerror, &numKeys, &keys);

	if (ktxerror == KTX_SUCCESS) {
		ImBuf *ibuf;
		int xsize, ysize;
		int internal_format;
		bool flipx = false, flipy = false;
		glEnable(target);

		if (isMipmapped)
			/* Enable bilinear mipmapping */
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		else
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &xsize);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &ysize);

		ibuf = IMB_allocImBuf(xsize, ysize, 32, (int)IB_rect);

		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &internal_format);

		glDeleteTextures(1, &texture);

		if (internal_format) {
			flipx = flipy = true;
		}
		else {
			if (keys) {
				KTX_hash_table table;
				ktxerror = ktxHashTable_Deserialize(numKeys, keys, &table);
				if (ktxerror == KTX_SUCCESS) {
					unsigned int valLength;
					unsigned char *value;
					ktxerror = ktxHashTable_FindValue(table, KTX_ORIENTATION_KEY, &valLength, (void **)&value);

					if (ktxerror == KTX_SUCCESS) {
						if (value[6] == 'd')
							flipy = true;
					}
				}
			}
		}

		if (flipx && flipy) {
			int i;
			size_t imbuf_size = ibuf->x * ibuf->y;

			for (i = 0; i < imbuf_size / 2; i++) {
				SWAP(unsigned int, ibuf->rect[i], ibuf->rect[imbuf_size - i -1]);
			}
		}
		else if (flipy) {
			size_t i, j;
			for (j = 0; j < ibuf->y / 2; j++) {
				for (i = 0; i < ibuf->x; i++) {
					SWAP(unsigned int, ibuf->rect[i + j * ibuf->x], ibuf->rect[i + (ibuf->y - j - 1) * ibuf->x]);
				}
			}
		}

		if (keys)
			free(keys);

		return ibuf;
	}

	if (keys)
		free(keys);

	return NULL;
}


int imb_savektx(struct ImBuf *ibuf, const char *name, int UNUSED(flags))
{
	KTX_texture_info tinfo;
	KTX_image_info info;
	KTX_error_code ktxerror;

	tinfo.glType = GL_UNSIGNED_BYTE;
	/* compression type, expose as option later */
	tinfo.glTypeSize = 1;
	tinfo.glFormat = GL_RGBA;
	tinfo.glInternalFormat = GL_RGBA;
	tinfo.glBaseInternalFormat = GL_RGBA;
	tinfo.pixelWidth = ibuf->x;
 	tinfo.pixelHeight = ibuf->y;
	tinfo.pixelDepth = 0;
	tinfo.numberOfArrayElements = 0;
	tinfo.numberOfFaces = 1;
	tinfo.numberOfMipmapLevels = 1;

	info.data = (GLubyte *) ibuf->rect;
	info.size = ((size_t)ibuf->x) * ibuf->y * 4 * sizeof(char);

	ktxerror = ktxWriteKTXN(name, &tinfo, 0, NULL, 1, &info);

	if (KTX_SUCCESS == ktxerror) {
		return 1;
	}
	else return 0;
}
