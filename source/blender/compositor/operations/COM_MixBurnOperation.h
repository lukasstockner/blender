#ifndef _COM_MixBurnOperation_h
#define _COM_MixBurnOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MixBurnOperation : public MixBaseOperation {
public:
    /**
      * Default constructor
      */
    MixBurnOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

};
#endif
