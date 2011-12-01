#include "COM_SetValueOperation.h"
#include "COM_OutputSocket.h"

SetValueOperation::SetValueOperation(): NodeOperation() {
    this->addOutputSocket(COM_DT_VALUE);
}

void SetValueOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    outputValue[0] = this->value;
}

void SetValueOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
    resolution[0] = preferredResolution[0];
    resolution[1] = preferredResolution[1];
}
