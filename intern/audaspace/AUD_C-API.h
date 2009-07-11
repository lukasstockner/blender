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

#ifndef AUD_CAPI
#define AUD_CAPI

#ifdef __cplusplus
extern "C" {
#endif

#include "intern/AUD_Space.h"

#ifndef AUD_CAPI_IMPLEMENTATION
	typedef void AUD_Sound;
	typedef void AUD_Handle;
#endif

/**
 * Initializes an audio device.
 * \return Whether the device has been initialized.
 */
extern int AUD_init();

/**
 * Unitinitializes an audio device.
 */
extern void AUD_exit();

/**
 * Loads a sound file.
 * \param filename The filename of the sound file.
 * \return A handle of the sound file.
 */
extern AUD_Sound* AUD_load(const char* filename);

/**
 * Loads a sound file.
 * \param buffer The buffer which contains the sound file.
 * \param size The size of the buffer.
 * \return A handle of the sound file.
 */
extern AUD_Sound* AUD_loadBuffer(unsigned char* buffer, int size);

/**
 * Unloads a sound file.
 * \param sound The handle of the sound file.
 */
extern void AUD_unload(AUD_Sound* sound);

/**
 * Plays back a sound file.
 * \param sound The handle of the sound file.
 * \param endBehaviour The behaviour after the end of the sound file has been
 *                     reached.
 * \param seekTo From where the sound file should be played back in seconds.
 *               A negative value indicates the seconds that should be waited
 *               before playback starts.
 * \return A handle to the played back sound.
 */
extern AUD_Handle* AUD_play(AUD_Sound* sound,
							AUD_EndBehaviour endBehaviour,
							double seekTo);

/**
 * Pauses a played back sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been playing or not.
 */
extern int AUD_pause(AUD_Handle* handle);

/**
 * Resumes a paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been paused or not.
 */
extern int AUD_resume(AUD_Handle* handle);

/**
 * Stops a playing or paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_stop(AUD_Handle* handle);

/**
 * Sets the end behaviour of a playing or paused sound.
 * \param handle The handle to the sound.
 * \param endBehaviour The behaviour after the end of the file has been reached.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_setEndBehaviour(AUD_Handle* handle,
								AUD_EndBehaviour endBehaviour);

/**
 * Seeks a playing or paused sound.
 * \param handle The handle to the sound.
 * \param seekTo From where the sound file should be played back in seconds.
 *               A negative value indicates the seconds that should be waited
 *               before playback starts.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_seek(AUD_Handle* handle, int seekTo);

/**
 * Returns the status of a playing, paused or stopped sound.
 * \param handle The handle to the sound.
 * \return The status of the sound behind the handle.
 */
extern AUD_Status AUD_getStatus(AUD_Handle* handle);

#ifdef __cplusplus
}
#endif

#endif //AUD_CAPI
