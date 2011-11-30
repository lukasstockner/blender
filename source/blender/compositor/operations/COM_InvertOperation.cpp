#include "COM_InvertOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

InvertOperation::InvertOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
    this->inputValueProgram = NULL;
    this->inputColorProgram = NULL;
    this->color = true;
    this->alpha = false;
    setResolutionInputSocketIndex(1);
}
void InvertOperation::initExecution() {
	this->inputValueProgram = this->getInputSocketReader(0);
	this->inputColorProgram = this->getInputSocketReader(1);
}

void InvertOperation::executePixel(float* out, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputValue[4];
    float inputColor[4];
	this->inputValueProgram->read(inputValue, x, y, inputBuffers);
	this->inputColorProgram->read(inputColor, x, y, inputBuffers);

    const float value = inputValue[0];
    const float invertedValue = 1.0f - value;

    if(color) {
        out[0] = (1.0f - inputColor[0])*value + inputColor[0]*invertedValue;
        out[1] = (1.0f - inputColor[1])*value + inputColor[1]*invertedValue;
        out[2] = (1.0f - inputColor[2])*value + inputColor[2]*invertedValue;
    } else {
        out[0] = inputColor[0];
        out[1] = inputColor[1];
        out[2] = inputColor[2];
    }

    if(alpha)
            out[3] = (1.0f - inputColor[3])*value + inputColor[3]*invertedValue;
    else
            out[3] = inputColor[3];

}

void InvertOperation::deinitExecution() {
    this->inputValueProgram = NULL;
    this->inputColorProgram = NULL;
}

