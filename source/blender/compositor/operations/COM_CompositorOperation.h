#ifndef _COM_CompositorOperation_h
#define _COM_CompositorOperation_h
#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "BLI_rect.h"

class CompositorOperation : public NodeOperation {
private:
	const Scene* scene;
	const bNodeTree* tree;
    float *outputBuffer;

	SocketReader* imageInput;
	SocketReader* alphaInput;
public:
    CompositorOperation();
    void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers);
	void setScene(const Scene* scene) {this->scene = scene;}
	void setbNodeTree(const bNodeTree* tree) {this->tree= tree;}
	bool isOutputOperation(bool rendering) const {return true;}
    void initExecution();
    void deinitExecution();
	const int getRenderPriority() const {return 7;};
private:
    void initImage();
};
#endif
