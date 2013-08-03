
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

#ifndef _COM_DownsampleOperation_h
#define _COM_DownsampleOperation_h

#include <string.h>

#include "COM_NodeOperation.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

class DownsampleOperation : public NodeOperation {
protected:
	int m_newWidth;
	int m_newHeight;
	bool m_keepAspect;

	/**
	 * Determine the output resolution. The resolution is retrieved from the Renderer
	 */
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);

public:
	DownsampleOperation();

	void setNewWidth(int new_width) { this->m_newWidth = new_width; }
	void setNewHeight(int new_height) { this->m_newHeight = new_height; }
	void setKeepAspect(bool keep_aspect) { this->m_keepAspect = keep_aspect; }

	void *initializeTileData(rcti *rect);
	void executePixel(float output[4], int x, int y, void *data);

	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};

#endif
