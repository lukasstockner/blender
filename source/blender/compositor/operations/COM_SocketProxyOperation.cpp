#include "COM_SocketProxyOperation.h"

SocketProxyOperation::SocketProxyOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_COLOR/*|COM_DT_VECTOR|COM_DT_VALUE*/);
    this->addOutputSocket(COM_DT_COLOR);
    this->inputOperation = NULL;
}

void SocketProxyOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void SocketProxyOperation::deinitExecution() {
    this->inputOperation = NULL;
}

void SocketProxyOperation::executePixel(float *color,float x, float y, MemoryBuffer *inputBuffers[]) {
	this->inputOperation->read(color, x, y, inputBuffers);
}
