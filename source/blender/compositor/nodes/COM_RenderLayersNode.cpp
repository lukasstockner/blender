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

#include "COM_RenderLayersNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_RenderLayersImageProg.h"
#include "COM_RenderLayersAlphaProg.h"
#include "COM_RenderLayersDepthProg.h"
#include "COM_RenderLayersNormalOperation.h"
#include "COM_RenderLayersSpeedOperation.h"
#include "COM_RenderLayersColorOperation.h"
#include "COM_RenderLayersUVOperation.h"
#include "COM_RenderLayersMistOperation.h"
#include "COM_RenderLayersObjectIndexOperation.h"
#include "COM_RenderLayersDiffuseOperation.h"
#include "COM_RenderLayersSpecularOperation.h"
#include "COM_RenderLayersShadowOperation.h"
#include "COM_RenderLayersAOOperation.h"
#include "COM_RenderLayersEmitOperation.h"
#include "COM_RenderLayersReflectionOperation.h"
#include "COM_RenderLayersRefractionOperation.h"
#include "COM_RenderLayersEnvironmentOperation.h"
#include "COM_RenderLayersIndirectOperation.h"
#include "COM_RenderLayersMaterialIndexOperation.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"

RenderLayersNode::RenderLayersNode(bNode *editorNode): Node(editorNode) {
}

void RenderLayersNode::testSocketConnection(ExecutionSystem* system, int outputSocketNumber, RenderLayersBaseProg * operation) {
	OutputSocket *outputSocket = this->getOutputSocket(outputSocketNumber);
	Scene* scene = (Scene*)this->getbNode()->id;
	short layerId = this->getbNode()->custom1;

	if (outputSocket->isConnected()) {
		operation->setScene(scene);
		operation->setLayerId(layerId);
		outputSocket->relinkConnections(operation->getOutputSocket());
		system->addOperation(operation);
		NodeRenderlayerData* data = (NodeRenderlayerData*)this->getbNode()->storage;
		NodeOperation *lastConnection = operation;
		if (data->scalex != 1.0 || data->scaley != 1.0 ) {
			SetValueOperation * valuex = new SetValueOperation();
			SetValueOperation * valuey = new SetValueOperation();
			ScaleOperation * scale = new ScaleOperation();
			valuex->setValue(data->scalex);
			valuey->setValue(data->scaley);

			addLink(system, valuex->getOutputSocket(), scale->getInputSocket(1));
			addLink(system, valuey->getOutputSocket(), scale->getInputSocket(2));

			lastConnection->getOutputSocket()->relinkConnections(scale->getOutputSocket());
			addLink(system, lastConnection->getOutputSocket(), scale->getInputSocket(0));

			lastConnection = scale;

			system->addOperation(valuex);
			system->addOperation(valuey);
			system->addOperation(scale);
		}
		if (data->angle != 0.0) {
			RotateOperation * rotate = new RotateOperation();
			SetValueOperation * valueangle = new SetValueOperation();
			valueangle->setValue(data->angle);
			lastConnection->getOutputSocket()->relinkConnections(rotate->getOutputSocket());
			addLink(system, valueangle->getOutputSocket(), rotate->getInputSocket(1));
			addLink(system, lastConnection->getOutputSocket(), rotate->getInputSocket(0));
			lastConnection = rotate;

			system->addOperation(rotate);
			system->addOperation(valueangle);
		}
		if (data->offsetx != 0 || data->offsety != 0) {
			TranslateOperation * translate = new TranslateOperation();
			SetValueOperation * xop = new SetValueOperation();
			xop->setValue(data->offsetx);
			this->addLink(system, xop->getOutputSocket(), translate->getInputSocket(1));
			SetValueOperation * yop = new SetValueOperation();
			yop->setValue(data->offsety);
			this->addLink(system, yop->getOutputSocket(), translate->getInputSocket(2));
			system->addOperation(xop);
			system->addOperation(yop);

			system->addOperation(translate);
			lastConnection->getOutputSocket()->relinkConnections(translate->getOutputSocket());
			addLink(system, lastConnection->getOutputSocket(), translate->getInputSocket(0));
		}
		if (outputSocketNumber == 0) { // only do for image socket if connected
			addPreviewOperation(system, operation->getOutputSocket(), 9);
		}
	} else {
		if (outputSocketNumber == 0) {
			system->addOperation(operation);
			operation->setScene(scene);
			operation->setLayerId(layerId);
			addPreviewOperation(system, operation->getOutputSocket(), 9);
		} else {
			delete operation;
		}
	}
}

void RenderLayersNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	testSocketConnection(graph, 0, new RenderLayersColourProg());
	testSocketConnection(graph, 1, new RenderLayersAlphaProg());
	testSocketConnection(graph, 2, new RenderLayersDepthProg());
	testSocketConnection(graph, 3, new RenderLayersNormalOperation());
	testSocketConnection(graph, 4, new RenderLayersUVOperation());
	testSocketConnection(graph, 5, new RenderLayersSpeedOperation());
	testSocketConnection(graph, 6, new RenderLayersColorOperation());
	testSocketConnection(graph, 7, new RenderLayersDiffuseOperation());
	testSocketConnection(graph, 8, new RenderLayersSpecularOperation());
	testSocketConnection(graph, 9, new RenderLayersShadowOperation());
	testSocketConnection(graph, 10, new RenderLayersAOOperation());
	testSocketConnection(graph, 11, new RenderLayersReflectionOperation());
	testSocketConnection(graph, 12, new RenderLayersRefractionOperation());
	testSocketConnection(graph, 13, new RenderLayersIndirectOperation());
	testSocketConnection(graph, 14, new RenderLayersObjectIndexOperation());
	testSocketConnection(graph, 15, new RenderLayersMaterialIndexOperation());
	testSocketConnection(graph, 16, new RenderLayersMistOperation());
	testSocketConnection(graph, 17, new RenderLayersEmitOperation());
	testSocketConnection(graph, 18, new RenderLayersEnvironmentOperation());
}
