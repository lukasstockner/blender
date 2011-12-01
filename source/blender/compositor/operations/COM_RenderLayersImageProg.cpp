#include "COM_RenderLayersImageProg.h"

RenderLayersColourProg::RenderLayersColourProg() :RenderLayersBaseProg(SCE_PASS_COMBINED, 4) {
    this->addOutputSocket(COM_DT_COLOR);
}
