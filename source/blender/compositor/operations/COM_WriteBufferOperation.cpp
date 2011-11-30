#include "COM_WriteBufferOperation.h"
#include "COM_defines.h"
#include "COM_MemoryManager.h"
#include <stdio.h>

/// @TODO: writebuffers don't have an actual data type set.
WriteBufferOperation::WriteBufferOperation() :NodeOperation() {
	this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
	this->memoryProxy = new MemoryProxy();
	this->memoryProxy->setWriteBufferOperation(this);
	this->memoryProxy->setExecutor(NULL);
	this->tree = NULL;
}
WriteBufferOperation::~WriteBufferOperation() {
	if (this->memoryProxy) {
		delete this->memoryProxy;
		this->memoryProxy = NULL;
	}
}

void WriteBufferOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
	input->read(color, x, y, inputBuffers);
}
void WriteBufferOperation::initExecution() {
		this->input = this->getInputOperation(0);
	MemoryManager::addMemoryProxy(this->memoryProxy);
}
void WriteBufferOperation::deinitExecution() {
	this->input = NULL;
}


void WriteBufferOperation::executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers) {
	MemoryBuffer* memoryBuffer = MemoryManager::getMemoryBuffer(this->getMemoryProxy(), tileNumber, false);
	float* buffer = memoryBuffer->getBuffer();
	if (this->input->isComplex()) {
		void* data = this->input->initializeTileData(rect, memoryBuffers);
		int x1 = rect->xmin;
		int y1 = rect->ymin;
		int x2 = rect->xmax;
		int y2 = rect->ymax;
		int offset4 = 0;
		int x;
		int y;
		bool breaked = false;
		for (y = y1 ; y < y2 && (!breaked) ; y++) {
			for (x = x1 ; x < x2; x++) {
				input->read(&(buffer[offset4]), x, y, memoryBuffers, data);
				offset4 +=4;

			}
			if (tree->test_break && tree->test_break(tree->tbh)) {
				breaked = true;
			}

		}
		if (data) {
			this->input->deinitializeTileData(rect, memoryBuffers, data);
			data = NULL;
		}
	} else {
		int x1 = rect->xmin;
		int y1 = rect->ymin;
		int x2 = rect->xmax;
		int y2 = rect->ymax;
		int offset4 = 0;
		int x;
		int y;
		bool breaked = false;
		for (y = y1 ; y < y2 && (!breaked) ; y++) {
			for (x = x1 ; x < x2 ; x++) {
				input->read(&(buffer[offset4]), x, y, memoryBuffers);
				offset4 +=4;
			}
			if (tree->test_break && tree->test_break(tree->tbh)) {
				breaked = true;
			}
		}
	}
	memoryBuffer->setCreatedState();
}

void WriteBufferOperation::executeOpenCLRegion(cl_context context, cl_program program, cl_command_queue queue, rcti *rect, unsigned int chunkNumber, MemoryBuffer** inputMemoryBuffers) {
	MemoryBuffer* outputMemoryBuffer = MemoryManager::getMemoryBuffer(this->getMemoryProxy(), chunkNumber, false);
	float* outputFloatBuffer = outputMemoryBuffer->getBuffer();
	cl_int error;
	/*
	  1. create cl_mem from outputbuffer
	  2. call NodeOperation (input) executeOpenCLChunk(.....)
	  3. schedule readback from opencl to main device (outputbuffer)
	  4. schedule native callback

	  note: list of cl_mem will be filled by 2, and needs to be cleaned up by 4
	  */
	// STEP 1
	const unsigned int outputBufferWidth = outputMemoryBuffer->getWidth();
	const unsigned int outputBufferHeight = outputMemoryBuffer->getHeight();

	const cl_image_format imageFormat = {
		CL_RGBA,
		CL_FLOAT
	};

	cl_mem clOutputBuffer = clCreateImage2D(context, CL_MEM_WRITE_ONLY|CL_MEM_USE_HOST_PTR, &imageFormat, outputBufferWidth, outputBufferHeight, 0, outputFloatBuffer, &error);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
	
	// STEP 2
	list<cl_mem> * clMemToCleanUp = new list<cl_mem>();
	clMemToCleanUp->push_back(clOutputBuffer);
	list<cl_kernel> *clKernelsToCleanUp = new list<cl_kernel>();

	this->input->executeOpenCL(context, program, queue, outputMemoryBuffer, clOutputBuffer, inputMemoryBuffers, clMemToCleanUp, clKernelsToCleanUp);

	// STEP 3

	size_t origin[3] = {0,0,0};
	size_t region[3] = {outputBufferWidth,outputBufferHeight,1};

	error = clEnqueueBarrier(queue);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
	error = clEnqueueReadImage(queue, clOutputBuffer, CL_TRUE, origin, region, 0, 0, outputFloatBuffer, 0, NULL, NULL);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
	// STEP 4

	while (clMemToCleanUp->size()>0) {
		cl_mem mem = clMemToCleanUp->front();
		error = clReleaseMemObject(mem);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
		clMemToCleanUp->pop_front();
	}

	while (clKernelsToCleanUp->size()>0) {
		cl_kernel kernel = clKernelsToCleanUp->front();
		error = clReleaseKernel(kernel);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
		clKernelsToCleanUp->pop_front();
	}
	delete clKernelsToCleanUp;
}
