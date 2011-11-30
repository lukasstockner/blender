#ifndef _COM_LensGlowOperation_h
#define _COM_LensGlowOperation_h
#include "COM_NodeOperation.h"
#include "DNA_lamp_types.h"

class LensGlowOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputProgram;
	Lamp* lamp;

public:
	LensGlowOperation();

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

	void setLamp(Lamp* lamp) {this->lamp = lamp;}
};
#endif
