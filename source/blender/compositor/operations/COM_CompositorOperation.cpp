#include "COM_CompositorOperation.h"
#include "COM_SocketConnection.h"
#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "BKE_image.h"

extern "C" {
	#include "RE_pipeline.h"
	#include "RE_shader_ext.h"
	#include "RE_render_ext.h"
	#include "MEM_guardedalloc.h"
#include "render_types.h"
}
#include "PIL_time.h"


CompositorOperation::CompositorOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);

	this->setScene(NULL);
	this->outputBuffer = NULL;
	this->imageInput = NULL;
	this->alphaInput = NULL;
}

void CompositorOperation::initExecution() {
	// When initializing the tree during initial load the width and height can be zero.
	this->imageInput = getInputSocketReader(0);
	this->alphaInput = getInputSocketReader(1);
	initImage();
}

void CompositorOperation::initImage() {
	const Scene * scene = this->scene;
	Render* re= RE_GetRender(scene->id.name);
	RenderResult *rr= RE_AcquireResultWrite(re);
	if(rr) {

		if(rr->rectf  != NULL) {
			MEM_freeN(rr->rectf);
		}
		if (this->getWidth() * this->getHeight() != 0) {
			this->outputBuffer=(float*) MEM_mapallocN(this->getWidth()*this->getHeight()*4*sizeof(float), "CompositorOperation");
		}
		rr->rectf= outputBuffer;
	}
	if (re) {
		RE_ReleaseResult(re);
		re = NULL;
	}
}

void CompositorOperation::deinitExecution() {
	this->outputBuffer = NULL;
	this->imageInput = NULL;
	this->alphaInput = NULL;
}


void CompositorOperation::executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers) {
	float* buffer = this->outputBuffer;

	if (!buffer) return;
	int x1 = rect->xmin;
	int y1 = rect->ymin;
	int x2 = rect->xmax;
	int y2 = rect->ymax;
	int offset = (y1*this->getWidth() + x1 ) * 4;
	int x;
	int y;
	bool breaked = false;

	for (y = y1 ; y < y2 && (!breaked); y++) {
		for (x = x1 ; x < x2 && (!breaked) ; x++) {
			imageInput->read(&(buffer[offset]), x, y, memoryBuffers);
			if (alphaInput != NULL) {
				alphaInput->read(&(buffer[offset+3]), x, y, memoryBuffers);
			}
			offset +=4;
			if (tree->test_break && tree->test_break(tree->tbh)) {
				breaked = true;
			}

		}
		offset += (this->getWidth()-(x2-x1))*4;
	}
}

