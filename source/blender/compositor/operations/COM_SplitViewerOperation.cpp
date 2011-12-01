#include "COM_SplitViewerOperation.h"
#include "COM_SocketConnection.h"
#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "BKE_image.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"

extern "C" {
    #include "MEM_guardedalloc.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
}


SplitViewerOperation::SplitViewerOperation() : ViewerBaseOperation() {
    this->addInputSocket(COM_DT_COLOR);
    this->addInputSocket(COM_DT_COLOR);
    this->image1Input = NULL;
    this->image2Input = NULL;
}

void SplitViewerOperation::initExecution() {
    // When initializing the tree during initial load the width and height can be zero.
	this->image1Input = getInputSocketReader(0);
	this->image2Input = getInputSocketReader(1);
    ViewerBaseOperation::initExecution();
}

void SplitViewerOperation::deinitExecution() {
    this->image1Input = NULL;
    this->image2Input = NULL;
    ViewerBaseOperation::deinitExecution();
}


void SplitViewerOperation::executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers) {
    float* buffer = this->outputBuffer;
    unsigned char* bufferDisplay = this->outputBufferDisplay;

    if (!buffer) return;
    int x1 = rect->xmin;
    int y1 = rect->ymin;
    int x2 = rect->xmax;
    int y2 = rect->ymax;
    int offset = (y1*this->getWidth() + x1 ) * 4;
    int x;
    int y;
    int perc = xSplit?this->splitPercentage*getWidth()/100.0f:this->splitPercentage*getHeight()/100.0f;
    for (y = y1 ; y < y2 ; y++) {
        for (x = x1 ; x < x2 ; x++) {
            bool image1;
            image1 = xSplit?x>perc:y>perc;
            if (image1) {
				image1Input->read(&(buffer[offset]), x, y, memoryBuffers);
            } else {
				image2Input->read(&(buffer[offset]), x, y, memoryBuffers);
            }
            /// @todo: linear conversion only when scene color management is selected.
            if (this->doColorManagement) {
                bufferDisplay[offset] = FTOCHAR(linearrgb_to_srgb(buffer[offset]));
                bufferDisplay[offset+1] = FTOCHAR(linearrgb_to_srgb(buffer[offset+1]));
                bufferDisplay[offset+2] = FTOCHAR(linearrgb_to_srgb(buffer[offset+2]));
                bufferDisplay[offset+3] = FTOCHAR(buffer[offset+3]);
            } else {
                bufferDisplay[offset] = FTOCHAR((buffer[offset]));
                bufferDisplay[offset+1] = FTOCHAR((buffer[offset+1]));
                bufferDisplay[offset+2] = FTOCHAR((buffer[offset+2]));
                bufferDisplay[offset+3] = FTOCHAR(buffer[offset+3]);
            }

            offset +=4;
        }
        offset += (this->getWidth()-(x2-x1))*4;
    }
    updateImage(rect);
}

