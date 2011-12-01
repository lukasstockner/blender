#include "COM_FlipOperation.h"

FlipOperation::FlipOperation() : NodeOperation() {
    this->addInputSocket(COM_DT_COLOR);
    this->addOutputSocket(COM_DT_COLOR);
    this->setResolutionInputSocketIndex(0);
    this->inputOperation = NULL;
    this->flipX = true;
    this->flipY = false;
}
void FlipOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void FlipOperation::deinitExecution() {
    this->inputOperation = NULL;
}


void FlipOperation::executePixel(float *color,float x, float y, MemoryBuffer *inputBuffers[]) {
    float nx = this->flipX?this->getWidth()-1-x:x;
    float ny = this->flipY?this->getHeight()-1-y:y;

	this->inputOperation->read(color, nx, ny, inputBuffers);
}

bool FlipOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
    rcti newInput;

    if (this->flipX) {
        newInput.xmax = (this->getWidth()- 1 - input->xmin)+1;
        newInput.xmin = (this->getWidth()- 1 - input->xmax)-1;
    } else {
        newInput.xmin = input->xmin;
        newInput.xmax = input->xmax;
    }
    if (this->flipY) {
        newInput.ymax = (this->getHeight()- 1 - input->ymin)+1;
        newInput.ymin = (this->getHeight()- 1 - input->ymax)-1;
    } else {
        newInput.ymin = input->ymin;
        newInput.ymax = input->ymax;
    }

    return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
