#ifndef _COM_BrightnessOperation_h
#define _COM_BrightnessOperation_h
#include "COM_NodeOperation.h"


class BrightnessOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputProgram;
	SocketReader * inputBrightnessProgram;
	SocketReader* inputContrastProgram;

public:
    BrightnessOperation();

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

};
#endif
