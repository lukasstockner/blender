#ifndef _COM_WriteBufferOperation_h_
#define _COM_WriteBufferOperation_h_

#include "COM_NodeOperation.h"
#include "COM_MemoryProxy.h"
#include "COM_SocketReader.h"
/**
  * @brief Operation to write to a tile
  * @ingroup Operation
  */
class WriteBufferOperation: public NodeOperation {
    MemoryProxy *memoryProxy;
        NodeOperation *input;
	const bNodeTree * tree;
public:
    WriteBufferOperation();
    ~WriteBufferOperation();
    int isBufferOperation() {return true;}
    MemoryProxy* getMemoryProxy() {return this->memoryProxy;}
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);
	const bool isWriteBufferOperation() const {return true;}

    void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers);
    void initExecution();
    void deinitExecution();
	void setbNodeTree(const bNodeTree* tree) {this->tree = tree;}
    void executeOpenCLRegion(cl_context context, cl_program program, cl_command_queue queue, rcti *rect, unsigned int chunkNumber, MemoryBuffer** memoryBuffers);

};
#endif
