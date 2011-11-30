#ifndef _COM_MixOverlayOperation_h
#define _COM_MixOverlayOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MixOverlayOperation : public MixBaseOperation {
public:
    /**
      * Default constructor
      */
    MixOverlayOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

};
#endif
