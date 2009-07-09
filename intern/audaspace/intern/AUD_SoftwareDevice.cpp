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

#include "AUD_SoftwareDevice.h"
#include "AUD_IReader.h"
#include "AUD_MixerFactory.h"
#include "AUD_ISuperposer.h"

#include <cstring>

/// Saves the data for playback.
struct AUD_SoftwareHandle : AUD_Handle
{
	/// The reader source.
	AUD_IReader* reader;

	/// The behaviour if end of the source is reached.
	AUD_EndBehaviour endBehaviour;

	/// Sample count until playback starts.
	int delay;
};

void AUD_SoftwareDevice::create()
{
	m_playingSounds = new std::list<AUD_SoftwareHandle*>();
	m_pausedSounds = new std::list<AUD_SoftwareHandle*>();
	m_playback = false;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_mutex, &attr);

	pthread_mutexattr_destroy(&attr);
}

void AUD_SoftwareDevice::destroy()
{
	if(m_playback)
		playing(m_playback = false);

	delete m_mixer;

	delete m_superposer;

	// delete all playing sounds
	while(m_playingSounds->begin() != m_playingSounds->end())
	{
		delete (*(m_playingSounds->begin()))->reader; AUD_DELETE("reader")
		delete *(m_playingSounds->begin());
		m_playingSounds->erase(m_playingSounds->begin());
	}
	delete m_playingSounds;

	// delete all paused sounds
	while(m_pausedSounds->begin() != m_pausedSounds->end())
	{
		delete (*(m_pausedSounds->begin()))->reader; AUD_DELETE("reader")
		delete *(m_pausedSounds->begin());
		m_pausedSounds->erase(m_pausedSounds->begin());
	}
	delete m_pausedSounds;

	pthread_mutex_destroy(&m_mutex);
}

