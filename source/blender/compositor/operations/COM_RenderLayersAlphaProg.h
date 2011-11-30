#ifndef _COM_RenderLayersAlphaProg_h
#define _COM_RenderLayersAlphaProg_h

#include "COM_RenderLayersBaseProg.h"

class RenderLayersAlphaProg : public RenderLayersBaseProg {
public:
    RenderLayersAlphaProg();
    void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

};

#endif
