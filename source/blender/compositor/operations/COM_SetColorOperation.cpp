#include "COM_SetColorOperation.h"
#include "COM_OutputSocket.h"

SetColorOperation::SetColorOperation(): NodeOperation() {
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
}

void SetColorOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    outputValue[0] = this->channel1;
    outputValue[1] = this->channel2;
    outputValue[2] = this->channel3;
    outputValue[3] = this->channel4;
}

void SetColorOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
    resolution[0] = preferredResolution[0];
    resolution[1] = preferredResolution[1];
}
