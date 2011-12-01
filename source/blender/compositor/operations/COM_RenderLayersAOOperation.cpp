#include "COM_RenderLayersAOOperation.h"

RenderLayersAOOperation::RenderLayersAOOperation() :RenderLayersBaseProg(SCE_PASS_AO, 3) {
    this->addOutputSocket(COM_DT_COLOR);
}

