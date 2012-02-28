/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_OpenCLTestOperation.h"
#include "stdio.h"

OpenCLTestOperation::OpenCLTestOperation(): NodeOperation() {
    this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->setOpenCL(true);
	this->openCLKernel = NULL;
}

void OpenCLTestOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
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
