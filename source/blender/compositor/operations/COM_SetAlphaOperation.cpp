#include "COM_SetAlphaOperation.h"
#include "COM_OutputSocket.h"

SetAlphaOperation::SetAlphaOperation(): NodeOperation() {
    this->addInputSocket(COM_DT_COLOR);
    this->addInputSocket(COM_DT_VALUE);
    this->addOutputSocket(COM_DT_COLOR);

    this->inputColor = NULL;
    this->inputAlpha = NULL;
}

void SetAlphaOperation::initExecution() {
	this->inputColor = getInputSocketReader(0);
	this->inputAlpha = getInputSocketReader(1);
}

void SetAlphaOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float alphaInput[4];

	this->inputColor->read(outputValue, x, y, inputBuffers);
	this->inputAlpha->read(alphaInput, x, y, inputBuffers);

    outputValue[3] = alphaInput[0];
}

void SetAlphaOperation::deinitExecution() {
    this->inputColor = NULL;
    this->inputAlpha = NULL;
}
