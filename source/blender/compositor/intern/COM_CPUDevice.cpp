#include "COM_CPUDevice.h"

void CPUDevice::execute(WorkPackage *work) {
	const unsigned int chunkNumber = work->getChunkNumber();
	ExecutionGroup * executionGroup = work->getExecutionGroup();
	rcti rect;

	executionGroup->determineChunkRect(&rect, chunkNumber);
	MemoryBuffer ** inputBuffers = executionGroup->getInputBuffers(chunkNumber);
	MemoryBuffer * outputBuffer = executionGroup->allocateOutputBuffer(chunkNumber, &rect);

	executionGroup->getOutputNodeOperation()->executeRegion(&rect, chunkNumber, inputBuffers);

	executionGroup->finalizeChunkExecution(chunkNumber, inputBuffers);
	if (outputBuffer != NULL) {
		outputBuffer->setCreatedState();
	}
}