void AUD_SoftwareDevice::mix(sample_t* buffer, int length)
{
	pthread_mutex_lock(&m_mutex);

	AUD_SoftwareHandle* sound;
	int len, left, pos;
	unsigned char* buf;
	int sample_size = AUD_SAMPLE_SIZE(m_specs);
	bool looped;

	// fill with silence
	memset(buffer, 0, length*sample_size);

	// for all sounds
	std::list<AUD_SoftwareHandle*>::iterator it = m_playingSounds->begin();
	while(it != m_playingSounds->end())
	{
		sound = *it;
		// increment the iterator to make sure it's valid,
		// in case the sound gets deleted after stopping
		++it;

		pos = sound->delay;

		// check for playback start
		if(pos >= length)
		{
			sound->delay -= length;
			continue;
		}

		// remove delay if exists
		if(pos > 0)
			sound->delay = 0;

		left = length-pos;
		looped = false;
		while(left > 0)
		{
			// get the buffer from the source
			len = left;
			sound->reader->read(len, buf);
			left -= len;

			if(len == 0)
			{
				// prevent endless loop
				if(looped)
				{
					stop(sound);
					break;
				}
			}
			else
				looped = false;

			m_superposer->superpose(buffer + pos*sample_size,
									buf,
									len*sample_size);

			pos += len;

			// in case the end of the sound is reached
			if(left > 0)
			{
				// looping requires us to seek back
				if(sound->endBehaviour == AUD_BEHAVIOUR_LOOP)
				{
					sound->reader->seek(0);
					looped = true;
				}
				else
				{
					if(sound->endBehaviour == AUD_BEHAVIOUR_PAUSE)
						pause(sound);
					else
						stop(sound);
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&m_mutex);
}

AUD_Specs AUD_SoftwareDevice::getSpecs()
{
	return m_specs;
}

AUD_Handle* AUD_SoftwareDevice::play(AUD_IFactory* factory,
							 AUD_EndBehaviour endBehaviour,
							 int seekTo)
{
	AUD_IReader* reader = factory->createReader();

	if(reader == NULL)
		AUD_THROW(AUD_ERROR_READER);

	// check if the reader needs to be mixed
	AUD_Specs specs = reader->getSpecs();
	if(memcmp(&m_specs, &specs, sizeof(AUD_Specs)) != 0)
	{
		m_mixer->setReader(reader);
		reader = m_mixer->createReader();
		if(reader == NULL)
			return NULL;
	}

	AUD_Specs rs = reader->getSpecs();

	// play sound
	AUD_SoftwareHandle* sound = new AUD_SoftwareHandle;
	sound->endBehaviour = endBehaviour;
	sound->reader = reader;
	sound->delay = 0;

	if(seekTo > 0)
		reader->seek(seekTo);
	else if(seekTo < 0)
		sound->delay = -seekTo;

	pthread_mutex_lock(&m_mutex);
	m_playingSounds->push_back(sound);

	if(!m_playback)
		playing(m_playback = true);
	pthread_mutex_unlock(&m_mutex);

	return sound;
}

bool AUD_SoftwareDevice::pause(AUD_Handle* handle)
{
	// only songs that are played can be paused
	pthread_mutex_lock(&m_mutex);
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
	{
		if(*i == handle)
		{
			m_pausedSounds->push_back(*i);
			m_playingSounds->erase(i);
			if(m_playingSounds->empty())
				playing(m_playback = false);
			pthread_mutex_unlock(&m_mutex);
			return true;
		}
	}
	pthread_mutex_unlock(&m_mutex);
	return false;
}

bool AUD_SoftwareDevice::resume(AUD_Handle* handle)
{
	// only songs that are paused can be resumed
	pthread_mutex_lock(&m_mutex);
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_pausedSounds->begin();
		i != m_pausedSounds->end(); i++)
	{
		if(*i == handle)
		{
			m_playingSounds->push_back(*i);
			m_pausedSounds->erase(i);
			if(!m_playback)
				playing(m_playback = true);
			pthread_mutex_unlock(&m_mutex);
			return true;
		}
	}
	pthread_mutex_unlock(&m_mutex);
	return false;
}

bool AUD_SoftwareDevice::stop(AUD_Handle* handle)
{
	pthread_mutex_lock(&m_mutex);
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
	{
		if(*i == handle)
		{
			delete (*i)->reader; AUD_DELETE("reader")
			delete *i;
			m_playingSounds->erase(i);
			if(m_playingSounds->empty())
				playing(m_playback = false);
			pthread_mutex_unlock(&m_mutex);
			return true;
		}
	}
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_pausedSounds->begin();
		i != m_pausedSounds->end(); i++)
	{
		if(*i == handle)
		{
			delete (*i)->reader; AUD_DELETE("reader")
			delete *i;
			m_pausedSounds->erase(i);
			pthread_mutex_unlock(&m_mutex);
			return true;
		}
	}
	pthread_mutex_unlock(&m_mutex);
	return false;
}

bool AUD_SoftwareDevice::setEndBehaviour(AUD_Handle* handle,
									AUD_EndBehaviour endBehaviour)
{
	pthread_mutex_lock(&m_mutex);
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
	{
		if(*i == handle)
		{
			(*i)->endBehaviour = endBehaviour;
			pthread_mutex_unlock(&m_mutex);
			return true;
		}
	}
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_pausedSounds->begin();
		i != m_pausedSounds->end(); i++)
	{
		if(*i == handle)
		{
			(*i)->endBehaviour = endBehaviour;
			pthread_mutex_unlock(&m_mutex);
			return true;
		}
	}
	pthread_mutex_unlock(&m_mutex);
	return false;
}

bool AUD_SoftwareDevice::seek(AUD_Handle* handle, int position)
{
	pthread_mutex_lock(&m_mutex);
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
	{
		if(*i == handle)
		{
			(*i)->reader->seek(position);
			pthread_mutex_unlock(&m_mutex);
			return true;
		}
	}
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_pausedSounds->begin();
		i != m_pausedSounds->end(); i++)
	{
		if(*i == handle)
		{
			(*i)->reader->seek(position);
			pthread_mutex_unlock(&m_mutex);
			return true;
		}
	}
	pthread_mutex_unlock(&m_mutex);
	return false;
}

AUD_Status AUD_SoftwareDevice::getStatus(AUD_Handle* handle)
{
	pthread_mutex_lock(&m_mutex);
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
	{
		if(*i == handle)
		{
			pthread_mutex_unlock(&m_mutex);
			return AUD_STATUS_PLAYING;
		}
	}
	for(std::list<AUD_SoftwareHandle*>::iterator i = m_pausedSounds->begin();
		i != m_pausedSounds->end(); i++)
	{
		if(*i == handle)
		{
			pthread_mutex_unlock(&m_mutex);
			return AUD_STATUS_PAUSED;
		}
	}
	pthread_mutex_unlock(&m_mutex);
	return AUD_STATUS_INVALID;
}

void AUD_SoftwareDevice::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void AUD_SoftwareDevice::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}
