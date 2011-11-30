#ifndef _COM_ColorRampOperation_h
#define _COM_ColorRampOperation_h
#include "COM_NodeOperation.h"
#include "DNA_texture_types.h"

class ColorRampOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputProgram;
	ColorBand* colorBand;
public:
	ColorRampOperation();

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

	void setColorBand(ColorBand* colorBand) {this->colorBand = colorBand;}


};
#endif
