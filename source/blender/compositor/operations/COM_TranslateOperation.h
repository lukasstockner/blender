#ifndef _COM_TranslateOperation_h_
#define _COM_TranslateOperation_h_

#include "COM_NodeOperation.h"

class TranslateOperation: public NodeOperation {
private:
	SocketReader *inputOperation;
	SocketReader*inputXOperation;
	SocketReader*inputYOperation;
	float deltaX;
	float deltaY;
public:
	TranslateOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

	void initExecution();
	void deinitExecution();

	float getDeltaX() {return this->deltaX;}
	float getDeltaY() {return this->deltaY;}
};

#endif
