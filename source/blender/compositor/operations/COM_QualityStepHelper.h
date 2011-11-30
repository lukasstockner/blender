#ifndef _COM_QualityStepHelper_h
#define _COM_QualityStepHelper_h
#include "COM_defines.h"

typedef enum QualityHelper {
	COM_QH_INCREASE,
	COM_QH_MULTIPLY
} QualityHelper;

class QualityStepHelper  {
private:
	CompositorQuality quality;
	int step;
	int offsetadd;

protected:
	/**
	   * Initialize the execution
	   */
	 void initExecution(QualityHelper helper);

	 inline int getStep() const {return this->step;}
	 inline int getOffsetAdd() const {return this->offsetadd;}

public:
	QualityStepHelper();


	void setQuality(CompositorQuality quality) {this->quality = quality;}
};
#endif
