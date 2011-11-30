#include "COM_LensGlowOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"

LensGlowOperation::LensGlowOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
    this->inputProgram = NULL;
	this->lamp = NULL;
}
void LensGlowOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
}

void LensGlowOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
//	const float emit100 = this->lamp->energy*100;
//	const float emit200 = emit100*2;
//	const float deltaX = 160-x;
//	const float deltaY = 100-y;
//	const float distance = deltaX * deltaX + deltaY*deltaY;

//	float glow = (emit100-(distance))/(emit200);
//	if (glow<0) glow=0;

//	color[0] = glow*lamp->r;
//	color[1] = glow*lamp->g;
//	color[2] = glow*lamp->b;
//	color[3] = 1.0f;
}

void LensGlowOperation::deinitExecution() {
    this->inputProgram = NULL;
}
