#ifndef _COM_CombineChannelsOperation_h_
#define _COM_CombineChannelsOperation_h_

#include "COM_NodeOperation.h"

class CombineChannelsOperation: public NodeOperation {
private:
	SocketReader *inputChannel1Operation;
	SocketReader *inputChannel2Operation;
	SocketReader *inputChannel3Operation;
	SocketReader *inputChannel4Operation;
public:
    CombineChannelsOperation();
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

    void initExecution();
    void deinitExecution();
};

#endif
