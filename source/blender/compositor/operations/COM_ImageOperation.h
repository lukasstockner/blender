
#ifndef _COM_ImageOperation_h
#define _COM_ImageOperation_h

#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "BLI_listbase.h"
#include "BKE_image.h"
extern "C" {
    #include "RE_pipeline.h"
    #include "RE_shader_ext.h"
    #include "RE_render_ext.h"
    #include "MEM_guardedalloc.h"
}

typedef enum InterpolationMode {
	COM_IM_NEAREST,
	COM_IM_LINEAR
} InterpolationMode;

/**
  * Base class for all renderlayeroperations
  *
  * @todo: rename to operation.
  */
class BaseImageOperation : public NodeOperation {
protected:
	Image* image;
    ImageUser* imageUser;
    float *imageBuffer;
    int imageheight;
    int imagewidth;
	int framenumber;
	InterpolationMode interpolation;

	BaseImageOperation();
    /**
      * Determine the output resolution. The resolution is retrieved from the Renderer
      */
    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

public:

    void initExecution();
    void deinitExecution();
    void setImage(Image* image) {this->image = image;}
    void setImageUser(ImageUser* imageuser) {this->imageUser = imageuser;}

	void setFramenumber(int framenumber) {this->framenumber = framenumber;}
	void setInterpolationMode(InterpolationMode mode) {this->interpolation= mode;}
};
class ImageOperation: public BaseImageOperation {
public:
	/**
      * Constructor
      */
    ImageOperation();
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);
};
class ImageAlphaOperation: public BaseImageOperation {
public:
	/**
      * Constructor
      */
    ImageAlphaOperation();
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);
};
#endif
