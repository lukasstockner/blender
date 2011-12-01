#include "COM_PreviewOperation.h"
#include "COM_SocketConnection.h"
#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "BKE_image.h"
#include "WM_api.h"
#include "WM_types.h"
#include "PIL_time.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"
#include "COM_defines.h"
#include "BLI_math.h"
extern "C" {
    #include "MEM_guardedalloc.h"
    #include "IMB_imbuf.h"
    #include "IMB_imbuf_types.h"
}


PreviewOperation::PreviewOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
    this->outputBuffer = NULL;
    this->input = NULL;
    this->divider = 1.0f;
	this->node = NULL;
	this->priority = 0;
}

void PreviewOperation::initExecution() {
	this->input = getInputSocketReader(0);
	if (!this->node->preview) {
		this->node->preview = (bNodePreview*)MEM_callocN(sizeof(bNodePreview), "node preview");

    } else {
                if (this->getWidth() == (unsigned int)this->node->preview->xsize && this->getHeight() == (unsigned int)this->node->preview->ysize) {
			this->outputBuffer = this->node->preview->rect;
        } else {
        }
    }

    if (this->outputBuffer == NULL) {
        this->outputBuffer = (unsigned char*)MEM_callocN(sizeof(unsigned char)*4*getWidth()*getHeight(), "PreviewOperation");
		if(this->node->preview->rect) {
				MEM_freeN(this->node->preview->rect);
        }
		this->node->preview->xsize= getWidth();
		this->node->preview->ysize= getHeight();
		this->node->preview->rect= outputBuffer;
    }
}

void PreviewOperation::deinitExecution() {
    this->outputBuffer = NULL;
    this->input = NULL;
//    this->node->done=1;
}

void PreviewOperation::executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer **memoryBuffers) {
    int offset;
	float color[4];
	for (int y = rect->ymin ; y < rect->ymax ; y++) {
        offset = (y * getWidth() + rect->xmin)*4;
        for (int x = rect->xmin ; x < rect->xmax ; x++) {
            float rx = floor(x/divider);
            float ry = floor(y/divider);

			color[0] = 0.0f;
            color[1] = 0.0f;
            color[2] = 0.0f;
            color[3] = 1.0f;
			input->read(color, rx, ry, memoryBuffers);
            /// @todo: linear conversion only when scene color management is selected.
            outputBuffer[offset] = FTOCHAR(linearrgb_to_srgb(color[0]));
            outputBuffer[offset+1] = FTOCHAR(linearrgb_to_srgb(color[1]));
            outputBuffer[offset+2] = FTOCHAR(linearrgb_to_srgb(color[2]));
            outputBuffer[offset+3] = FTOCHAR(color[3]);
            offset +=4;
        }
    }
}
bool PreviewOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
    rcti newInput;

    newInput.xmin = input->xmin/divider;
    newInput.xmax = input->xmax/divider;
    newInput.ymin = input->ymin/divider;
    newInput.ymax = input->ymax/divider;

    return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
void PreviewOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
    NodeOperation::determineResolution(resolution, preferredResolution);
    int width = resolution[0];
    int height = resolution[1];
    this->divider = 0.0f;
    if (width > height) {
        divider = COM_PREVIEW_SIZE / (width-1);
    } else {
        divider = COM_PREVIEW_SIZE / (height-1);
    }
    width = width * divider;
    height = height * divider;

    resolution[0] = width;
    resolution[1] = height;
}

const int PreviewOperation::getRenderPriority() const {
	return this->priority;
}
