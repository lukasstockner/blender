
#ifndef _COM_RenderLayersBaseProg_h
#define _COM_RenderLayersBaseProg_h

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

/**
  * Base class for all renderlayeroperations
  *
  * @todo: rename to operation.
  */
class RenderLayersBaseProg : public NodeOperation {
private:
    /**
      * Reference to the scene object.
      */
    Scene* scene;

    /**
      * layerId of the layer where this operation needs to get its data from
      */
    short layerId;

    /**
      * cached instance to the float buffer inside the layer
      */
    float* inputBuffer;

    /**
      * renderpass where this operation needs to get its data from
      */
    int renderpass;

	int elementsize;

protected:
    /**
      * Constructor
      */
	RenderLayersBaseProg(int renderpass, int elementsize);

    /**
      * Determine the output resolution. The resolution is retrieved from the Renderer
      */
    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

    /**
      * retrieve the reference to the float buffer of the renderer.
      */
    inline float* getInputBuffer() {return this->inputBuffer;}

public:
    /**
      * setter for the scene field. Will be called from
      * @see RenderLayerNode to set the actual scene where
      * the data will be retrieved from.
      */
    void setScene(Scene* scene) {this->scene = scene;}
    Scene* getScene() {return this->scene;}
    void setLayerId(short layerId) {this->layerId = layerId;}
    short getLayerId() {return this->layerId;}
    void initExecution();
    void deinitExecution();
	void executePixel(float* output, float x, float y, MemoryBuffer *inputBuffers[]);

};

#endif
