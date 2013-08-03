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

#include "COM_PlaneTrackWarpMaskOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

extern "C" {
	#include "BKE_movieclip.h"
	#include "BKE_node.h"
	#include "BKE_tracking.h"
}

PlaneTrackWarpMaskOperation::PlaneTrackWarpMaskOperation() : PlaneTrackCommonOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->m_pixelReader = NULL;
}

void PlaneTrackWarpMaskOperation::initExecution()
{
	PlaneTrackCommonOperation::initExecution();

	this->m_pixelReader = this->getInputSocketReader(0);
}

void PlaneTrackWarpMaskOperation::deinitExecution()
{
	this->m_pixelReader = NULL;
}

void PlaneTrackWarpMaskOperation::executePixel(float output[4], float x, float y, PixelSampler sampler)
{
	zero_v4(output);
}
