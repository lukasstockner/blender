#include "COM_ReadBufferOperation.h"
#include "COM_WriteBufferOperation.h"
#include "COM_defines.h"

ReadBufferOperation::ReadBufferOperation():NodeOperation() {
    this->addOutputSocket(COM_DT_COLOR);
    this->offset = 0;
    this->readmode = COM_RM_NORMAL;
}

void* ReadBufferOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) {
	return getInputMemoryBuffer(memoryBuffers);
}

void ReadBufferOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
    if (this->memoryProxy != NULL) {
        memoryProxy->getWriteBufferOperation()->determineResolution(resolution, preferredResolution);
		/// @todo: may not occur!, but does with blur node
		if (memoryProxy->getExecutor()) memoryProxy->getExecutor()->setResolution(resolution);
    }
}
void ReadBufferOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
    MemoryBuffer *inputBuffer = inputBuffers[this->offset];
	if (inputBuffer) {
        if (readmode == COM_RM_NORMAL) {
            inputBuffer->read(color, x, y);
        } else {
            inputBuffer->readCubic(color, x, y);
        }
    }
}

bool ReadBufferOperation::determineDependingAreaOfInterest(rcti * input, ReadBufferOperation* readOperation, rcti* output) {
    if (this==readOperation) {
        BLI_init_rcti(output, input->xmin, input->xmax, input->ymin, input->ymax);
        return true;
    }
    return false;
}
