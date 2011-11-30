#ifndef _COM_SocketProxyOperation_h_
#define _COM_SocketProxyOperation_h_

#include "COM_NodeOperation.h"

class SocketProxyOperation: public NodeOperation {
private:
	SocketReader *inputOperation;
public:
    SocketProxyOperation();
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

    void initExecution();
    void deinitExecution();
};

#endif
