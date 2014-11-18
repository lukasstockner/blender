#ifndef _GPU_SPRITE_H_
#define _GPU_SPRITE_H_

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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/GPU_sprite.h
 *   \ingroup gpu
 */

#include "BLI_sys_types.h" // for uint32_t

#ifdef __cplusplus
extern "C" {
#endif

void GPU_sprite_begin(void);
void GPU_sprite_end  (void);

void GPU_point_size (float size);
void GPU_sprite_size(float size);

typedef enum GPUSpriteShaderOption {
	GPU_SPRITE_CIRCULAR   = (1<<0), /* */
	GPU_SPRITE_TEXTURE_2D = (1<<1), /* */

	GPU_SPRITE_OPTIONS_NUM         = 1,
	GPU_SPRITE_OPTION_COMBINATIONS = (1<<GPU_SPRITE_OPTIONS_NUM)
} GPUSpriteShaderOption;

void GPU_sprite_2f (float x, float y);
void GPU_sprite_2fv(const float v[2]);

void GPU_sprite_3f (float x, float y, float z);
void GPU_sprite_3fv(const float v[3]);

#ifdef __cplusplus
}
#endif

#endif /* _GPU_SPRITE_H_ */