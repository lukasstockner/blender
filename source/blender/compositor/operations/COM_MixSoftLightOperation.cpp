#include "COM_MixSoftLightOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

MixSoftLightOperation::MixSoftLightOperation(): MixBaseOperation() {
}

void MixSoftLightOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];
    float inputColor2[4];
    float value;

	inputValueOperation->read(&value, x, y, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, inputBuffers);

    if (this->useValueAlphaMultiply()) {
        value *= inputColor2[3];
    }
    float valuem = 1.0f-value;
    float scr, scg, scb;

    /* first calculate non-fac based Screen mix */
    scr = 1.0f - (1.0f - inputColor2[0]) * (1.0f - inputColor1[0]);
    scg = 1.0f - (1.0f - inputColor2[1]) * (1.0f - inputColor1[1]);
    scb = 1.0f - (1.0f - inputColor2[2]) * (1.0f - inputColor1[2]);

    outputValue[0] = valuem*(inputColor1[0]) + value*(((1.0f - inputColor1[0]) * inputColor2[0] * (inputColor1[0])) + (inputColor1[0] * scr));
    outputValue[1] = valuem*(inputColor1[1]) + value*(((1.0f - inputColor1[1]) * inputColor2[1] * (inputColor1[1])) + (inputColor1[1] * scg));
    outputValue[2] = valuem*(inputColor1[2]) + value*(((1.0f - inputColor1[2]) * inputColor2[2] * (inputColor1[2])) + (inputColor1[2] * scb));
    outputValue[3] = inputColor1[3];
}

