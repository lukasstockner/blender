#include "COM_SeparateRGBANode.h"

#include "COM_SeparateChannelOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "DNA_material_types.h" // the ramp types


SeparateRGBANode::SeparateRGBANode(bNode *editorNode): Node(editorNode) {
}


void SeparateRGBANode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	InputSocket *imageSocket = this->getInputSocket(0);
	OutputSocket *outputRSocket = this->getOutputSocket(0);
	OutputSocket *outputGSocket = this->getOutputSocket(1);
	OutputSocket *outputBSocket = this->getOutputSocket(2);
	OutputSocket *outputASocket = this->getOutputSocket(3);

	if (outputRSocket->isConnected()) {
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(0);
		imageSocket->relinkConnections(operation->getInputSocket(0), true, 0, true, graph);
		outputRSocket->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
	 }
	if (outputGSocket->isConnected()) {
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(1);
		imageSocket->relinkConnections(operation->getInputSocket(0), true, 0, true, graph);
		outputGSocket->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
	 }
	if (outputBSocket->isConnected()) {
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(2);
		imageSocket->relinkConnections(operation->getInputSocket(0), true, 0, true, graph);
		outputBSocket->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
	 }
	if (outputASocket->isConnected()) {
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(3);
		imageSocket->relinkConnections(operation->getInputSocket(0), true, 0, true, graph);
		outputASocket->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
	 }
}
