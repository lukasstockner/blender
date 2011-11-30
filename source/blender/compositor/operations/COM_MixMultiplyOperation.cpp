#include "COM_MixMultiplyOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

MixMultiplyOperation::MixMultiplyOperation(): MixBaseOperation() {
}

void MixMultiplyOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];
    float inputColor2[4];
    float inputValue[4];

	inputValueOperation->read(inputValue, x, y, inputBuffers);
	inputColor1Operation->read(inputColor1, x, y, inputBuffers);
	inputColor2Operation->read(inputColor2, x, y, inputBuffers);

    float value = inputValue[0];
    if (this->useValueAlphaMultiply()) {
        value *= inputColor2[3];
    }
    float valuem= 1.0f-value;
    outputValue[0] = inputColor1[0] *(valuem+value*inputColor2[0]);
    outputValue[1] = inputColor1[1] *(valuem+value*inputColor2[1]);
    outputValue[2] = inputColor1[2] *(valuem+value*inputColor2[2]);
    outputValue[3] = inputColor1[3];
}

