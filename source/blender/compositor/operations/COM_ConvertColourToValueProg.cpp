#include "COM_ConvertColourToValueProg.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

ConvertColourToValueProg::ConvertColourToValueProg(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
    this->inputOperation = NULL;
}

void ConvertColourToValueProg::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertColourToValueProg::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor[4];
	inputOperation->read(&inputColor[0], x, y, inputBuffers);
    outputValue[0] = ((inputColor[0] + inputColor[1] + inputColor[2])/3.0f)*inputColor[3];
}

void ConvertColourToValueProg::deinitExecution() {
    this->inputOperation = NULL;
}
