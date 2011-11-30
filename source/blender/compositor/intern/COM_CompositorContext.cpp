#include "COM_CompositorContext.h"
#include "COM_defines.h"
#include <stdio.h>

CompositorContext::CompositorContext() {
	this->scene = NULL;
	this->quality = COM_QUALITY_HIGH;
	this->hasActiveOpenCLDevices = false;
}

const int CompositorContext::getFramenumber() const {
	if (this->scene) {
		return this->scene->r.cfra;
	} else {
		return -1; /* this should never happen */
	}
}
