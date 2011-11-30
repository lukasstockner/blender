#ifndef _COM_MixSoftLightOperation_h
#define _COM_MixSoftLightOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MixSoftLightOperation : public MixBaseOperation {
public:
    /**
      * Default constructor
      */
    MixSoftLightOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

};
#endif
