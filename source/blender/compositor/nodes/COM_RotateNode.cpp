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

#include "COM_RotateNode.h"

#include "COM_RotateOperation.h"
#include "COM_ExecutionSystem.h"

RotateNode::RotateNode(bNode *editorNode) : Node(editorNode) {
}

void RotateNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *inputSocket = this->getInputSocket(0);
    InputSocket *inputDegreeSocket = this->getInputSocket(1);
    OutputSocket *outputSocket = this->getOutputSocket(0);
    RotateOperation *operation = new RotateOperation();

    inputSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
	operation->setDegree(inputDegreeSocket->getStaticValues()[0]);
    inputDegreeSocket->relinkConnections(operation->getInputSocket(1), true, 1, graph);
    outputSocket->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}
