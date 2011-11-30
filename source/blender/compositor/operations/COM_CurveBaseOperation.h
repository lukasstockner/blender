#ifndef _COM_CurveBaseOperation_h
#define _COM_CurveBaseOperation_h
#include "COM_NodeOperation.h"
#include "DNA_color_types.h"

class CurveBaseOperation : public NodeOperation {
protected:
    /**
      * Cached reference to the inputProgram
      */
	CurveMapping *curveMapping;
public:
	CurveBaseOperation();

	/**
      * Initialize the execution
      */
    void initExecution();

	void setCurveMapping(CurveMapping* mapping) {this->curveMapping = mapping;}
};
#endif
