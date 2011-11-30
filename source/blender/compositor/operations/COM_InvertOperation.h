#ifndef _COM_InvertOperation_h
#define _COM_InvertOperation_h
#include "COM_NodeOperation.h"


class InvertOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputValueProgram;
	SocketReader * inputColorProgram;

    bool alpha;
    bool color;

public:
    InvertOperation();

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

    void setColor(bool color) {this->color = color;}
    void setAlpha(bool alpha) {this->alpha = alpha;}
};
#endif
