/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

//#define AUD_CAPI_IMPLEMENTATION
//#include "AUD_C-API.h"
#include "AUD_FFMPEGFactory.h"
#include "AUD_SDLDevice.h"

extern "C" {
#include <libavformat/avformat.h>
}
#include <assert.h>

typedef AUD_IFactory AUD_Sound;

static AUD_IDevice* AUD_device = NULL;

bool AUD_init()
{
	av_register_all();
	try
	{
		AUD_device = new AUD_SDLDevice();
		return true;
	}
	catch(AUD_Exception e)
	{
		return false;
	}
}

void AUD_exit()
{
	assert(AUD_device);
	delete AUD_device;
}

AUD_Sound* AUD_load(const char* filename)
{
	assert(filename);
	return new AUD_FFMPEGFactory(filename);
}

void AUD_unload(AUD_Sound* sound)
{
	assert(sound);
	delete sound;
}

AUD_Handle* AUD_play(AUD_Sound* sound,
					 AUD_EndBehaviour endBehaviour,
					 double seekTo)
{
	assert(AUD_device);
	assert(sound);
	int position = (int)(seekTo * AUD_device->getSpecs().rate);
	try
	{
		return AUD_device->play(sound, endBehaviour, position);
	}
	catch(AUD_Exception e)
	{
		return NULL;
	}
}

bool AUD_pause(AUD_Handle* handle)
{
	assert(AUD_device);
	return AUD_device->pause(handle);
}

bool AUD_resume(AUD_Handle* handle)
{
	assert(AUD_device);
	return AUD_device->resume(handle);
}

bool AUD_stop(AUD_Handle* handle)
{
	assert(AUD_device);
	return AUD_device->stop(handle);
}

bool AUD_setEndBehaviour(AUD_Handle* handle,
						 AUD_EndBehaviour endBehaviour)
{
	assert(AUD_device);
	return AUD_device->setEndBehaviour(handle, endBehaviour);
}

bool AUD_seek(AUD_Handle* handle, double seekTo)
{
	assert(AUD_device);
	int position = (int)(seekTo * AUD_device->getSpecs().rate);
	return AUD_device->seek(handle, position);
}

AUD_Status AUD_getStatus(AUD_Handle* handle)
{
	assert(AUD_device);
	return AUD_device->getStatus(handle);
}
