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

//#include "AUD_C-API.h"
#include "AUD_FFMPEGFactory.h"
#include "AUD_SDLDevice.h"

extern "C" {
#include <libavformat/avformat.h>
}
#include <assert.h>

typedef AUD_IDevice AUD_Device;
typedef AUD_IFactory AUD_Sound;

AUD_Device* AUD_init()
{
	av_register_all();
	return new AUD_SDLDevice();
}

void AUD_exit(AUD_Device* device)
{
	assert(device);
	delete device;
}

AUD_Sound* openSound(const char* filename)
{
	assert(filename);
	return new AUD_FFMPEGFactory(filename);
}

void closeSound(AUD_Sound* sound)
{
	assert(sound);
	delete sound;
}

void playSound(AUD_Device* device, AUD_Sound* sound)
{
	assert(device);
	assert(sound);
	device->play(sound);
}
