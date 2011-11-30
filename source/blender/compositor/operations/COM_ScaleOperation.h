#ifndef _COM_ScaleOperation_h_
#define _COM_ScaleOperation_h_

#include "COM_NodeOperation.h"

class ScaleOperation: public NodeOperation {
private:
	SocketReader *inputOperation;
	SocketReader *inputXOperation;
	SocketReader *inputYOperation;
    float centerX;
    float centerY;
public:
    ScaleOperation();
    bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

    void initExecution();
    void deinitExecution();

};

#endif
