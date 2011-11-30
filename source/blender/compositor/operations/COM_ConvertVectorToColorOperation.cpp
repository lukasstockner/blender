#include "COM_ConvertVectorToColorOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

ConvertVectorToColorOperation::ConvertVectorToColorOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_VECTOR)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
    this->inputOperation = NULL;
}

void ConvertVectorToColorOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertVectorToColorOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
	inputOperation->read(outputValue, x, y, inputBuffers);
    outputValue[3] = 1.0f;
}

void ConvertVectorToColorOperation::deinitExecution() {
    this->inputOperation = NULL;
}
