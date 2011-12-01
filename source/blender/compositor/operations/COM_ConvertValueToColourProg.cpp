#include "COM_ConvertValueToColourProg.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

ConvertValueToColourProg::ConvertValueToColourProg(): NodeOperation() {
    this->addInputSocket(COM_DT_VALUE);
    this->addOutputSocket(COM_DT_COLOR);
    this->inputProgram = NULL;
}
void ConvertValueToColourProg::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
}

void ConvertValueToColourProg::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputValue[4];
	this->inputProgram->read(inputValue, x, y, inputBuffers);
    color[0] = inputValue[0];
    color[1] = inputValue[0];
    color[2] = inputValue[0];
    color[3] = 1.0f;
}

void ConvertValueToColourProg::deinitExecution() {
    this->inputProgram = NULL;
}
