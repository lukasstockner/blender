#include "COM_ConvertColorToVectorOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

ConvertColorToVectorOperation::ConvertColorToVectorOperation(): NodeOperation() {
    this->addInputSocket(COM_DT_COLOR);
    this->addOutputSocket(COM_DT_VECTOR);
    this->inputOperation = NULL;
}

void ConvertColorToVectorOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertColorToVectorOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
	inputOperation->read(outputValue, x, y, inputBuffers);
}

void ConvertColorToVectorOperation::deinitExecution() {
    this->inputOperation = NULL;
}
