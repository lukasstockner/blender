#ifndef _COM_RotateOperation_h_
#define _COM_RotateOperation_h_

#include "COM_NodeOperation.h"

class RotateOperation: public NodeOperation {
private:
	SocketReader *imageSocket;
	float degree;
    float centerX;
    float centerY;
    float cosine;
    float sine;
public:
    RotateOperation();
    bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);
	void setDegree(float degree) {this->degree = degree;}
    void initExecution();
    void deinitExecution();
};

#endif
