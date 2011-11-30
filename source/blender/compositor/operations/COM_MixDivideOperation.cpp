#include "COM_MixDivideOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

MixDivideOperation::MixDivideOperation(): MixBaseOperation() {
}

void MixDivideOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];
    float inputColor2[4];
    float value;

	inputValueOperation->read(&value, x, y, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, inputBuffers);

    if (this->useValueAlphaMultiply()) {
        value *= inputColor2[3];
    }
    float valuem= 1.0f-value;

    if(inputColor2[0]!=0.0f)
            outputValue[0] = valuem*(inputColor1[0]) + value*(inputColor1[0])/inputColor2[0];
    if(inputColor2[1]!=0.0f)
            outputValue[1] = valuem*(inputColor1[1]) + value*(inputColor1[1])/inputColor2[1];
    if(inputColor2[2]!=0.0f)
            outputValue[2] = valuem*(inputColor1[2]) + value*(inputColor1[2])/inputColor2[2];

    outputValue[3] = inputColor1[3];
}

