#ifndef _COM_SetAlphaOperation_h
#define _COM_SetAlphaOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class SetAlphaOperation : public NodeOperation {
private:
	SocketReader *inputColor;
	SocketReader *inputAlpha;

public:
    /**
      * Default constructor
      */
    SetAlphaOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

    void initExecution();
    void deinitExecution();
};
#endif
