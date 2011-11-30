#include "COM_ColorRampNode.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"
#include "COM_ColorRampOperation.h"
#include "COM_SeparateChannelOperation.h"
#include "DNA_texture_types.h"

ColorRampNode::ColorRampNode(bNode* editorNode): Node(editorNode)
{}

void ColorRampNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *inputSocket = this->getInputSocket(0);
    OutputSocket *outputSocket = this->getOutputSocket(0);
	OutputSocket *outputSocketAlpha = this->getOutputSocket(1);
    bNode* editorNode = this->getbNode();

	ColorRampOperation * operation = new ColorRampOperation();
	outputSocket->relinkConnections(operation->getOutputSocket(0));
	if (outputSocketAlpha->isConnected()) {
		SeparateChannelOperation *operation2 = new SeparateChannelOperation();
		outputSocketAlpha->relinkConnections(operation2->getOutputSocket());
		addLink(graph, operation->getOutputSocket(), operation2->getInputSocket(0));
		operation2->setChannel(3);
		graph->addOperation(operation2);
	}
	operation->setColorBand((ColorBand*)editorNode->storage);
	inputSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
	graph->addOperation(operation);
}
