#ifndef _COM_BokehImageOperation_h
#define _COM_BokehImageOperation_h
#include "COM_NodeOperation.h"


class BokehImageOperation : public NodeOperation {
private:
	NodeBokehImage *data;

	float center[2];
	float centerX;
	float centerY;
	float inverseRounding;
	float circularDistance;
	float flapRad;
	float flapRadAdd;

	void detemineStartPointOfFlap(float r[2], int flapNumber, float distance);
	float isInsideBokeh(float distance, float x, float y);
public:
	BokehImageOperation();

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

	void setData(NodeBokehImage *data) {this->data = data;}
};
#endif
