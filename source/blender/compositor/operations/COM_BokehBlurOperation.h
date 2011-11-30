#ifndef _COM_BokehBokehBlurOperation_h
#define _COM_BokehBlurOperation_h
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

class BokehBlurOperation : public NodeOperation, public QualityStepHelper {
private:
	SocketReader* inputProgram;
	SocketReader* inputBokehProgram;
	SocketReader* inputBoundingBoxReader;
	float size;
	float bokehMidX;
	float bokehMidY;
	float bokehDimension;
public:
	BokehBlurOperation();

	void* initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
    /**
      * the inner loop of this program
      */
	void executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data);

    /**
      * Initialize the execution
      */
    void initExecution();

    /**
      * Deinitialize the execution
      */
    void deinitExecution();

    bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	void setSize(float size) {this->size = size;}
};
#endif
