#ifndef _COM_ZCombineOperation_h
#define _COM_ZCombineOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class ZCombineOperation : public NodeOperation {
protected:
	SocketReader* image1Reader;
	SocketReader* depth1Reader;
	SocketReader* image2Reader;
	SocketReader* depth2Reader;
public:
    /**
      * Default constructor
      */
	ZCombineOperation();

	void initExecution();
	void deinitExecution();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);
};

class ZCombineAlphaOperation: public ZCombineOperation {
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);
};

#endif
