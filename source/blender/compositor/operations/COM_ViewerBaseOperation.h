#ifndef _COM_ViewerBaseOperation_h
#define _COM_ViewerBaseOperation_h
#include "COM_NodeOperation.h"
#include "DNA_image_types.h"
#include "BLI_rect.h"

class ViewerBaseOperation : public NodeOperation {
protected:
    float *outputBuffer;
    unsigned char *outputBufferDisplay;
    Image * image;
    ImageUser * imageUser;
    void *lock;
    bool active;
	const bNodeTree* tree;
    float centerX;
    float centerY;
	OrderOfChunks chunkOrder;
    bool doColorManagement;

public:
	bool isOutputOperation(bool rendering) const {return !rendering;}
    void initExecution();
    void deinitExecution();
    void setImage(Image* image) {this->image = image;}
    void setImageUser(ImageUser* imageUser) {this->imageUser = imageUser;}
	const bool isActiveViewerOutput() const {return active;}
    void setActive(bool active) {this->active = active;}
	void setbNodeTree(const bNodeTree* tree) {this->tree = tree;}
    void setCenterX(float centerX) {this->centerX = centerX;}
    void setCenterY(float centerY) {this->centerY = centerY;}
	void setChunkOrder(OrderOfChunks tileOrder) {this->chunkOrder = tileOrder;}
    float getCenterX() { return this->centerX; }
    float getCenterY() { return this->centerY; }
	OrderOfChunks getChunkOrder() { return this->chunkOrder; }
	const int getRenderPriority() const;
        void setColorManagement(bool doColorManagement) {this->doColorManagement = doColorManagement;}
	bool isViewerOperation() {return true;}
		
protected:
    ViewerBaseOperation();
    void updateImage(rcti*rect);

private:
    void initImage();
};
#endif
