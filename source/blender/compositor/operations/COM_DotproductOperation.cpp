#include "COM_DotproductOperation.h"

DotproductOperation::DotproductOperation() : NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_VECTOR)));
    this->addInputSocket(*(new InputSocket(COM_DT_VECTOR)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
    this->setResolutionInputSocketIndex(0);
    this->input1Operation = NULL;
    this->input2Operation = NULL;
}
void DotproductOperation::initExecution() {
	this->input1Operation = this->getInputSocketReader(0);
	this->input2Operation = this->getInputSocketReader(1);
}

void DotproductOperation::deinitExecution() {
    this->input1Operation = NULL;
    this->input2Operation = NULL;
}

/** @todo: current implementation is the inverse of a dotproduct. not 'logically' correct
  */
void DotproductOperation::executePixel(float *color,float x, float y, MemoryBuffer *inputBuffers[]) {
    float input1[4];
    float input2[4];
	this->input1Operation->read(input1, x, y, inputBuffers);
	this->input2Operation->read(input2, x, y, inputBuffers);
    color[0] = -(input1[0]*input2[0]+input1[1]*input2[1]+input1[2]*input2[2]);
}
