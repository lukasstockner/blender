#include "COM_GroupNode.h"
#include "COM_SocketProxyNode.h"
#include "COM_ExecutionSystemHelper.h"

GroupNode::GroupNode(bNode *editorNode): Node(editorNode) {
	vector<InputSocket*> &inputsockets = this->getInputSockets();
	vector<OutputSocket*> &outputsockets = this->getOutputSockets();
	unsigned int index;
	for (index = 0 ; index < inputsockets.size();index ++) {
		InputSocket *inputSocket = inputsockets[index];
		OutputSocket *insideSocket = new OutputSocket(inputSocket->getDataType());
		inputSocket->setGroupOutputSocket(insideSocket);
        insideSocket->setInsideOfGroupNode(true);
		insideSocket->setGroupInputSocket(inputSocket);
		insideSocket->setEditorSocket(inputSocket->getbNodeSocket()->groupsock);
        insideSocket->setNode(this);
    }
	for (index = 0 ; index < outputsockets.size();index ++) {
		OutputSocket * outputSocket = outputsockets[index];
		InputSocket * insideSocket = new InputSocket(outputSocket->getDataType());
		outputSocket->setGroupInputSocket(insideSocket);
        insideSocket->setInsideOfGroupNode(true);
		insideSocket->setGroupOutputSocket(outputSocket);
		insideSocket->setEditorSocket(outputSocket->getbNodeSocket()->groupsock);
        insideSocket->setNode(this);
    }
}

void GroupNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
}

void GroupNode::ungroup(ExecutionSystem &system) {
    bNodeTree* subtree = (bNodeTree*)this->getbNode()->id;
	ExecutionSystemHelper::addbNodeTree(system.getNodes(), system.getConnections(), subtree);
	vector<InputSocket*> &inputsockets = this->getInputSockets();
	vector<OutputSocket*> &outputsockets = this->getOutputSockets();
	unsigned int index;

	for (index = 0 ; index < inputsockets.size();index ++) {
		InputSocket * inputSocket = inputsockets[index];
		if (inputSocket->isConnected()) {
			SocketProxyNode * proxy = new SocketProxyNode(this->getbNode());
			inputSocket->relinkConnections(proxy->getInputSocket(0), true, index, &system);
			inputSocket->getGroupOutputSocket()->relinkConnections(proxy->getOutputSocket(0));
			ExecutionSystemHelper::addNode(system.getNodes(), proxy);
        }
    }
	for (index = 0 ; index < outputsockets.size();index ++) {
		OutputSocket * outputSocket = outputsockets[index];
		SocketProxyNode * proxy = new SocketProxyNode(this->getbNode());
		outputSocket->relinkConnections(proxy->getOutputSocket(0));
		outputSocket->getGroupInputSocket()->relinkConnections(proxy->getInputSocket(0));
		ExecutionSystemHelper::addNode(system.getNodes(), proxy);
	}
}
