#include "COM_LensGlowImageOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"

LensGlowImageOperation::LensGlowImageOperation(): NodeOperation() {
    this->addOutputSocket(COM_DT_COLOR);
}
void LensGlowImageOperation::initExecution() {
	this->scale = 1/20000.0f;
}
void LensGlowImageOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
	const float cs_r = 1.f, cs_g = 1.f, cs_b = 1.f;
	const float v = 2.f*(y / (float)512.0f) - 1.f;
	const float u = 2.f*(x / (float)512.0f) - 1.f;
	const float r = (u*u + v*v)*scale;
	const float d = -sqrtf(sqrtf(sqrtf(r)))*9.f;
	const float w = (0.5f + 0.5f*cos((double)u*M_PI))*(0.5f + 0.5f*cos((double)v*M_PI));
	color[0] = expf(d*cs_r) * w;
	color[1] = expf(d*cs_g) * w;
	color[2] = expf(d*cs_b) * w;
	color[3] = 1.0f;
}

void LensGlowImageOperation::deinitExecution() {
}

void LensGlowImageOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
	resolution[0] = 512;
	resolution[1] = 512;
}
