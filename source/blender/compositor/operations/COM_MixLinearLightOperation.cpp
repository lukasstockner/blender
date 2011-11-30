#include "COM_MixLinearLightOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

MixLinearLightOperation::MixLinearLightOperation(): MixBaseOperation() {
}

void MixLinearLightOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];
    float inputColor2[4];
    float value;

	inputValueOperation->read(&value, x, y, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, inputBuffers);

    if (this->useValueAlphaMultiply()) {
        value *= inputColor2[3];
    }
    if (inputColor2[0] > 0.5f)
            outputValue[0] = inputColor1[0] + value*(2.0f*(inputColor2[0]-0.5f));
    else
            outputValue[0] = inputColor1[0] + value*(2.0f*(inputColor2[0]) - 1.0f);
    if (inputColor2[1] > 0.5f)
            outputValue[1] = inputColor1[1] + value*(2.0f*(inputColor2[1]-0.5f));
    else
            outputValue[1] = inputColor1[1] + value*(2.0f*(inputColor2[1]) - 1.0f);
    if (inputColor2[2] > 0.5f)
            outputValue[2] = inputColor1[2] + value*(2.0f*(inputColor2[2]-0.5f));
    else
            outputValue[2] = inputColor1[2] + value*(2.0f*(inputColor2[2]) - 1.0f);

    outputValue[3] = inputColor1[3];
}

