#include "COM_TranslateOperation.h"

TranslateOperation::TranslateOperation() : NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
    this->setResolutionInputSocketIndex(0);
    this->inputOperation = NULL;
    this->inputXOperation = NULL;
    this->inputYOperation = NULL;
}
void TranslateOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
	this->inputXOperation = this->getInputSocketReader(1);
	this->inputYOperation = this->getInputSocketReader(2);

	float tempDelta[4];
	this->inputXOperation->read(tempDelta, 0, 0, NULL);
	this->deltaX = tempDelta[0];
	this->inputYOperation->read(tempDelta, 0, 0, NULL);
	this->deltaY = tempDelta[0];
}

void TranslateOperation::deinitExecution() {
    this->inputOperation = NULL;
    this->inputXOperation = NULL;
    this->inputYOperation = NULL;
}


void TranslateOperation::executePixel(float *color,float x, float y, MemoryBuffer *inputBuffers[]) {
	this->inputOperation->read(color, x-this->getDeltaX(), y-this->getDeltaY(), inputBuffers);
}

bool TranslateOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
    rcti newInput;

    newInput.xmax = input->xmax - this->getDeltaX();
    newInput.xmin = input->xmin - this->getDeltaX();
    newInput.ymax = input->ymax - this->getDeltaY();
    newInput.ymin = input->ymin - this->getDeltaY();

    return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
