#ifndef _COM_ConvertDepthToRadiusOperation_h
#define _COM_ConvertDepthToRadiusOperation_h
#include "COM_NodeOperation.h"
#include "DNA_object_types.h"

/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class ConvertDepthToRadiusOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputOperation;
	float fStop;
	float maxRadius;
	float focalDistance;
	Object *cameraObject;
public:
    /**
      * Default constructor
      */
	ConvertDepthToRadiusOperation();

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

	void setfStop(float fStop) {this->fStop = fStop;}
	void setMaxRadius(float maxRadius) {this->maxRadius = maxRadius;}
	void setCameraObject(Object* camera) {this->cameraObject = camera;}
	float determineFocalDistance() const;
};
#endif
