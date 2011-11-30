#include "COM_QualityStepHelper.h"

QualityStepHelper::QualityStepHelper() {
	this->quality = COM_QUALITY_HIGH;
	this->step = 1;
	this->offsetadd = 4;
}

void QualityStepHelper::initExecution(QualityHelper helper) {
	switch (helper) {
	case COM_QH_INCREASE:
		switch (this->quality) {
		case COM_QUALITY_HIGH:
		default:
			this->step = 1;
			this->offsetadd = 4;
			break;
		case COM_QUALITY_MEDIUM:
			this->step = 2;
			this->offsetadd = 8;
			break;
		case COM_QUALITY_LOW:
			this->step = 3;
			this->offsetadd = 12;
			break;
		}
		break;
	case COM_QH_MULTIPLY:
		switch (this->quality) {
		case COM_QUALITY_HIGH:
		default:
			this->step = 1;
			this->offsetadd = 4;
			break;
		case COM_QUALITY_MEDIUM:
			this->step = 2;
			this->offsetadd = 8;
			break;
		case COM_QUALITY_LOW:
			this->step = 4;
			this->offsetadd = 16;
			break;
		}
		break;
	}
}

