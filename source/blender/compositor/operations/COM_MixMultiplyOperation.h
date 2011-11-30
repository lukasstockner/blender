#ifndef _COM_MixMultiplyOperation_h
#define _COM_MixMultiplyOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MixMultiplyOperation : public MixBaseOperation {
public:
    /**
      * Default constructor
      */
    MixMultiplyOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

};
#endif
