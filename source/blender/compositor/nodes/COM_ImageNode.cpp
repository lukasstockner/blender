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

#include "COM_ImageNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_ImageOperation.h"

ImageNode::ImageNode(bNode *editorNode): Node(editorNode) {
}

void ImageNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	/// Image output
    OutputSocket *outputImage = this->getOutputSocket(0);
    bNode *editorNode = this->getbNode();
    Image *image = (Image*)editorNode->id;
    ImageUser *imageuser = (ImageUser*)editorNode->storage;


	ImageOperation *operation = new ImageOperation();
	if (outputImage->isConnected()) {
		outputImage->relinkConnections(operation->getOutputSocket());
	}
	operation->setImage(image);
	operation->setImageUser(imageuser);
	operation->setFramenumber(context->getFramenumber());
	graph->addOperation(operation);
	addPreviewOperation(graph, operation->getOutputSocket(), 9);

	OutputSocket *alphaImage = this->getOutputSocket(1);
	if (alphaImage->isConnected()) {
		ImageAlphaOperation *alphaOperation = new ImageAlphaOperation();
		alphaOperation->setImage(image);
		alphaOperation->setImageUser(imageuser);
		alphaOperation->setFramenumber(context->getFramenumber());
		alphaImage->relinkConnections(alphaOperation->getOutputSocket());
		graph->addOperation(alphaOperation);
	}
	/// @todo: ImageZOperation
}
