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
 *
 * Contributor:
 *		Sergey Sharybin
 */

#include "COM_PlaneTrackMaskOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

extern "C" {
	#include "BKE_movieclip.h"
	#include "BKE_node.h"
	#include "BKE_tracking.h"
}

PlaneTrackMaskOperation::PlaneTrackMaskOperation() : PlaneTrackCommonOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
}

void PlaneTrackMaskOperation::executePixel(float output[4], float x, float y, PixelSampler sampler)
{
	const int kernel_size = 4;
	float point[2];
	float frame_space_corners[4][2];

	for (int i = 0; i < 4; i++) {
		frame_space_corners[i][0] = this->m_corners[i][0] * this->getWidth() ;
		frame_space_corners[i][1] = this->m_corners[i][1] * this->getHeight();
	}

	int inside_counter = 0;
	for (int dx = 0; dx < kernel_size; dx++) {
		for (int dy = 0; dy < kernel_size; dy++) {
			point[0] = x + (float) dx / kernel_size;
			point[1] = y + (float) dy / kernel_size;

			if (isect_point_tri_v2(point, frame_space_corners[0], frame_space_corners[1], frame_space_corners[2]) ||
			    isect_point_tri_v2(point, frame_space_corners[0], frame_space_corners[2], frame_space_corners[3]))
			{
				inside_counter++;
			}
		}
	}

	output[0] = (float) inside_counter / (kernel_size * kernel_size);
}
