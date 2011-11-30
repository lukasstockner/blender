#ifndef _COM_SplitViewerOperation_h
#define _COM_SplitViewerOperation_h
#include "COM_ViewerBaseOperation.h"
#include "DNA_image_types.h"
#include "BLI_rect.h"

class SplitViewerOperation : public ViewerBaseOperation {
private:
	SocketReader* image1Input;
	SocketReader* image2Input;

    float splitPercentage;
    bool xSplit;
public:
    SplitViewerOperation();
    void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers);
    void initExecution();
    void deinitExecution();
    void setSplitPercentage(float splitPercentage) {this->splitPercentage = splitPercentage;}
    void setXSplit(bool xsplit) {this->xSplit = xsplit;}
};
#endif
