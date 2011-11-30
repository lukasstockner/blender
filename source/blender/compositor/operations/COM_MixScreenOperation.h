#ifndef _COM_MixScreenOperation_h
#define _COM_MixScreenOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MixScreenOperation : public MixBaseOperation {
public:
    /**
      * Default constructor
      */
    MixScreenOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);
};
#endif
