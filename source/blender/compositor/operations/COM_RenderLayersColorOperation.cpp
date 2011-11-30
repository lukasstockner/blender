#include "COM_RenderLayersColorOperation.h"

RenderLayersColorOperation::RenderLayersColorOperation() :RenderLayersBaseProg(SCE_PASS_RGBA, 4) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
}
