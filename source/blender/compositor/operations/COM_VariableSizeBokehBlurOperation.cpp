#include "COM_VariableSizeBokehBlurOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

VariableSizeBokehBlurOperation::VariableSizeBokehBlurOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);

	this->inputProgram = NULL;
	this->inputBokehProgram = NULL;
	this->inputSizeProgram = NULL;
}

void* VariableSizeBokehBlurOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) {
	void* buffer = getInputOperation(0)->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void VariableSizeBokehBlurOperation::initExecution() {
	this->inputProgram = getInputSocketReader(0);
	this->inputBokehProgram = getInputSocketReader(1);
	this->inputSizeProgram = getInputSocketReader(2);
	this->radx = 100;
	this->rady = 100;
}

void VariableSizeBokehBlurOperation::executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data) {
	float tempColor[4];
	float bokeh[4];
	tempColor[0] = 0;
	tempColor[1] = 0;
	tempColor[2] = 0;
	tempColor[3] = 0;
	float tempSize[4];
	inputSizeProgram->read(tempSize, x, y, inputBuffers);
	float size = tempSize[0];
	if (size < 2.0f) {
		inputProgram->read(color, x, y, inputBuffers);
	} else {
		float overallmultiplyerr = 0;
		float overallmultiplyerg = 0;
		float overallmultiplyerb = 0;
		MemoryBuffer* inputBuffer = (MemoryBuffer*)data;
		float* buffer = inputBuffer->getBuffer();
		int bufferwidth = inputBuffer->getWidth();
		int bufferstartx = inputBuffer->getRect()->xmin;
		int bufferstarty = inputBuffer->getRect()->ymin;

		int miny = y - size;
		int maxy = y + size;
		int minx = x - size;
		int maxx = x + size;
		miny = max(miny, inputBuffer->getRect()->ymin);
		minx = max(minx, inputBuffer->getRect()->xmin);
		maxy = min(maxy, inputBuffer->getRect()->ymax);
		maxx = min(maxx, inputBuffer->getRect()->xmax);

		float m = 256/size;
		for (int ny = miny ; ny < maxy ; ny ++) {
			int bufferindex = ((minx - bufferstartx)*4)+((ny-bufferstarty)*4*bufferwidth);
			for (int nx = minx ; nx < maxx ; nx ++) {
				float u = 256 - (nx-x) *m;
				float v = 256 - (ny-y) *m;
				inputBokehProgram->read(bokeh, u, v, inputBuffers);
				tempColor[0] += bokeh[0] * buffer[bufferindex];
				tempColor[1] += bokeh[1] * buffer[bufferindex+1];
				tempColor[2] += bokeh[2]* buffer[bufferindex+2];
	//			tempColor[3] += bokeh[3] * buffer[bufferindex+3];
				overallmultiplyerr += bokeh[0];
				overallmultiplyerg += bokeh[1];
				overallmultiplyerb += bokeh[2];
				bufferindex +=4;
			}
		}
		color[0] = tempColor[0]*(1.0/overallmultiplyerr);
		color[1] = tempColor[1]*(1.0/overallmultiplyerg);
		color[2] = tempColor[2]*(1.0/overallmultiplyerb);
		color[3] = 1.0f;
	}
}

void VariableSizeBokehBlurOperation::deinitExecution() {
	this->inputProgram = NULL;
	this->inputBokehProgram = NULL;
	this->inputSizeProgram = NULL;
}

bool VariableSizeBokehBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;
	rcti bokehInput;

	int addx = radx;
	int addy = rady;

	newInput.xmax = input->xmax + addx;
	newInput.xmin = input->xmin - addx;
	newInput.ymax = input->ymax + addy;
	newInput.ymin = input->ymin - addy;
	bokehInput.xmax = 512;
	bokehInput.xmin = 0;
	bokehInput.ymax = 512;
	bokehInput.ymin = 0;

	NodeOperation* operation = getInputOperation(2);
	if (operation->determineDependingAreaOfInterest(input, readOperation, output) ) {
		return true;
	}
	operation = getInputOperation(1);
	if (operation->determineDependingAreaOfInterest(&bokehInput, readOperation, output) ) {
		return true;
	}
	operation = getInputOperation(0);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output) ) {
		return true;
	}
	return false;
}
