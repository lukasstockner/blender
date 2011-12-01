#include "COM_MuteNode.h"
#include "COM_SocketConnection.h"
#include "stdio.h"

MuteNode::MuteNode(bNode *editorNode): Node(editorNode) {
}

void MuteNode::reconnect(OutputSocket * output) {
	vector<InputSocket*> &inputsockets = this->getInputSockets();
	for (unsigned int index = 0; index < inputsockets.size() ; index ++) {
		InputSocket *input = inputsockets[index];
		if (input->getDataType() == output->getDataType()) {
			if (input->isConnected()) {
				output->relinkConnections(input->getConnection()->getFromSocket(), false);
                return;
            }
        }
    }

	output->clearConnections();
    printf("!Unknown how to mute\n");
}

void MuteNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	vector<OutputSocket*> &outputsockets = this->getOutputSockets();

	for (unsigned int index = 0 ; index < outputsockets.size() ; index ++) {
		OutputSocket * output = outputsockets[index];
		if (output->isConnected()) {
            reconnect(output);
        }
    }
}
