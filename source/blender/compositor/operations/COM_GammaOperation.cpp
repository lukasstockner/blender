#include "COM_GammaOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"

GammaOperation::GammaOperation(): NodeOperation() {
    this->addInputSocket(COM_DT_COLOR);
    this->addInputSocket(COM_DT_VALUE);
    this->addOutputSocket(COM_DT_COLOR);
    this->inputProgram = NULL;
    this->inputGammaProgram = NULL;
}
void GammaOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
	this->inputGammaProgram = this->getInputSocketReader(1);
}

void GammaOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputValue[4];
    float inputGamma[4];

	this->inputProgram->read(inputValue, x, y, inputBuffers);
	this->inputGammaProgram->read(inputGamma, x, y, inputBuffers);
    const float gamma = inputGamma[0];
        /* check for negative to avoid nan's */
    color[0] = inputValue[0]>0.0f?pow(inputValue[0], gamma):inputValue[0];
    color[1] = inputValue[1]>0.0f?pow(inputValue[1], gamma):inputValue[1];
    color[2] = inputValue[2]>0.0f?pow(inputValue[2], gamma):inputValue[2];

        color[3] = inputValue[3];
}

void GammaOperation::deinitExecution() {
    this->inputProgram = NULL;
    this->inputGammaProgram = NULL;
}
