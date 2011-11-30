#include "COM_SocketConnection.h"
#include "COM_NodeOperation.h"

SocketConnection::SocketConnection() {
	this->fromSocket = NULL;
	this->toSocket = NULL;
	this->setIgnoreResizeCheck(false);
}

void SocketConnection::setFromSocket(OutputSocket* fromsocket){
	if (fromsocket == NULL) {
		throw "ERROR";
	}
	this->fromSocket = fromsocket;
}

OutputSocket* SocketConnection::getFromSocket() const {return this->fromSocket;}
void SocketConnection::setToSocket(InputSocket* tosocket) {
	if (tosocket == NULL) {
		throw "ERROR";
	}
	this->toSocket = tosocket;
}

InputSocket* SocketConnection::getToSocket() const {return this->toSocket;}

NodeBase* SocketConnection::getFromNode() const {
	if (this->getFromSocket() == NULL) {
		return NULL;
	} else {
		return this->getFromSocket()->getNode();
	}
}
NodeBase* SocketConnection::getToNode() const {
	if (this->getToSocket() == NULL) {
		return NULL;
	} else {
		return this->getToSocket()->getNode();
	}
}
bool SocketConnection::isValid() const {
	if ((this->getToSocket() != NULL && this->getFromSocket() != NULL)) {
		if (this->getFromNode()->isOperation() && this->getToNode()->isOperation()) {
			return true;
		}
	}
	return false;
}

bool SocketConnection::needsResolutionConversion() const {
	if (this->ignoreResizeCheck) {return false;}
	NodeOperation* fromOperation = (NodeOperation*)this->getFromNode();
	NodeOperation* toOperation = (NodeOperation*)this->getToNode();
	if (this->toSocket->getResizeMode() == COM_SC_NO_RESIZE) {return false;}
	const unsigned int fromWidth = fromOperation->getWidth();
	const unsigned int fromHeight = fromOperation->getHeight();
	const unsigned int toWidth = toOperation->getWidth();
	const unsigned int toHeight = toOperation->getHeight();

	if (fromWidth == toWidth && fromHeight == toHeight) {
		return false;
	}
	return true;
}
