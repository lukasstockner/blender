#ifndef _COM_DotproductOperation_h_
#define _COM_DotproductOperation_h_

#include "COM_NodeOperation.h"

class DotproductOperation: public NodeOperation {
private:
	SocketReader *input1Operation;
	SocketReader *input2Operation;
public:
    DotproductOperation();
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

    void initExecution();
    void deinitExecution();

};

#endif
