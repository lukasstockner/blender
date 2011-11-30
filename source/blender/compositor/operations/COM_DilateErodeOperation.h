#ifndef _COM_DilateErodeOperation_h
#define _COM_DilateErodeOperation_h
#include "COM_NodeOperation.h"


class DilateErodeOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputProgram;

	float distance;
	float _switch;
	float inset;

	/**
	  * determines the area of interest to track pixels
	  * keep this one as small as possible for speed gain.
	  */
	int scope;
public:
	DilateErodeOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data);

    /**
      * Initialize the execution
      */
	void initExecution();

	void* initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
    /**
      * Deinitialize the execution
      */
    void deinitExecution();

	void setData(NodeDilateErode* data) {this->distance= data->distance;this->inset = data->inset;this->_switch = data->sw;}
	void setDistance(float distance) {this->distance = distance;}
	void setSwitch(float sw) {this->_switch = sw;}
	void setInset(float inset) {this->inset = inset;}

    bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

};
#endif
