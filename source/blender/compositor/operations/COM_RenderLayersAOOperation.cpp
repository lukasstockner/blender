#include "COM_RenderLayersAOOperation.h"

RenderLayersAOOperation::RenderLayersAOOperation() :RenderLayersBaseProg(SCE_PASS_AO, 3) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
}

