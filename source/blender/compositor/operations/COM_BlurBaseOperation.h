#ifndef _COM_BlurBaseOperation_h
#define _COM_BlurBaseOperation_h
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

class BlurBaseOperation : public NodeOperation, public QualityStepHelper {
private:

protected:
	/**
	  * Cached reference to the inputProgram
	  */
	SocketReader* inputProgram;
	NodeBlurData * data;
	BlurBaseOperation();
	float* make_gausstab(int rad);
	float size;

public:
    /**
      * Initialize the execution
      */
    void initExecution();

    /**
      * Deinitialize the execution
      */
    void deinitExecution();

	void setData(NodeBlurData* data) {this->data= data;}

	void setSize(float size) {this->size = size;}
};
#endif
