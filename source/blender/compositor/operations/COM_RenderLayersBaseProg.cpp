#include "COM_RenderLayersBaseProg.h"

#include "BLI_listbase.h"
#include "DNA_scene_types.h"

extern "C" {
    #include "RE_pipeline.h"
    #include "RE_shader_ext.h"
    #include "RE_render_ext.h"
}

RenderLayersBaseProg::RenderLayersBaseProg(int renderpass, int elementsize): NodeOperation() {
    this->renderpass = renderpass;
    this->setScene(NULL);
    this->inputBuffer = NULL;
	this->elementsize = elementsize;
}


void RenderLayersBaseProg::initExecution() {
    Scene * scene = this->getScene();
    Render *re= (scene)? RE_GetRender(scene->id.name): NULL;
    RenderResult *rr= NULL;

    if(re)
            rr= RE_AcquireResultRead(re);

    if(rr) {
        SceneRenderLayer *srl= (SceneRenderLayer*)BLI_findlink(&scene->r.layers, getLayerId());
        if(srl) {

               RenderLayer *rl= RE_GetRenderLayer(rr, srl->name);
               if(rl && rl->rectf) {
                   this->inputBuffer = RE_RenderLayerGetPass(rl, renderpass);

                   if (this->inputBuffer == NULL || renderpass == SCE_PASS_COMBINED) {
					   this->inputBuffer = rl->rectf;
                   }
               }
           }
    }
    if (re) {
        RE_ReleaseResult(re);
        re = NULL;
    }
}

void RenderLayersBaseProg::executePixel(float* output, float x, float y, MemoryBuffer *inputBuffers[]) {
	int ix = x;
	int iy = y;

        if (inputBuffer == NULL || ix < 0 || iy < 0 || ix >= (int)this->getWidth() || iy >= (int)this->getHeight() ) {
		output[0] = 0.0f;
		output[1] = 0.0f;
		output[2] = 0.0f;
		output[3] = 0.0f;
	} else {
		unsigned int offset = (iy*this->getWidth()+ix) * elementsize;
		if (elementsize == 1) {
			output[0] = inputBuffer[offset];
			output[1] = 0.0f;
			output[2] = 0.0f;
			output[3] = 0.0f;
		} else if (elementsize == 3){
			output[0] = inputBuffer[offset];
			output[1] = inputBuffer[offset+1];
			output[2] = inputBuffer[offset+2];
			output[3] = 1.0f;
		} else {
			output[0] = inputBuffer[offset];
			output[1] = inputBuffer[offset+1];
			output[2] = inputBuffer[offset+2];
			output[3] = inputBuffer[offset+3];
		}
	}
}

void RenderLayersBaseProg::deinitExecution() {
    this->inputBuffer = NULL;
}

void RenderLayersBaseProg::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
    Scene *sce= this->getScene();
    Render *re= (sce)? RE_GetRender(sce->id.name): NULL;
    RenderResult *rr= NULL;

	resolution[0] = 0;
	resolution[1] = 0;

    if(re)
            rr= RE_AcquireResultRead(re);

    if(rr) {
        SceneRenderLayer *srl= (SceneRenderLayer*)BLI_findlink(&sce->r.layers, getLayerId());
        if(srl) {
               RenderLayer *rl= RE_GetRenderLayer(rr, srl->name);
               if(rl && rl->rectf) {
                   resolution[0]=rl->rectx;
                   resolution[1]=rl->recty;
               }
        }
    }

    if(re)
         RE_ReleaseResult(re);

}

