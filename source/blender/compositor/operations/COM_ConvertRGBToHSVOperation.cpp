#include "COM_ConvertRGBToHSVOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math_color.h"

ConvertRGBToHSVOperation::ConvertRGBToHSVOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
    this->inputOperation = NULL;
}

void ConvertRGBToHSVOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertRGBToHSVOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor[4];
	inputOperation->read(inputColor, x, y, inputBuffers);
    rgb_to_hsv(inputColor[0], inputColor[1], inputColor[2], &outputValue[0], &outputValue[1], &outputValue[2]);
    outputValue[3] = inputColor[3];
}

void ConvertRGBToHSVOperation::deinitExecution() {
    this->inputOperation = NULL;
}
