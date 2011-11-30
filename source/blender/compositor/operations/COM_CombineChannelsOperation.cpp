#include "COM_CombineChannelsOperation.h"
#include <stdio.h>

CombineChannelsOperation::CombineChannelsOperation() : NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
    this->setResolutionInputSocketIndex(0);
    this->inputChannel1Operation = NULL;
    this->inputChannel2Operation = NULL;
    this->inputChannel3Operation = NULL;
    this->inputChannel4Operation = NULL;
}
void CombineChannelsOperation::initExecution() {
	this->inputChannel1Operation = this->getInputSocketReader(0);
	this->inputChannel2Operation = this->getInputSocketReader(1);
	this->inputChannel3Operation = this->getInputSocketReader(2);
	this->inputChannel4Operation = this->getInputSocketReader(3);
}

void CombineChannelsOperation::deinitExecution() {
    this->inputChannel1Operation = NULL;
    this->inputChannel2Operation = NULL;
    this->inputChannel3Operation = NULL;
    this->inputChannel4Operation = NULL;
}


void CombineChannelsOperation::executePixel(float *color,float x, float y, MemoryBuffer *inputBuffers[]) {
    float input[4];
    /// @todo: remove if statements
    if (this->inputChannel1Operation) {
		this->inputChannel1Operation->read(input, x, y, inputBuffers);
        color[0] = input[0];
    }
    if (this->inputChannel2Operation) {
		this->inputChannel2Operation->read(input, x, y, inputBuffers);
        color[1] = input[0];
    }
    if (this->inputChannel3Operation) {
		this->inputChannel3Operation->read(input, x, y, inputBuffers);
        color[2] = input[0];
    }
    if (this->inputChannel4Operation) {
		this->inputChannel4Operation->read(input, x, y, inputBuffers);
        color[3] = input[0];
    }
}
