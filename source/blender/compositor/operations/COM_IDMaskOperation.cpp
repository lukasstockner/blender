#include "COM_IDMaskOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

IDMaskOperation::IDMaskOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
    this->inputProgram = NULL;
}
void IDMaskOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
}

void IDMaskOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputValue[4];

	this->inputProgram->read(inputValue, x, y, inputBuffers);
    const float a = (inputValue[0] == this->objectIndex)?1.0f:0.0f;
    color[0] = a;
}

void IDMaskOperation::deinitExecution() {
    this->inputProgram = NULL;
}

