#include "COM_ViewerOperation.h"
#include "COM_SocketConnection.h"
#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "BKE_image.h"
#include "WM_api.h"
#include "WM_types.h"
#include "PIL_time.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"

extern "C" {
	#include "MEM_guardedalloc.h"
	#include "IMB_imbuf.h"
	#include "IMB_imbuf_types.h"
}


ViewerOperation::ViewerOperation() : ViewerBaseOperation() {
	this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
	this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));

	this->imageInput = NULL;
	this->alphaInput = NULL;
}

void ViewerOperation::initExecution() {
	// When initializing the tree during initial load the width and height can be zero.
	this->imageInput = getInputSocketReader(0);
	this->alphaInput = getInputSocketReader(1);
	ViewerBaseOperation::initExecution();
}

void ViewerOperation::deinitExecution() {
	this->imageInput = NULL;
	this->alphaInput = NULL;
	ViewerBaseOperation::deinitExecution();
}


void ViewerOperation::executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers) {
	float* buffer = this->outputBuffer;
	unsigned char* bufferDisplay = this->outputBufferDisplay;
	if (!buffer) return;
	const int x1 = rect->xmin;
	const int y1 = rect->ymin;
	const int x2 = rect->xmax;
	const int y2 = rect->ymax;
	const int offsetadd = (this->getWidth()-(x2-x1))*4;
	int offset = (y1*this->getWidth() + x1 ) * 4;
	float alpha[4];
	int x;
	int y;
	bool breaked = false;

	for (y = y1 ; y < y2 && (!breaked) ; y++) {
		for (x = x1 ; x < x2; x++) {
			imageInput->read(&(buffer[offset]), x, y, memoryBuffers);
			if (alphaInput != NULL) {
				alphaInput->read(alpha, x, y, memoryBuffers);
				buffer[offset+3] = alpha[0];
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
		if (tree->test_break && tree->test_break(tree->tbh)) {
			breaked = true;
		}

		offset += offsetadd;
	}
	updateImage(rect);
}
