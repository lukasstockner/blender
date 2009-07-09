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

#include "AUD_SDLDevice.h"
#include "AUD_SDLMixerFactory.h"
#include "AUD_IReader.h"
#include "AUD_SDLSuperposer.h"

#include <SDL.h>

// this is the callback function for SDL, it only calls the class
void mixAudio(void *data, Uint8* buffer, int length)
{
	AUD_SDLDevice* device = (AUD_SDLDevice*)data;
	device->SDLmix((sample_t *)buffer, length);
}

void AUD_SDLDevice::init()
{
	SDL_AudioSpec format, obtained;

	// SDL only supports mono and stereo
	if(m_specs.channels > 2)
		m_specs.channels = AUD_CHANNELS_STEREO;

	format.freq = m_specs.rate;
	if(m_specs.format == AUD_FORMAT_U8)
		format.format = AUDIO_U8;
	else
		format.format = AUDIO_S16SYS;
	format.channels = m_specs.channels;
	format.samples = 1024;
	format.callback = &mixAudio;
	format.userdata = this;

	if(SDL_OpenAudio(&format, &obtained) != 0)
		AUD_THROW(AUD_ERROR_SDL);

	m_specs.rate = (AUD_SampleRate)obtained.freq;
	m_specs.channels = (AUD_Channels)obtained.channels;
	if(obtained.format == AUDIO_U8)
		m_specs.format = AUD_FORMAT_U8;
	else if(obtained.format == AUDIO_S16LSB || obtained.format == AUDIO_S16MSB)
		m_specs.format = AUD_FORMAT_S16;
	else
		AUD_THROW(AUD_ERROR_SDL);

	m_mixer = new AUD_SDLMixerFactory(m_specs);
	m_superposer = new AUD_SDLSuperposer();

	create();
}

AUD_SDLDevice::AUD_SDLDevice(AUD_Specs specs)
{
	m_specs = specs;

	init();
}

AUD_SDLDevice::AUD_SDLDevice()
{
	m_specs.channels = AUD_CHANNELS_STEREO;
	m_specs.format = AUD_FORMAT_S16;
	m_specs.rate = AUD_RATE_44100;

	init();
}

AUD_SDLDevice::~AUD_SDLDevice()
{
	destroy();

	SDL_CloseAudio();
}

void AUD_SDLDevice::SDLmix(sample_t* buffer, int length)
{
	mix(buffer, length/AUD_SAMPLE_SIZE(m_specs));
}

void AUD_SDLDevice::playing(bool playing)
{
	SDL_PauseAudio(playing ? 0 : 1);
}
