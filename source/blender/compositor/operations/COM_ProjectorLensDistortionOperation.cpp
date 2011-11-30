#include "COM_ProjectorLensDistortionOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

ProjectorLensDistortionOperation::ProjectorLensDistortionOperation(): NodeOperation() {
	this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
	this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
	this->setComplex(true);
    this->inputProgram = NULL;
}
void ProjectorLensDistortionOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
	kr = 0.25f*MAX2(MIN2(this->dispersion, 1.f), 0.f);
	kr2 = kr * 20;
}

void* ProjectorLensDistortionOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) {
	void* buffer = inputProgram->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void ProjectorLensDistortionOperation::executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data) {
	float inputValue[4];
	const float height = this->getHeight();
	const float width = this->getWidth();
	const float v = (y + 0.5f)/height;
	const float u = (x + 0.5f)/width;
	MemoryBuffer * inputBuffer = (MemoryBuffer*)data;
	inputBuffer->readCubic(inputValue, (u*width + kr2) - 0.5f, v*height - 0.5f);
	color[0] = inputValue[0];
	inputBuffer->read(inputValue, x, y);
	color[1] = inputValue[1];
	inputBuffer->readCubic(inputValue, (u*width - kr2) - 0.5f, v*height - 0.5f);
	color[2] = inputValue[2];
	color[3] = 1.0f;
}

void ProjectorLensDistortionOperation::deinitExecution() {
	this->inputProgram = NULL;
}

bool ProjectorLensDistortionOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;
	newInput.ymax = input->ymax;
	newInput.ymin = input->ymin;
	newInput.xmin = input->xmin-kr2-1;
	newInput.xmax = input->xmax+kr2+1;
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
