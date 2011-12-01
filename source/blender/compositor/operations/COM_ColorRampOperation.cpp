#include "COM_ColorRampOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

#ifdef __cplusplus
extern "C" {
#endif
	#include "BKE_texture.h"
#ifdef __cplusplus
}
#endif

ColorRampOperation::ColorRampOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);

	this->inputProgram = NULL;
	this->colorBand = NULL;
}
void ColorRampOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
}

void ColorRampOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
	float values[4];

	this->inputProgram->read(values, x, y, inputBuffers);
	do_colorband(this->colorBand, values[0], color);
}

void ColorRampOperation::deinitExecution() {
	this->inputProgram = NULL;
}
