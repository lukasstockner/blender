#ifndef _COM_MixDifferenceOperation_h
#define _COM_MixDifferenceOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MixDifferenceOperation : public MixBaseOperation {
public:
    /**
      * Default constructor
      */
    MixDifferenceOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

};
#endif
