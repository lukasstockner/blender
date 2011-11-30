#ifndef _COM_ReadBufferOperation_h
#define _COM_ReadBufferOperation_h

#include "COM_NodeOperation.h"
#include "COM_MemoryProxy.h"

class ReadBufferOperation: public NodeOperation {
private:
	MemoryProxy *memoryProxy;
	unsigned int offset;
    int readmode;
public:
    ReadBufferOperation();
    int isBufferOperation() {return true;}
	void setMemoryProxy(MemoryProxy* memoryProxy) {this->memoryProxy = memoryProxy;}
	MemoryProxy* getMemoryProxy() {return this->memoryProxy;}
    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

	void* initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);
	const bool isReadBufferOperation() const {return true;}
	void setOffset(unsigned int offset) {this->offset = offset;}
	unsigned int getOffset() {return this->offset;}
    bool determineDependingAreaOfInterest(rcti * input, ReadBufferOperation *readOperation, rcti* output);
    void setReadMode(int readmode) {this->readmode = readmode;}
	MemoryBuffer* getInputMemoryBuffer(MemoryBuffer** memoryBuffers) {return memoryBuffers[offset];}
	
};

#endif
