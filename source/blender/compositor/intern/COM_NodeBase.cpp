#include "COM_NodeBase.h"
#include "string.h"
#include "COM_NodeOperation.h"
#include "BKE_node.h"
#include "COM_SetValueOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SocketConnection.h"
#include "COM_ExecutionSystem.h"

NodeBase::NodeBase() {
}


NodeBase::~NodeBase(){
    this->outputsockets.clear();
    this->inputsockets.clear();
}

void NodeBase::addInputSocket(InputSocket &socket) {
    socket.setNode(this);
    this->inputsockets.push_back(socket);
}
void NodeBase::addOutputSocket(OutputSocket &socket) {
    socket.setNode(this);
    this->outputsockets.push_back(socket);
}
const bool NodeBase::isInputNode() const {
    return this->inputsockets.size() == 0;
}

OutputSocket* NodeBase::getOutputSocket(int index) {
    return &this->outputsockets[index];
}

InputSocket* NodeBase::getInputSocket(int index) {
    return &this->inputsockets[index];
}


void NodeBase::determineActualSocketDataTypes() {
	unsigned int index;
	for (index = 0 ; index < this->outputsockets.size() ; index ++) {
		OutputSocket* socket = &(this->outputsockets[index]);
		if (socket->getActualDataType() ==COM_DT_UNKNOWN && socket->isConnected()) {
			socket->determineActualDataType();
		}
	}
	for (index = 0 ; index < this->inputsockets.size() ; index ++) {
		InputSocket* socket = &(this->inputsockets[index]);
		if (socket->getActualDataType() ==COM_DT_UNKNOWN) {
			socket->determineActualDataType();
		}
	}
}

DataType NodeBase::determineActualDataType(OutputSocket *outputsocket) {
	const int inputIndex = outputsocket->getInputSocketDataTypeDeterminatorIndex();
	if (inputIndex != -1) {
		return this->getInputSocket(inputIndex)->getActualDataType();
	} else {
		return outputsocket->getDataType();
	}
}

void NodeBase::notifyActualDataTypeSet(InputSocket *socket, DataType actualType) {
	unsigned int index;
    int socketIndex = -1;
    for (index = 0 ; index < this->inputsockets.size() ; index ++) {
        if (&this->inputsockets[index] == socket) {
			socketIndex = (int)index;
            break;
        }
    }
    if (socketIndex == -1) return;

    for (index = 0 ; index < this->outputsockets.size() ; index ++) {
        OutputSocket* socket = &(this->outputsockets[index]);
        if (socket->isActualDataTypeDeterminedByInputSocket() &&
                socket->getInputSocketDataTypeDeterminatorIndex() == socketIndex) {
            socket->setActualDataType(actualType);
            socket->fireActualDataType();
        }
    }
}
