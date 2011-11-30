#include "COM_RenderLayersImageProg.h"

RenderLayersColourProg::RenderLayersColourProg() :RenderLayersBaseProg(SCE_PASS_COMBINED, 4) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
}
