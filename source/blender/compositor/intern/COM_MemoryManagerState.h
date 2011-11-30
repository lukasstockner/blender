class MemoryManagerState;

#ifndef _COM_MemoryManagerState_h_
#define _COM_MemoryManagerState_h_

#include "COM_MemoryProxy.h"
#include "COM_MemoryBuffer.h"
#include <vector>
extern "C" {
    #include "BLI_threads.h"
}

/**
  * @brief State of a MemoryProxy in the MemoryManager.
  * @ingroup Memory
  */
class MemoryManagerState {
private:
	/**
	  * @brief reference to the MemoryProxy of this state
	  */
	MemoryProxy *memoryProxy;

	/**
	  * @brief list of all chunkbuffers
	  */
	MemoryBuffer** chunkBuffers;

	/**
	  * @brief size of the chunkBuffers
	  */
    unsigned int currentSize;

	/**
	  * @brief lock to this memory for multithreading
	  */
    ThreadMutex mutex;
public:
	/**
	  * @brief creates a new MemoryManagerState for a certain MemoryProxy.
	  */
	MemoryManagerState(MemoryProxy * memoryProxy);
	/**
	  * @brief destructor
	  */
    ~MemoryManagerState();

	/**
	  * @brief get the reference to the MemoryProxy this state belongs to.
	  */
	MemoryProxy *getMemoryProxy();

	/**
	  * @brief add a new memorybuffer to the state
	  */
    void addMemoryBuffer(MemoryBuffer* buffer);

	/**
	  * @brief get the MemoryBuffer assiciated to a chunk.
	  * @param chunkNumber the chunknumber
	  */
	MemoryBuffer* getMemoryBuffer(unsigned int chunkNumber);
};

#endif
