#include "COM_RenderLayersMistOperation.h"

RenderLayersMistOperation::RenderLayersMistOperation() :RenderLayersBaseProg(SCE_PASS_MIST, 1) {
    this->addOutputSocket(COM_DT_VALUE);
}
