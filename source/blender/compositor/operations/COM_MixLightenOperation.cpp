#include "COM_MixLightenOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

MixLightenOperation::MixLightenOperation(): MixBaseOperation() {
}

void MixLightenOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];
    float inputColor2[4];
    float value;

	inputValueOperation->read(&value, x, y, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, inputBuffers);

    if (this->useValueAlphaMultiply()) {
        value *= inputColor2[3];
    }
    float tmp;
    tmp=value * inputColor2[0];
    if(tmp > inputColor1[0]) outputValue[0]= tmp;
    else outputValue[0] = inputColor1[0];
    tmp=value * inputColor2[1];
    if(tmp > inputColor1[1]) outputValue[1]= tmp;
    else outputValue[1] = inputColor1[1];
    tmp=value * inputColor2[2];
    if(tmp > inputColor1[2]) outputValue[2]= tmp;
    else outputValue[2] = inputColor1[2];
    outputValue[3] = inputColor1[3];
}

