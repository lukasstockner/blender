#include "COM_ConvertValueToVectorOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

ConvertValueToVectorOperation::ConvertValueToVectorOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_VECTOR)));
    this->inputOperation = NULL;
}

void ConvertValueToVectorOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertValueToVectorOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float input[4];
	inputOperation->read(input, x, y, inputBuffers);
    outputValue[0] = input[0];
    outputValue[1] = input[0];
    outputValue[2] = input[0];
    outputValue[3] = 0.0f;
}

void ConvertValueToVectorOperation::deinitExecution() {
    this->inputOperation = NULL;
}
