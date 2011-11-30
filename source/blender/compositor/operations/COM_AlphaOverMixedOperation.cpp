#include "COM_AlphaOverMixedOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

AlphaOverMixedOperation::AlphaOverMixedOperation(): MixBaseOperation() {
    this->x = 0.0f;
}

void AlphaOverMixedOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor1[4];
    float inputOverColor[4];
    float value[4];

	inputValueOperation->read(value, x, y, inputBuffers);
	inputColor1Operation->read(inputColor1, x, y, inputBuffers);
	inputColor2Operation->read(inputOverColor, x, y, inputBuffers);

    if(inputOverColor[3]<=0.0f) {
        outputValue[0] = inputColor1[0];
        outputValue[1] = inputColor1[1];
        outputValue[2] = inputColor1[2];
        outputValue[3] = inputColor1[3];
    }
    else if(value[0]==1.0f && inputOverColor[3]>=1.0f) {
        outputValue[0] = inputOverColor[0];
        outputValue[1] = inputOverColor[1];
        outputValue[2] = inputOverColor[2];
        outputValue[3] = inputOverColor[3];
    }
    else {
        float addfac= 1.0f - this->x + inputOverColor[3]*this->x;
        float premul= value[0]*addfac;
        float mul= 1.0f - value[0]*inputOverColor[3];

        outputValue[0]= (mul*inputColor1[0]) + premul*inputOverColor[0];
        outputValue[1]= (mul*inputColor1[1]) + premul*inputOverColor[1];
        outputValue[2]= (mul*inputColor1[2]) + premul*inputOverColor[2];
        outputValue[3]= (mul*inputColor1[3]) + value[0]*inputOverColor[3];
    }
}

