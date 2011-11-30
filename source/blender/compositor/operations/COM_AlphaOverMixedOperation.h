#ifndef _COM_AlphaOverMixedOperation_h
#define _COM_AlphaOverMixedOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class AlphaOverMixedOperation : public MixBaseOperation {
private:
    float x;
public:
    /**
      * Default constructor
      */
    AlphaOverMixedOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

    void setX(float x) {this->x = x;}
};
#endif
