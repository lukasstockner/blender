#ifndef _COM_FlipOperation_h_
#define _COM_FlipOperation_h_

#include "COM_NodeOperation.h"

class FlipOperation: public NodeOperation {
private:
	SocketReader *inputOperation;
    bool flipX;
    bool flipY;
public:
    FlipOperation();
    bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

    void initExecution();
    void deinitExecution();
    void setFlipX(bool flipX) {this->flipX = flipX;}
    void setFlipY(bool flipY) {this->flipY = flipY;}
};

#endif
