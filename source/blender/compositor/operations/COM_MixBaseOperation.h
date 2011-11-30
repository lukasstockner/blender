#ifndef _COM_MixBaseOperation_h
#define _COM_MixBaseOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MixBaseOperation : public NodeOperation {
protected:
    /**
      * Prefetched reference to the inputProgram
      */
	SocketReader * inputValueOperation;
	SocketReader* inputColor1Operation;
	SocketReader* inputColor2Operation;
    bool valueAlphaMultiply;
public:
    /**
      * Default constructor
      */
    MixBaseOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

    /**
      * Initialize the execution
      */
    void initExecution();

    /**
      * Deinitialize the execution
      */
    void deinitExecution();

    void setUseValueAlphaMultiply(const bool value) {this->valueAlphaMultiply = value;}
    bool useValueAlphaMultiply() {return this->valueAlphaMultiply;}

};
#endif
