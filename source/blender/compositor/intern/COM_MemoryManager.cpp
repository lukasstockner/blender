#include "COM_MemoryManager.h"
#include "BLI_threads.h"
#include <stdio.h>
#include "COM_defines.h"

vector<MemoryBuffer*> buffers;

ThreadMutex mutex;
int hits = 0;
int fails = 0;

MemoryBuffer* MemoryManager::allocateMemoryBuffer(MemoryProxy *id, unsigned int chunkNumber, rcti *rect, bool addUser) {
#if COM_MM_ENABLE
    if (getTotalAllocatedMemory() > COM_MM_MAX_ALLOCATED_MEMORY) {
        freeSomeMemory();
    }
#endif
	MemoryBuffer *result = new MemoryBuffer(id, chunkNumber, rect);
    MemoryManagerState * state = MemoryManager::getState(id);
    state->addMemoryBuffer(result);
    BLI_mutex_lock(&mutex);
    buffers.push_back(result);
    BLI_mutex_unlock(&mutex);
    if (addUser) {
        result->addUser();
    }
    return result;
}

void MemoryManager::freeSomeMemory() {
    BLI_mutex_lock(&mutex);
    int numberStores = 0;
	unsigned int index;
	for (index = 0 ; index < buffers.size() && numberStores < 50;index ++) {
        MemoryBuffer* buffer = buffers[index];
        if (buffer->saveToDisc()) {
            numberStores ++;
        }
    }
    BLI_mutex_unlock(&mutex);
}
void MemoryManager::addMemoryProxy(MemoryProxy *memoryProxy) {
	MemoryManagerState * state = MemoryManager::getState(memoryProxy);
    if (!state) {
		state = new MemoryManagerState(memoryProxy);
		memoryProxy->setState(state);
    }
}
MemoryBuffer* MemoryManager::getMemoryBuffer(MemoryProxy *id, unsigned int chunkNumber, bool addUser){
    MemoryManagerState * state = MemoryManager::getState(id);
    if (!state) {
        return NULL;
    }
	MemoryBuffer* buffer = state->getMemoryBuffer(chunkNumber);
	if (!buffer) return NULL;
#if COM_MM_ENABLE
    if (buffer->makeAvailable(addUser)) {
        fails++;
    } else {
        hits++;
    }

#else
    if (addUser) {
        buffer->addUser();
    }
#endif
    return buffer;
}

MemoryManagerState* MemoryManager::getState(MemoryProxy* memoryProxy) {
	return memoryProxy->getState();
}
void MemoryManager::initialize() {
    BLI_mutex_init(&mutex);
    hits = 0;
    fails = 0;
}
void MemoryManager::clear() {
//    printf("MemoryManager::cache performance [chunks:%d requests:%d hits:%d fail:%d ratio:%f%%]\n", buffers.size(), (hits+fails), hits, fails, getPerformance());
    buffers.clear();
    BLI_mutex_end(&mutex);
}

long MemoryManager::getTotalAllocatedMemory() {
    long result = 0;
	unsigned int index;
	for (index = 0 ; index < buffers.size();index ++) {
        MemoryBuffer* buffer = buffers[index];
        result += buffer->getAllocatedMemorySize();
    }
    return result;
}

void MemoryManager::releaseUser(MemoryBuffer* memoryBuffer) {
    memoryBuffer->removeUser();
}

float MemoryManager::getPerformance() {
    float result = hits+fails;
    if (result == 0.0f) {return 100.0f;}
    return 100.0f*( 1.0f - (fails / result));
}
