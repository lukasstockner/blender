#ifndef _COM_HueSaturationValueCorrectOperation_h
#define _COM_HueSaturationValueCorrectOperation_h
#include "COM_NodeOperation.h"
#include "COM_CurveBaseOperation.h"

class HueSaturationValueCorrectOperation : public CurveBaseOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputProgram;
public:
	HueSaturationValueCorrectOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* Vector, float x, float y, MemoryBuffer *inputBuffers[]);

    /**
      * Initialize the execution
      */
    void initExecution();

    /**
      * Deinitialize the execution
      */
    void deinitExecution();

};
#endif
