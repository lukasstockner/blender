#ifndef _COM_DifferenceMatteOperation_h
#define _COM_DifferenceMatteOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class DifferenceMatteOperation : public NodeOperation {
private:
	NodeChroma* settings;
	SocketReader * inputImage1Program;
	SocketReader* inputImage2Program;
public:
    /**
      * Default constructor
      */
	DifferenceMatteOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

	void initExecution();
	void deinitExecution();

	void setSettings(NodeChroma* nodeChroma) {this->settings= nodeChroma;}
};
#endif
