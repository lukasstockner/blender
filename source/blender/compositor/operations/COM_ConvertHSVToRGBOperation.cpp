#include "COM_ConvertHSVToRGBOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math_color.h"

ConvertHSVToRGBOperation::ConvertHSVToRGBOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
    this->inputOperation = NULL;
}

void ConvertHSVToRGBOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertHSVToRGBOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor[4];
	inputOperation->read(inputColor, x, y, inputBuffers);
    hsv_to_rgb(inputColor[0], inputColor[1], inputColor[2], &outputValue[0], &outputValue[1], &outputValue[2]);
    outputValue[3] = inputColor[3];
}

void ConvertHSVToRGBOperation::deinitExecution() {
    this->inputOperation = NULL;
}

