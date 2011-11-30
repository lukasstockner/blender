#include "COM_MixScreenOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

MixScreenOperation::MixScreenOperation(): MixBaseOperation() {
}

void MixScreenOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
	float inputColor1[4];
	float inputColor2[4];
	float valuev[4];

	inputValueOperation->read(valuev, x, y, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, inputBuffers);

	float value = valuev[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem= 1.0f-value;

	outputValue[0] = 1.0f - (valuem + value*(1.0f-inputColor2[0])) *(1.0f-inputColor1[0]);
	outputValue[1] = 1.0f - (valuem + value*(1.0f-inputColor2[1])) *(1.0f-inputColor1[1]);
	outputValue[2] = 1.0f - (valuem + value*(1.0f-inputColor2[2])) *(1.0f-inputColor1[2]);
	outputValue[3] = inputColor1[3];
}

