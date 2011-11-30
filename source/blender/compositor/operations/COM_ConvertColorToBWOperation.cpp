#include "COM_ConvertColorToBWOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

ConvertColorToBWOperation::ConvertColorToBWOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
    this->inputOperation = NULL;
}

void ConvertColorToBWOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertColorToBWOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor[4];
	inputOperation->read(&inputColor[0], x, y, inputBuffers);
    outputValue[0] = (inputColor[0]*0.35f + inputColor[1]*0.45f + inputColor[2]*0.2f)*inputColor[3];
}

void ConvertColorToBWOperation::deinitExecution() {
    this->inputOperation = NULL;
}
