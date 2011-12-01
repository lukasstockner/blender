#include "COM_RenderLayersAlphaProg.h"

RenderLayersAlphaProg::RenderLayersAlphaProg() :RenderLayersBaseProg(SCE_PASS_COMBINED, 4) {
    this->addOutputSocket(COM_DT_VALUE);
}

void RenderLayersAlphaProg::executePixel(float* output, float x, float y, MemoryBuffer *inputBuffers[]) {
	int ix = x;
	int iy = y;
	float * inputBuffer = this->getInputBuffer();

	if (inputBuffer == NULL || ix < 0 || iy < 0 || ix >= (int)this->getWidth() || iy >= (int)this->getHeight() ) {
		output[0] = 0.0f;
		output[1] = 0.0f;
		output[2] = 0.0f;
		output[3] = 0.0f;
	} else {
		unsigned int offset = (iy*this->getWidth()+ix) * 4;
		output[0] = inputBuffer[offset+3];
		output[1] = 0.0f;
		output[2] = 0.0f;
		output[3] = 0.0f;
	}
}
