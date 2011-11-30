#include "COM_DilateErodeOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"

DilateErodeOperation::DilateErodeOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
	this->setComplex(true);
    this->inputProgram = NULL;
	this->inset = 0.0f;
	this->_switch = 0.5f;
	this->distance = 0.0f;
}
void DilateErodeOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
	if (this->distance < 0.0f) {
		this->scope = - this->distance + this->inset;
	} else {
		if (this->inset*2 > this->distance) {
			this->scope = max(this->inset*2 - this->distance, this->distance);
		} else {
			this->scope = distance;
		}
	}
	if (scope < 3) {
		scope = 3;
	}
}

void* DilateErodeOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) {
	void* buffer = inputProgram->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void DilateErodeOperation::executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data) {
	float inputValue[4];
	const float sw = this->_switch;
	const float distance = this->distance;
	float pixelvalue;
	const float rd = scope * scope;
	const float inset = this->inset;
	float mindist = rd*2;

	MemoryBuffer* inputBuffer = (MemoryBuffer*)data;
	float* buffer = inputBuffer->getBuffer();
	rcti* rect = inputBuffer->getRect();
	const int minx = max(x - scope, rect->xmin);
	const int miny = max(y - scope, rect->ymin);
	const int maxx = min(x + scope, rect->xmax);
	const int maxy = min(y + scope, rect->ymax);
	const int bufferWidth = rect->xmax-rect->xmin;
	int offset;

	this->inputProgram->read(inputValue, x, y, inputBuffers);
	if (inputValue[0]>sw) {
		for (int yi = miny ; yi<maxy;yi++) {
			offset = ((yi-rect->ymin)*bufferWidth+(minx-rect->xmin))*4;
			for (int xi = minx ; xi<maxx;xi++) {
				if (buffer[offset]<sw) {
					const float dx = xi-x;
					const float dy = yi-y;
					const float dis = dx*dx+dy*dy;
					mindist = min(mindist, dis);
				}
				offset +=4;
			}
		}
		pixelvalue = -sqrtf(mindist);
	} else {
		for (int yi = miny ; yi<maxy;yi++) {
			offset = ((yi-rect->ymin)*bufferWidth+(minx-rect->xmin))*4;
			for (int xi = minx ; xi<maxx;xi++) {
				if (buffer[offset]>sw) {
					const float dx = xi-x;
					const float dy = yi-y;
					const float dis = dx*dx+dy*dy;
					mindist = min(mindist, dis);
				}
				offset +=4;

			}
		}
		pixelvalue = sqrtf(mindist);
	}

	if (distance > 0.0f) {
		const float delta = distance - pixelvalue;
		if (delta >= 0.0f) {
			if (delta >= inset) {
				color[0] = 1.0f;
			} else {
				color[0] = delta/inset;
			}
		} else {
			color[0] = 0.0f;
		}
	} else {
		const float delta = -distance+pixelvalue;
		if (delta < 0.0f) {
			if (delta < -inset) {
				color[0] = 1.0f;
			} else {
				color[0] = (-delta)/inset;
			}
		} else {
			color[0] = 0.0f;
		}
	}
}

void DilateErodeOperation::deinitExecution() {
	this->inputProgram = NULL;
}

bool DilateErodeOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;

	newInput.xmax = input->xmax + scope;
	newInput.xmin = input->xmin - scope;
	newInput.ymax = input->ymax + scope;
	newInput.ymin = input->ymin - scope;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
