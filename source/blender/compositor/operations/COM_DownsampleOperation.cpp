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

#include "COM_DownsampleOperation.h"
#include "COM_ReadBufferOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

extern "C" {
	#include "BKE_node.h"
}

DownsampleOperation::DownsampleOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_keepAspect = false;
	this->m_newWidth = 0;
	this->m_newHeight = 0;
	this->setComplex(true);
}

void DownsampleOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	NodeOperation::determineResolution(resolution, preferredResolution);

	if (this->m_keepAspect) {
		float ratio_width = (float) resolution[0] / this->m_newWidth,
		      ratio_height = (float) resolution[1] / this->m_newHeight;
		float ratio = min_ff(ratio_width, ratio_height);
		resolution[0] /= ratio;
		resolution[1] /= ratio;
	}
	else {
		resolution[0] = this->m_newWidth;
		resolution[1] = this->m_newHeight;
	}
}

void *DownsampleOperation::initializeTileData(rcti *rect)
{
	void *buffer = getInputOperation(0)->initializeTileData(rect);

	return buffer;
}

void DownsampleOperation::executePixel(float output[4], int x, int y, void *data)
{
	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();

	int new_width = this->getWidth(),
	    new_height = this->getHeight();

	int input_width = inputBuffer->getWidth(),
	    input_height = inputBuffer->getHeight();

	float width_ratio = (float) input_width / new_width,
	      height_ratio = (float) input_height / new_height;

	float accum_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	int source_x = floor(x * width_ratio),
	    source_y = floor(y * height_ratio);
	int counter = 0;

	for (int delta_y = 0; delta_y < height_ratio; delta_y++) {
		for (int delta_x = 0; delta_x < width_ratio; delta_x++) {
			int current_x = source_x + delta_x,
			    current_y = source_y + delta_y;
			int offset = (current_y * input_width + current_x);
			float *current_color = buffer + offset * COM_NUMBER_OF_CHANNELS;
			add_v4_v4(accum_color, current_color);
			counter++;
		}
	}

	mul_v4_v4fl(output, accum_color, 1.0f / (float) counter);
}

bool DownsampleOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	/* TODO(sergey): This might be wrong, in some tests tiles were missing. */
	float width_ratio = (float) readOperation->getWidth() / this->getWidth(),
	      height_ratio = (float) readOperation->getHeight() / this->getHeight();

	newInput.xmin = input->xmin * width_ratio - 1;
	newInput.ymin = input->ymin * height_ratio - 1;
	newInput.xmax = (input->xmax + 1) * width_ratio + 1;
	newInput.ymax = (input->ymax + 1) * height_ratio + 1;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
