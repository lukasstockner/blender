#include "COM_SeparateChannelOperation.h"

SeparateChannelOperation::SeparateChannelOperation() : NodeOperation() {
    this->addInputSocket(COM_DT_COLOR);
    this->addOutputSocket(COM_DT_VALUE);
    this->inputOperation = NULL;
}
void SeparateChannelOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void SeparateChannelOperation::deinitExecution() {
    this->inputOperation = NULL;
}


void SeparateChannelOperation::executePixel(float *color,float x, float y, MemoryBuffer *inputBuffers[]) {
    float input[4];
	this->inputOperation->read(input, x, y, inputBuffers);
    color[0] = input[this->channel];
}
