#ifndef _COM_PreviewOperation_h
#define _COM_PreviewOperation_h
#include "COM_NodeOperation.h"
#include "DNA_image_types.h"
#include "BLI_rect.h"

class PreviewOperation : public NodeOperation {
protected:
    unsigned char *outputBuffer;

	/**
	  * @brief holds reference to the SDNA bNode, where this nodes will render the preview image for
	  */
	bNode* node;
	const bNodeTree* tree;
	SocketReader* input;
    float divider;
	int priority;

public:
    PreviewOperation();
	bool isOutputOperation(bool rendering) const {return !rendering;}
    void initExecution();
    void deinitExecution();
	const int getRenderPriority() const;

    void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer **memoryBuffers);
    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	void setbNode(bNode* node) { this->node = node;}
	void setbNodeTree(const bNodeTree* tree) { this->tree = tree;}
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void setPriority(int priority) { this->priority = priority; }
};
#endif
