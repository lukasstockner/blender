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

#ifndef AUD_IDEVICE
#define AUD_IDEVICE

#include "AUD_Space.h"
class AUD_IFactory;

/// Handle structure, for inherition.
typedef struct
{
} AUD_Handle;

/**
 * This class represents an output device for sound sources.
 * Output devices may be several backends such as plattform independand like
 * SDL or OpenAL or plattform specific like DirectSound, but they may also be
 * files, RAM buffers or other types of streams.
 * \warning Thread safety must be insured so that no reader is beeing called
 *          twice at the same time.
 */
class AUD_IDevice
{
public:
	/**
	 * Returns the specification of the device.
	 */
	virtual AUD_Specs getSpecs()=0;

	/**
	 * Plays a sound source.
	 * \param factory The factory to create the reader for the sound source.
	 * \param endBehaviour The behaviour of the device when the sound source
	 *                     doesn't return any more data.
	 * \param seekTo A seek value for the start of the playback.
	 * \return Returns a handle with which the playback can be controlled.
	 *         This is NULL if the sound couldn't be played back.
	 * \exception AUD_Exception Thrown if there's an unexpected (from the
	 *            device side) error during creation of the reader.
	 */
	virtual AUD_Handle* play(AUD_IFactory* factory,
							 AUD_EndBehaviour endBehaviour = AUD_BEHAVIOUR_STOP,
							 int seekTo = 0)=0;

	/**
	 * Pauses a played back sound.
	 * \param handle The handle returned by the play function.
	 * \return
	 *        - true if the sound has been paused.
	 *        - false if the sound isn't playing back or the handle is invalid.
	 */
	virtual bool pause(AUD_Handle* handle)=0;

	/**
	 * Resumes a paused sound.
	 * \param handle The handle returned by the play function.
	 * \return
	 *        - true if the sound has been resumed.
	 *        - false if the sound isn't paused or the handle is invalid.
	 */
	virtual bool resume(AUD_Handle* handle)=0;

	/**
	 * Stops a played back or paused sound.
	 * \param handle The handle returned by the play function.
	 * \return
	 *        - true if the sound has been stopped.
	 *        - false if the handle is invalid.
	 */
	virtual bool stop(AUD_Handle* handle)=0;

	/**
	 * Sets the behaviour of the device for a played back sound when the sound
	 * doesn't return any more samples.
	 * \param handle The handle returned by the play function.
	 * \param endBehaviour The new behaviour.
	 * \return
	 *        - true if the behaviour has been changed.
	 *        - false if the handle is invalid.
	 * \see AUD_EndBehaviour
	 */
	virtual bool setEndBehaviour(AUD_Handle* handle,
								 AUD_EndBehaviour endBehaviour)=0;

	/**
	 * Seeks in a played back sound.
	 * \param handle The handle returned by the play function.
	 * \param position The new position from where to play back, measured in
	 *        samples. To get this value simply multiply the desired time in
	 *        seconds with the sample rate of the device.
	 *        A negative value indicates that the playback should be started
	 *        from the start after -position samples.
	 * \return
	 *        - true if the handle is valid.
	 *        - false if the handle is invalid.
	 * \warning Whether the seek works or not depends on the sound source.
	 */
	virtual bool seek(AUD_Handle* handle, int position)=0;

	/**
	 * Returns the status of a played back sound.
	 * \param handle The handle returned by the play function.
	 * \return
	 *        - AUD_STATUS_INVALID if the sound has stopped or the handle is
	 *.         invalid
	 *        - AUD_STATUS_PLAYING if the sound is currently played back.
	 *        - AUD_STATUS_PAUSED if the sound is currently paused.
	 * \see AUD_Status
	 */
	virtual AUD_Status getStatus(AUD_Handle* handle)=0;

	/**
	 * Locks the device.
	 * Used to make sure that between lock and unlock, no buffers are read, so
	 * that it is possible to start, resume, pause, stop or seek several
	 * playback handles simultaneously.
	 * \warning Make sure the locking time is as small as possible to avoid
	 *          playback delays that result in unexpected noise and cracks.
	 */
	virtual void lock()=0;

	/**
	 * Unlocks the previously locked device.
	 */
	virtual void unlock()=0;
};

#endif //AUD_IDevice
