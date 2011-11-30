#ifndef _COM_ScreenLensDistortionOperation_h
#define _COM_ScreenLensDistortionOperation_h
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

class ScreenLensDistortionOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader* inputProgram;

	NodeLensDist * data;

	float dispersion;
	float distortion;
	float kr, kg, kb;
	float kr4, kg4, kb4;
	float maxk;
	float drg;
	float dgb;
	float sc, cx, cy;
public:
	ScreenLensDistortionOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data);

    /**
      * Initialize the execution
      */
	void initExecution();

	void* initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
    /**
      * Deinitialize the execution
      */
    void deinitExecution();

	void setData(NodeLensDist* data) {this->data = data;}
	void setDispertion(float dispersion) {this->dispersion = dispersion;}
	void setDistortion(float distortion) {this->distortion = distortion;}

    bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

private:
	void determineUV(float* result, float x, float y) const;

};
#endif
