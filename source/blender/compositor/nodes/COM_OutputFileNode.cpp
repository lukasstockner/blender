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

#include "COM_OutputFileNode.h"
#include "COM_OutputFileOperation.h"
#include "COM_ExecutionSystem.h"

OutputFileNode::OutputFileNode(bNode *editorNode): Node(editorNode) {
}

void OutputFileNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	InputSocket *imageSocket = this->getInputSocket(0);
	InputSocket *zSocket = this->getInputSocket(1);
	NodeImageFile* storage = (NodeImageFile*)this->getbNode()->storage;
	if (imageSocket->isConnected()) {
//		if (context->isRendering()) {
			if (storage->sfra == storage->efra || (context->getFramenumber()<=storage->efra && context->getFramenumber()>=storage->sfra)) {
				OutputFileOperation *outputFileOperation = new OutputFileOperation();
				outputFileOperation->setScene(context->getScene());
				outputFileOperation->setNodeImageFile(storage);
				outputFileOperation->setbNodeTree(context->getbNodeTree());
				imageSocket->relinkConnections(outputFileOperation->getInputSocket(0));
				zSocket->relinkConnections(outputFileOperation->getInputSocket(1));
				graph->addOperation(outputFileOperation);
				addPreviewOperation(graph, outputFileOperation->getInputSocket(0), 5);
//			}
		} else {
			addPreviewOperation(graph, imageSocket->getOperation()->getOutputSocket(), 5);
		}
	}
}
