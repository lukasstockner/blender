#ifndef _COM_SetValueOperation_h
#define _COM_SetValueOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class SetValueOperation : public NodeOperation {
private:
    float value;

public:
    /**
      * Default constructor
      */
    SetValueOperation();

    const float getValue() {return this->value;}
    void setValue(float value) {this->value = value;}

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

	const bool isSetOperation() const {return true;}
};
#endif
