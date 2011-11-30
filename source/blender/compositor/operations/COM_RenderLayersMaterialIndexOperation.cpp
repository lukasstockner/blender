#include "COM_RenderLayersMaterialIndexOperation.h"

RenderLayersMaterialIndexOperation::RenderLayersMaterialIndexOperation() :RenderLayersBaseProg(SCE_PASS_INDEXMA, 1) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
}
