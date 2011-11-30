#ifndef _COM_SeparateChannelOperation_h_
#define _COM_SeparateChannelOperation_h_

#include "COM_NodeOperation.h"

class SeparateChannelOperation: public NodeOperation {
private:
	SocketReader *inputOperation;
    int channel;
public:
    SeparateChannelOperation();
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

    void initExecution();
    void deinitExecution();

    void setChannel(int channel) {this->channel = channel;}
};

#endif
