#ifndef _COM_ViewerOperation_h
#define _COM_ViewerOperation_h
#include "COM_NodeOperation.h"
#include "DNA_image_types.h"
#include "BLI_rect.h"
#include "COM_ViewerBaseOperation.h"

class ViewerOperation : public ViewerBaseOperation {
private:
	SocketReader* imageInput;
	SocketReader* alphaInput;

public:
    ViewerOperation();
    void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers);
    void initExecution();
    void deinitExecution();
};
#endif
