#include "COM_OpenCLDevice.h"
#include "COM_WorkScheduler.h"


OpenCLDevice::OpenCLDevice(cl_context context, cl_device_id device, cl_program program){
	this->device = device;
	this->context = context;
	this->program = program;
	this->queue = NULL;
}

bool OpenCLDevice::initialize(){
	cl_int error;
	queue = clCreateCommandQueue(context, device, 0, &error);
	return false;
}

void OpenCLDevice::deinitialize(){
	if(queue){
		clReleaseCommandQueue(queue);
	}
}

void OpenCLDevice::execute(WorkPackage *work) {
	const unsigned int chunkNumber = work->getChunkNumber();
	ExecutionGroup * executionGroup = work->getExecutionGroup();
	rcti rect;

	executionGroup->determineChunkRect(&rect, chunkNumber);
	MemoryBuffer ** inputBuffers = executionGroup->getInputBuffers(chunkNumber);
	MemoryBuffer * outputBuffer = executionGroup->allocateOutputBuffer(chunkNumber, &rect);

	executionGroup->getOutputNodeOperation()->executeOpenCLRegion(this->context, this->program, this->queue, &rect, chunkNumber, inputBuffers);
	
	executionGroup->finalizeChunkExecution(chunkNumber, inputBuffers);
	if (outputBuffer != NULL) {
		outputBuffer->setCreatedState();
	}
}
