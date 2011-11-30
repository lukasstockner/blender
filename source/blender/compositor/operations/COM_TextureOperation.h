
#ifndef _COM_TextureOperation_h
#define _COM_TextureOperation_h

#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "BLI_listbase.h"
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
class TextureBaseOperation : public NodeOperation {
private:
	Tex* texture;
	float *textureSize;
	float *textureOffset;

protected:

    /**
      * Determine the output resolution. The resolution is retrieved from the Renderer
      */
    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

	/**
	  * Constructor
	  */
	TextureBaseOperation();

public:
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

	void setTexture(Tex* texture) {this->texture = texture;}
	void setTextureOffset(float* offset) {this->textureOffset= offset;}
	void setTextureSize(float* size) {this->textureSize= size;}

};

class TextureOperation:public TextureBaseOperation {
public:
	TextureOperation();

};
class TextureAlphaOperation:public TextureBaseOperation {
public:
	TextureAlphaOperation();
	void executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]);

};

#endif
