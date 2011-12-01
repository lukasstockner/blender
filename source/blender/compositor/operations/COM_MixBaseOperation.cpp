#include "COM_MixBaseOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

MixBaseOperation::MixBaseOperation(): NodeOperation() {
    this->addInputSocket(COM_DT_VALUE);
    this->addInputSocket(COM_DT_COLOR);
    this->addInputSocket(COM_DT_COLOR);
    this->addOutputSocket(COM_DT_COLOR);
    this->inputValueOperation = NULL;
    this->inputColor1Operation = NULL;
    this->inputColor2Operation = NULL;
    this->setUseValueAlphaMultiply(false);
}

void MixBaseOperation::initExecution() {
	this->inputValueOperation = this->getInputSocketReader(0);
	this->inputColor1Operation = this->getInputSocketReader(1);
	this->inputColor2Operation = this->getInputSocketReader(2);
}

void MixBaseOperation::executePixel(float* outputColor, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];
    float inputColor2[4];
    float value;

	inputValueOperation->read(&value, x, y, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, inputBuffers);

    float valuem= 1.0f-value;
    outputColor[0] = valuem*(inputColor1[0])+value*(inputColor2[0]);
    outputColor[1] = valuem*(inputColor1[1])+value*(inputColor2[1]);
    outputColor[2] = valuem*(inputColor1[2])+value*(inputColor2[2]);
    outputColor[3] = valuem*(inputColor1[3])+value*(inputColor2[0]);
}

void MixBaseOperation::deinitExecution() {
    this->inputValueOperation = NULL;
    this->inputColor1Operation = NULL;
    this->inputColor2Operation = NULL;
}

