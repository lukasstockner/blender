#include "COM_MixHueOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

MixHueOperation::MixHueOperation(): MixBaseOperation() {
}

void MixHueOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];
    float inputColor2[4];
    float value;

	inputValueOperation->read(&value, x, y, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, inputBuffers);

    if (this->useValueAlphaMultiply()) {
        value *= inputColor2[3];
    }
    outputValue[0] = inputColor1[0]+value*(inputColor2[0]);
    outputValue[1] = inputColor1[1]+value*(inputColor2[1]);
    outputValue[2] = inputColor1[2]+value*(inputColor2[2]);
    outputValue[3] = inputColor1[3];
}

