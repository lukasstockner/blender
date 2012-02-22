/*
 * Copyright 2011, Blender Foundation.
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
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#ifndef _COM_OutputFileOperation_h
#define _COM_OutputFileOperation_h
#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "BLI_rect.h"

class OutputFileOperation : public NodeOperation {
private:
	float *outputBuffer;
	float *zBuffer;
	const Scene *scene;
	NodeImageFile *imageFile;
	const bNodeTree* tree;
	SocketReader* imageInput;
	SocketReader* zInput;
public:
	OutputFileOperation();
	void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers);
	bool isOutputOperation(bool rendering) const {return true;}
	void initExecution();
	void deinitExecution();
	void setScene(const Scene*scene) {this->scene = scene;}
	void setbNodeTree(const bNodeTree *tree) {this->tree= tree;}
	void setNodeImageFile(NodeImageFile*file) {this->imageFile= file;}
	const int getRenderPriority() const {return 7;}
};
#endif
