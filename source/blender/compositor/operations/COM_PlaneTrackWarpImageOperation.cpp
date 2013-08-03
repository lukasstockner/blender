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

#include "COM_PlaneTrackWarpImageOperation.h"
#include "COM_ReadBufferOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

extern "C" {
	#include "BKE_movieclip.h"
	#include "BKE_node.h"
	#include "BKE_tracking.h"
}

PlaneTrackWarpImageOperation::PlaneTrackWarpImageOperation() : PlaneTrackCommonOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_pixelReader = NULL;
}

void PlaneTrackWarpImageOperation::initExecution()
{
	PlaneTrackCommonOperation::initExecution();

	this->m_pixelReader = this->getInputSocketReader(0);
}

void PlaneTrackWarpImageOperation::deinitExecution()
{
	this->m_pixelReader = NULL;
}

void PlaneTrackWarpImageOperation::executePixel(float output[4], float x, float y, PixelSampler sampler)
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
		/* Use reverse bilinear to get UV coordinates within original frame
		 * Shall we consider using inverse homography transform here?
		 */
		float uv[2];
		resolve_quad_uv(uv, point, frame_space_corners[0], frame_space_corners[1], frame_space_corners[2], frame_space_corners[3]);

		float source_x = uv[0] * this->m_pixelReader->getWidth(),
		      source_y = uv[1] * this->m_pixelReader->getHeight();

		/* Force to bilinear filtering */
		this->m_pixelReader->read(output, source_x, source_y, COM_PS_BILINEAR);
	}
	else {
		zero_v4(output);
	}
}

bool PlaneTrackWarpImageOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	/* XXX: use real area of interest! */
	newInput.xmin = 0;
	newInput.ymin = 0;
	newInput.xmax = readOperation->getWidth();
	newInput.ymax = readOperation->getHeight();

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
