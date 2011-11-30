#include "COM_ViewerBaseOperation.h"
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


ViewerBaseOperation::ViewerBaseOperation() : NodeOperation() {
//    this->setCpuBound(true);
    this->setImage(NULL);
    this->setImageUser(NULL);
    this->outputBuffer = NULL;
    this->outputBufferDisplay = NULL;
    this->active = false;
    this->doColorManagement = true;
}

void ViewerBaseOperation::initExecution() {
    // When initializing the tree during initial load the width and height can be zero.
    initImage();
}

void ViewerBaseOperation::initImage() {
    Image* anImage = this->image;
    ImBuf *ibuf= BKE_image_acquire_ibuf(anImage, NULL, &this->lock);

    if (!ibuf) return;
    if (ibuf->x != (int)getWidth() || ibuf->y != (int)getHeight()) {
        imb_freerectImBuf(ibuf);
        imb_freerectfloatImBuf(ibuf);
        IMB_freezbuffloatImBuf(ibuf);
        ibuf->x= getWidth();
        ibuf->y= getHeight();
        imb_addrectImBuf(ibuf);
        imb_addrectfloatImBuf(ibuf);
        anImage->ok= IMA_OK_LOADED;
    }

    /* now we combine the input with ibuf */
    this->outputBuffer = ibuf->rect_float;
    this->outputBufferDisplay = (unsigned char*)ibuf->rect;

    BKE_image_release_ibuf(this->image, this->lock);
}
void ViewerBaseOperation:: updateImage(rcti *rect) {
	/// @todo: introduce new event to update smaller area
	WM_main_add_notifier(NC_WINDOW|ND_DRAW, NULL);
}

void ViewerBaseOperation::deinitExecution() {
    this->outputBuffer = NULL;
}

const int ViewerBaseOperation::getRenderPriority() const {
	if (this->isActiveViewerOutput()) {
		return 8;
	} else {
		return 0;
	}
}
