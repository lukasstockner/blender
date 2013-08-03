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
	float point[2];
	float frame_space_corners[4][2];

	for (int i = 0; i < 4; i++) {
		frame_space_corners[i][0] = this->m_corners[i][0] * this->getWidth();
		frame_space_corners[i][1] = this->m_corners[i][1] * this->getHeight();
	}

	point[0] = x;
	point[1] = y;

	if (isect_point_tri_v2(point, frame_space_corners[0], frame_space_corners[1], frame_space_corners[2]) ||
	    isect_point_tri_v2(point, frame_space_corners[0], frame_space_corners[2], frame_space_corners[3]))
	{
		output[0] = 1.0f;
	}
	else {
		output[0] = 0.0f;
	}
}
