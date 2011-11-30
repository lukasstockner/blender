#include "COM_MixBlendOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

MixBlendOperation::MixBlendOperation(): MixBaseOperation() {
}

void MixBlendOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];
    float inputColor2[4];
    float inputValue[4];
    float value;

	inputValueOperation->read(inputValue, x, y, inputBuffers);
	inputColor1Operation->read(inputColor1, x, y, inputBuffers);
	inputColor2Operation->read(inputColor2, x, y, inputBuffers);
    value = inputValue[0];

    if (this->useValueAlphaMultiply()) {
        value *= inputColor2[3];
    }
    float valuem= 1.0f-value;
    outputValue[0] = valuem*(inputColor1[0])+value*(inputColor2[0]);
    outputValue[1] = valuem*(inputColor1[1])+value*(inputColor2[1]);
    outputValue[2] = valuem*(inputColor1[2])+value*(inputColor2[2]);
    outputValue[3] = inputColor1[3];
}
