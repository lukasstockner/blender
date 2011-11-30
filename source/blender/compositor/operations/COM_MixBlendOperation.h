#ifndef _COM_MixBlendOperation_h
#define _COM_MixBlendOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MixBlendOperation : public MixBaseOperation {
public:
    /**
      * Default constructor
      */
    MixBlendOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

};
#endif
