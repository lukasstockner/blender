#include "COM_ChannelInfo.h"
#include "COM_defines.h"
#include <stdio.h>

/**
  * @brief create new ChannelInfo instance and sets the defaults.
  */
ChannelInfo::ChannelInfo() {
	this->number = 0;
	this->premultiplied = true;
	this->type = COM_CT_UNUSED;
}
