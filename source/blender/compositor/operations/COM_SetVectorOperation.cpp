#include "COM_SetVectorOperation.h"
#include "COM_OutputSocket.h"
#include "COM_defines.h"

SetVectorOperation::SetVectorOperation(): NodeOperation() {
    this->addOutputSocket(*(new OutputSocket(COM_DT_VECTOR)));
}

void SetVectorOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    outputValue[0] = this->x;
    outputValue[1] = this->y;
    outputValue[2] = this->z;
    outputValue[3] = this->w;
}

void SetVectorOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
    if (preferredResolution[0] == 0 ||preferredResolution[1]==0) {
        resolution[0] = COM_DEFAULT_RESOLUTION_WIDTH;
        resolution[1] = COM_DEFAULT_RESOLUTION_HEIGHT;
    } else {
        resolution[0] = preferredResolution[0];
        resolution[1] = preferredResolution[1];
    }
}
