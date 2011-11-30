#include "COM_ChangeHSVOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

ChangeHSVOperation::ChangeHSVOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
    this->inputOperation = NULL;
}

void ChangeHSVOperation::initExecution() {
	this->inputOperation = getInputSocketReader(0);
}

void ChangeHSVOperation::deinitExecution() {
    this->inputOperation = NULL;
}

void ChangeHSVOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];

	inputOperation->read(inputColor1, x, y, inputBuffers);

    outputValue[0] = inputColor1[0] + (this->hue - 0.5f);
    if (outputValue[0]>1.0f) outputValue[0]-=1.0; else if(outputValue[0]<0.0) outputValue[0]+= 1.0;
    outputValue[1] = inputColor1[1] * this->saturation;
    outputValue[2] = inputColor1[2] * this->value;
    outputValue[3] = inputColor1[3];
}

