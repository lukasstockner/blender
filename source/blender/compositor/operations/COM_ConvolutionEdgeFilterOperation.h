#ifndef _COM_ConvolutionEdgeFilterOperation_h_
#define _COM_ConvolutionEdgeFilterOperation_h_

#include "COM_ConvolutionFilterOperation.h"

class ConvolutionEdgeFilterOperation: public ConvolutionFilterOperation {
public:
    ConvolutionEdgeFilterOperation();
    void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);
};

#endif
