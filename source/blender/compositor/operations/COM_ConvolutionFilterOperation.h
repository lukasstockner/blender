#ifndef _COM_ConvolutionFilterOperation_h_
#define _COM_ConvolutionFilterOperation_h_

#include "COM_NodeOperation.h"

class ConvolutionFilterOperation: public NodeOperation {
private:
    int filterWidth;
    int filterHeight;

protected:
	SocketReader *inputOperation;
	SocketReader *inputValueOperation;
    float* filter;

public:
    ConvolutionFilterOperation();
    void set3x3Filter(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9);
    bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

    void initExecution();
    void deinitExecution();
};

#endif
