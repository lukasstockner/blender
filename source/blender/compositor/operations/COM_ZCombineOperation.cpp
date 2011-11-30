#include "COM_ZCombineOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_utildefines.h"

ZCombineOperation::ZCombineOperation(): NodeOperation() {
	this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
	this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
	this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
	this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
	this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));

	this->image1Reader = NULL;
	this->depth1Reader = NULL;
	this->image2Reader = NULL;
	this->depth2Reader = NULL;

}

void ZCombineOperation::initExecution() {
	this->image1Reader = this->getInputSocketReader(0);
	this->depth1Reader = this->getInputSocketReader(1);
	this->image2Reader = this->getInputSocketReader(2);
	this->depth2Reader = this->getInputSocketReader(3);
}

void ZCombineOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
	float depth1[4];
	float depth2[4];

	this->depth1Reader->read(depth1, x, y, inputBuffers);
	this->depth2Reader->read(depth2, x, y, inputBuffers);
	if (depth1[0]<depth2[0]) {
		this->image1Reader->read(color, x, y, inputBuffers);
	} else {
		this->image2Reader->read(color, x, y, inputBuffers);
	}
}
void ZCombineAlphaOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
	float depth1[4];
	float depth2[4];
	float color1[4];
	float color2[4];

	this->depth1Reader->read(depth1, x, y, inputBuffers);
	this->depth2Reader->read(depth2, x, y, inputBuffers);
	if (depth1[0]<depth2[0]) {
		this->image1Reader->read(color1, x, y, inputBuffers);
		this->image2Reader->read(color2, x, y, inputBuffers);
	} else {
		this->image1Reader->read(color2, x, y, inputBuffers);
		this->image2Reader->read(color1, x, y, inputBuffers);
	}
	float fac = color1[3];
	float ifac = 1.0f-fac;
	color[0] = color1[0]+ifac*color2[0];
	color[1] = color1[1]+ifac*color2[1];
	color[2] = color1[2]+ifac*color2[2];
	color[3] = MAX2(color1[3], color2[3]);
}

void ZCombineOperation::deinitExecution() {
	this->image1Reader = NULL;
	this->depth1Reader = NULL;
	this->image2Reader = NULL;
	this->depth2Reader = NULL;
}
