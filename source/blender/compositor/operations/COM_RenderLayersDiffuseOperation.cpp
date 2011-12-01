#include "COM_RenderLayersDiffuseOperation.h"

RenderLayersDiffuseOperation::RenderLayersDiffuseOperation() :RenderLayersBaseProg(SCE_PASS_DIFFUSE, 3) {
    this->addOutputSocket(COM_DT_COLOR);
}
