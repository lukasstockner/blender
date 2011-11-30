#include "COM_MemoryManagerState.h"

MemoryManagerState::MemoryManagerState(MemoryProxy *memoryProxy) {
	this->memoryProxy = memoryProxy;
    this->currentSize = 0;
	this->chunkBuffers = NULL;
    BLI_mutex_init(&this->mutex);
}

MemoryProxy * MemoryManagerState::getMemoryProxy() {
	return this->memoryProxy;
}

MemoryManagerState::~MemoryManagerState() {
	this->memoryProxy = NULL;
    unsigned int index;
    for (index = 0 ; index < this->currentSize; index ++){
		MemoryBuffer* buffer = this->chunkBuffers[index];
        if (buffer) {
            delete buffer;
        }
    }
	delete this->chunkBuffers;
    BLI_mutex_end(&this->mutex);
}

void MemoryManagerState::addMemoryBuffer(MemoryBuffer *buffer) {
    BLI_mutex_lock(&this->mutex);
	unsigned int chunkNumber = buffer->getChunkNumber();
	unsigned int index;
	while (this->currentSize <= chunkNumber) {
		unsigned int newSize = this->currentSize + 1000;
        MemoryBuffer** newbuffer = new MemoryBuffer*[newSize];
		MemoryBuffer** oldbuffer = this->chunkBuffers;

		for (index = 0 ; index < this->currentSize ; index++) {
			newbuffer[index] = oldbuffer[index];
        }
		for (index = currentSize ; index < newSize; index++) {
			newbuffer[index] = NULL;
        }

		this->chunkBuffers = newbuffer;
        this->currentSize = newSize;
        if (oldbuffer) delete oldbuffer;
    }

	this->chunkBuffers[chunkNumber] = buffer;
    BLI_mutex_unlock(&this->mutex);
}

MemoryBuffer* MemoryManagerState::getMemoryBuffer(unsigned int chunkNumber) {
    MemoryBuffer* result = NULL;
	if (chunkNumber< this->currentSize){
		result = this->chunkBuffers[chunkNumber];
        if (result) {
            return result;
        }
    }

    BLI_mutex_lock(&this->mutex);
	if (chunkNumber< this->currentSize){
		result = this->chunkBuffers[chunkNumber];
    }

    BLI_mutex_unlock(&this->mutex);
    return result;
}
