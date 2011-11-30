#include "COM_GaussianYBlurOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

GaussianYBlurOperation::GaussianYBlurOperation(): BlurBaseOperation() {
	this->gausstab = NULL;
	this->rad = 0;

}
void GaussianYBlurOperation::initExecution() {
	BlurBaseOperation::initExecution();

	float rad = size*this->data->sizey;
	if(rad<1)
		rad= 1;

	this->rad = rad;
	this->gausstab = BlurBaseOperation::make_gausstab(rad);
}

void* GaussianYBlurOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) {
        void* buffer = getInputOperation(0)->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void GaussianYBlurOperation::executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void *data) {

	float tempColor[4];
	tempColor[0] = 0;
	tempColor[1] = 0;
	tempColor[2] = 0;
	tempColor[3] = 0;
	float overallmultiplyer = 0;
	MemoryBuffer* inputBuffer = (MemoryBuffer*)data;
	float* buffer = inputBuffer->getBuffer();
	int bufferwidth = inputBuffer->getWidth();
	int bufferstartx = inputBuffer->getRect()->xmin;
	int bufferstarty = inputBuffer->getRect()->ymin;

	int miny = y - this->rad;
	int maxy = y + this->rad;
	int minx = x;
	int maxx = x;
	miny = max(miny, inputBuffer->getRect()->ymin);
	minx = max(minx, inputBuffer->getRect()->xmin);
	maxy = min(maxy, inputBuffer->getRect()->ymax);
	maxx = min(maxx, inputBuffer->getRect()->xmax);

	int step = getStep();
	int index = 0;
	for (int ny = miny ; ny < maxy ; ny +=step) {
		int bufferindex = ((minx - bufferstartx)*4)+((ny-bufferstarty)*4*bufferwidth);
		float multiplyer = gausstab[index++];
		tempColor[0] += multiplyer * buffer[bufferindex];
		tempColor[1] += multiplyer * buffer[bufferindex+1];
		tempColor[2] += multiplyer * buffer[bufferindex+2];
		tempColor[3] += multiplyer * buffer[bufferindex+3];
		overallmultiplyer += multiplyer;
	}
	float divider = 1.0/overallmultiplyer;
	color[0] = tempColor[0]*divider;
	color[1] = tempColor[1]*divider;
	color[2] = tempColor[2]*divider;
	color[3] = tempColor[3]*divider;
}

void GaussianYBlurOperation::deinitExecution() {
	BlurBaseOperation::deinitExecution();
	delete this->gausstab;
	this->gausstab = NULL;
}

bool GaussianYBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;
	newInput.xmax = input->xmax;
	newInput.xmin = input->xmin;
	newInput.ymax = input->ymax + rad;
	newInput.ymin = input->ymin - rad;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
