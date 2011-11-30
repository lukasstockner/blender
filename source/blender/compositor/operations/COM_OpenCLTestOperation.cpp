#include "COM_OpenCLTestOperation.h"
#include "COM_OutputSocket.h"
#include "stdio.h"

OpenCLTestOperation::OpenCLTestOperation(): NodeOperation() {
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
	this->setComplex(true);
	this->setOpenCL(true);
	this->openCLKernel = NULL;
}

void OpenCLTestOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
    outputValue[0] = 1.0f;
    outputValue[1] = 0.0f;
    outputValue[2] = 1.0f;
    outputValue[3] = 1.0f;
}

void OpenCLTestOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
    resolution[0] = 1920;
    resolution[1] = 1080;
}

void OpenCLTestOperation::executeOpenCL(cl_context context,cl_program program, cl_command_queue queue, MemoryBuffer* outputMemoryBuffer, cl_mem clOutputBuffer, MemoryBuffer** inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, list<cl_kernel> *clKernelsToCleanUp) {
	cl_int error;
	cl_kernel kernel = clCreateKernel(program, "testKernel", &error);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}

	error = clSetKernelArg(kernel, 0, sizeof(cl_mem), &clOutputBuffer);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}

	//	const size_t offset[] = {0,0,0};
	const size_t size[] = {outputMemoryBuffer->getWidth(),outputMemoryBuffer->getHeight()};
	
	error = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, size, 0, 0, 0, NULL);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
}
