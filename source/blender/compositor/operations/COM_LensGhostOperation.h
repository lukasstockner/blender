#ifndef _COM_LensGhostOperation_h
#define _COM_LensGhostOperation_h
#include "COM_NodeOperation.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"

class LensGhostProjectionOperation : public NodeOperation {
protected:
	Object* lampObject;
	Lamp* lamp;
	Object* cameraObject;

	void* system;
	float visualLampPosition[3];
	CompositorQuality quality;
	int step;
	SocketReader * bokehReader;

public:
	LensGhostProjectionOperation();

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
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	void setLampObject(Object* lampObject) {this->lampObject = lampObject;}
	void setCameraObject(Object* cameraObject) {this->cameraObject = cameraObject;}

	void setQuality(CompositorQuality quality) {this->quality = quality;}
};

class LensGhostOperation : public LensGhostProjectionOperation {
public:
	LensGhostOperation();

	void* initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	void deinitializeTileData(rcti *rect, MemoryBuffer **memoryBuffers, void *data);
    /**
      * the inner loop of this program
      */
	void executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void * data);
    /**
      * Initialize the execution
      */
    void initExecution();
};
#endif
