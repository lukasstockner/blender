#ifndef _COM_ColorCurveOperation_h
#define _COM_ColorCurveOperation_h
#include "COM_NodeOperation.h"
#include "DNA_color_types.h"
#include "COM_CurveBaseOperation.h"

class ColorCurveOperation : public CurveBaseOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputFacProgram;
	SocketReader * inputImageProgram;
	SocketReader * inputBlackProgram;
	SocketReader * inputWhiteProgram;
public:
	ColorCurveOperation();

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
