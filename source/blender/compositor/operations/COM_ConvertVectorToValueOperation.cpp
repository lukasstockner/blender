#include "COM_ConvertVectorToValueOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

ConvertVectorToValueOperation::ConvertVectorToValueOperation(): NodeOperation() {
    this->addInputSocket(COM_DT_VECTOR);
    this->addOutputSocket(COM_DT_VALUE);
    this->inputOperation = NULL;
}

void ConvertVectorToValueOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertVectorToValueOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float input[4];
	inputOperation->read(input, x, y, inputBuffers);
    outputValue[0] = (input[0]+input[1]+input[2])/3.0f;
}

void ConvertVectorToValueOperation::deinitExecution() {
    this->inputOperation = NULL;
}
