#include "COM_RenderLayersMaterialIndexOperation.h"

RenderLayersMaterialIndexOperation::RenderLayersMaterialIndexOperation() :RenderLayersBaseProg(SCE_PASS_INDEXMA, 1) {
    this->addOutputSocket(COM_DT_VALUE);
}
