#ifndef _COM_LensGlowImageOperation_h
#define _COM_LensGlowImageOperation_h
#include "COM_NodeOperation.h"


class LensGlowImageOperation : public NodeOperation {
private:
	float scale;

public:
	LensGlowImageOperation();

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

	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
};
#endif
