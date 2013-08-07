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

BLI_INLINE bool isPointInsideQuad(const float x, const float y, const float corners[4][2])
{
	float point[2];

	point[0] = x;
	point[1] = y;

	return isect_point_tri_v2(point, corners[0], corners[1], corners[2]) ||
	       isect_point_tri_v2(point, corners[0], corners[2], corners[3]);
}

BLI_INLINE void resolveUV(const float x, const float y, const float corners[4][2], float uv[2])
{
	float point[2];

	point[0] = x;
	point[1] = y;

	/* Use reverse bilinear to get UV coordinates within original frame */
	resolve_quad_uv(uv, point, corners[0], corners[1], corners[2], corners[3]);
}

PlaneTrackWarpImageOperation::PlaneTrackWarpImageOperation() : PlaneTrackCommonOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_pixelReader = NULL;
	this->setComplex(true);
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
	float frame_space_corners[4][2];

	for (int i = 0; i < 4; i++) {
		frame_space_corners[i][0] = this->m_corners[i][0] * this->getWidth();
		frame_space_corners[i][1] = this->m_corners[i][1] * this->getHeight();
	}

	if (isPointInsideQuad(x, y, frame_space_corners)) {
		float inputUV[2];
		float uv_a[2], uv_b[2];
		float u, v;

		float dx, dy;
		float uv_l, uv_r;
		float uv_u, uv_d;

		resolveUV(x, y, frame_space_corners, inputUV);

		/* adaptive sampling, red (U) channel */
		resolveUV(x - 1, y, frame_space_corners, uv_a);
		resolveUV(x + 1, y, frame_space_corners, uv_b);
		uv_l = fabsf(inputUV[0] - uv_a[0]);
		uv_r = fabsf(inputUV[0] - uv_b[0]);

		dx = 0.5f * (uv_l + uv_r);

		/* adaptive sampling, green (V) channel */
		resolveUV(x, y - 1, frame_space_corners, uv_a);
		resolveUV(x, y + 1, frame_space_corners, uv_b);
		uv_u = fabsf(inputUV[1] - uv_a[1]);
		uv_d = fabsf(inputUV[1] - uv_b[1]);

		dy = 0.5f * (uv_u + uv_d);

		/* more adaptive sampling, red and green (UV) channels */
		resolveUV(x - 1, y - 1, frame_space_corners, uv_a);
		resolveUV(x - 1, y + 1, frame_space_corners, uv_b);
		uv_l = fabsf(inputUV[0] - uv_a[0]);
		uv_r = fabsf(inputUV[0] - uv_b[0]);
		uv_u = fabsf(inputUV[1] - uv_a[1]);
		uv_d = fabsf(inputUV[1] - uv_b[1]);

		dx += 0.25f * (uv_l + uv_r);
		dy += 0.25f * (uv_u + uv_d);

		resolveUV(x + 1, y - 1, frame_space_corners, uv_a);
		resolveUV(x + 1, y + 1, frame_space_corners, uv_b);
		uv_l = fabsf(inputUV[0] - uv_a[0]);
		uv_r = fabsf(inputUV[0] - uv_b[0]);
		uv_u = fabsf(inputUV[1] - uv_a[1]);
		uv_d = fabsf(inputUV[1] - uv_b[1]);

		dx += 0.25f * (uv_l + uv_r);
		dy += 0.25f * (uv_u + uv_d);

		/* should use mipmap */
		dx = min(dx, 0.2f);
		dy = min(dy, 0.2f);

		u = inputUV[0] * this->m_pixelReader->getWidth();
		v = inputUV[1] * this->m_pixelReader->getHeight();

		this->m_pixelReader->read(output, u, v, dx, dy, COM_PS_BICUBIC);
	}
	else {
		zero_v4(output);
	}
}

bool PlaneTrackWarpImageOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	float frame_space_corners[4][2];

	for (int i = 0; i < 4; i++) {
		frame_space_corners[i][0] = this->m_corners[i][0] * this->getWidth();
		frame_space_corners[i][1] = this->m_corners[i][1] * this->getHeight();
	}

	float UVs[4][2];

	/* TODO(sergey): figure out proper way to do this. */
	resolveUV(input->xmin - 2, input->ymin - 2, frame_space_corners, UVs[0]);
	resolveUV(input->xmax + 2, input->ymin - 2, frame_space_corners, UVs[1]);
	resolveUV(input->xmax + 2, input->ymax + 2, frame_space_corners, UVs[2]);
	resolveUV(input->xmin - 2, input->ymax + 2, frame_space_corners, UVs[3]);

	float min[2], max[2];
	INIT_MINMAX2(min, max);
	for (int i = 0; i < 4; i++) {
		minmax_v2v2_v2(min, max, UVs[i]);
	}

	rcti newInput;

	newInput.xmin = min[0] * readOperation->getWidth() - 1;
	newInput.ymin = min[1] * readOperation->getHeight() - 1;
	newInput.xmax = max[0] * readOperation->getWidth() + 1;
	newInput.ymax = max[1] * readOperation->getHeight() + 1;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
