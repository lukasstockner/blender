#ifndef _COM_GammaOperation_h
#define _COM_GammaOperation_h
#include "COM_NodeOperation.h"


class GammaOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputProgram;
	SocketReader* inputGammaProgram;

public:
    GammaOperation();

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
