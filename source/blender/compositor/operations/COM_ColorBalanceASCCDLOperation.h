#ifndef _COM_ColorBalanceASCCDLOperation_h
#define _COM_ColorBalanceASCCDLOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class ColorBalanceASCCDLOperation : public NodeOperation {
protected:
    /**
      * Prefetched reference to the inputProgram
      */
	SocketReader * inputValueOperation;
	SocketReader * inputColorOperation;

    float gain[3];
    float lift[3];
    float gamma[3];

public:
    /**
      * Default constructor
      */
    ColorBalanceASCCDLOperation();

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

    void setGain(float gain[3]) {
        this->gain[0] = gain[0];
        this->gain[1] = gain[1];
        this->gain[2] = gain[2];
    }
    void setLift(float lift[3]) {
        this->lift[0] = lift[0];
        this->lift[1] = lift[1];
        this->lift[2] = lift[2];
    }
    void setGamma(float gamma[3]) {
        this->gamma[0] = gamma[0];
        this->gamma[1] = gamma[1];
        this->gamma[2] = gamma[2];
    }
};
#endif
