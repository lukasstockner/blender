#ifndef _COM_ProjectorLensDistortionOperation_h
#define _COM_ProjectorLensDistortionOperation_h
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

class ProjectorLensDistortionOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputProgram;

	NodeLensDist * data;

	float dispersion;
	float kr, kr2;
public:
	ProjectorLensDistortionOperation();

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

    bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

};
#endif
