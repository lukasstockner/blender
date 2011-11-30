#include "COM_RenderLayersShadowOperation.h"

RenderLayersShadowOperation::RenderLayersShadowOperation() :RenderLayersBaseProg(SCE_PASS_SHADOW, 3) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
}
