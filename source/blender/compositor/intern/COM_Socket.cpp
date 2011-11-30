#include "COM_Socket.h"
#include "COM_Node.h"
#include "COM_SocketConnection.h"
#include <stdio.h>

Socket::Socket(DataType datatype) {
    this->datatype = datatype;
	this->actualType = COM_DT_UNKNOWN;
    this->editorSocket = NULL;
    this->node = NULL;
    this->insideOfGroupNode = false;
}

DataType Socket::getDataType() const {
    return this->datatype;
}

int Socket::isInputSocket() const { return false; }
int Socket::isOutputSocket() const { return false; }
const int Socket::isConnected() const {return false;}
void Socket::setNode(NodeBase *node) {this->node = node;}
NodeBase* Socket::getNode() const {return this->node;}

DataType Socket::getActualDataType() const {return this->actualType;}
void Socket::setActualDataType(DataType actualType) {
    if (actualType != COM_DT_VALUE && actualType != COM_DT_VECTOR && actualType != COM_DT_COLOR) {
        printf("WARNING: setting incorrect data type to socket");
    }
    this->actualType = actualType;
}
