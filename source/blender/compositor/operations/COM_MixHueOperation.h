#ifndef _COM_MixHueOperation_h
#define _COM_MixHueOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MixHueOperation : public MixBaseOperation {
public:
    /**
      * Default constructor
      */
    MixHueOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

};
#endif
