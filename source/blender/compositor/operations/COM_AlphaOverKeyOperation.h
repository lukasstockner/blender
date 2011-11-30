#ifndef _COM_AlphaOverKeyOperation_h
#define _COM_AlphaOverKeyOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class AlphaOverKeyOperation : public MixBaseOperation {
public:
    /**
      * Default constructor
      */
    AlphaOverKeyOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);
};
#endif
