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

#ifndef AUD_ENUMS
#define AUD_ENUMS

/// The size of a sample in the specified format in bytes.
#define AUD_SAMPLE_SIZE(specs) (specs.channels * (specs.format & 0x0F))
/// Throws a AUD_Exception with the provided error code.
#define AUD_THROW(exception) { AUD_Exception e; e.error = exception; throw e; }

/// Returns the smaller of the two values.
#define AUD_MIN(a, b) (((a) < (b)) ? (a) : (b))
/// Returns the bigger of the two values.
#define AUD_MAX(a, b) (((a) > (b)) ? (a) : (b))

// 5 sec * 44100 samples/sec * 4 bytes/sample * 6 channels
/// The size by which a buffer should be resized if the final extent is unknown.
#define AUD_BUFFER_RESIZE_BYTES 5292000

/// Used for debugging memory leaks.
//#define AUD_DEBUG_MEMORY

#ifdef AUD_DEBUG_MEMORY
int AUD_References(int count = 0, const char* text = "");
#define AUD_NEW(text) AUD_References(1, text);
#define AUD_DELETE(text) AUD_References(-1, text);
#else
#define AUD_NEW(text)
#define AUD_DELETE(text)
#endif

/// The behaviour of the playback device for a source that reached its end.
typedef enum
{
	AUD_BEHAVIOUR_STOP,				/// Stop the sound (deletes the reader).
	AUD_BEHAVIOUR_PAUSE,			/// Pause the sound.
	AUD_BEHAVIOUR_LOOP				/// Seek to front and replay.
} AUD_EndBehaviour;

/**
 * The format of a sample.
 * The last 4 bit save the byte count of the format.
 */
typedef enum
{
	AUD_FORMAT_INVALID = 0x00,		/// Invalid sample format.
	AUD_FORMAT_U8      = 0x01,		/// 1 byte unsigned byte.
	AUD_FORMAT_S16     = 0x12,		/// 2 byte signed integer.
	AUD_FORMAT_S24     = 0x13,		/// 3 byte signed integer.
	AUD_FORMAT_S32     = 0x14,		/// 4 byte signed integer.
	AUD_FORMAT_S64     = 0x18,		/// 8 byte signed integer.
	AUD_FORMAT_FLOAT32 = 0x24,		/// 4 byte float.
	AUD_FORMAT_FLOAT64 = 0x28		/// 8 byte float.
} AUD_SampleFormat;

/// The channel count.
typedef enum
{
	AUD_CHANNELS_INVALID    = 0,	/// Invalid channel count.
	AUD_CHANNELS_MONO       = 1,	/// Mono.
	AUD_CHANNELS_STEREO     = 2,	/// Stereo.
	AUD_CHANNELS_STEREO_LFE = 3,	/// Stereo with LFE channel.
	AUD_CHANNELS_SURROUND4  = 4,	/// 4 channel surround sound.
	AUD_CHANNELS_SURROUND5  = 5,	/// 5 channel surround sound.
	AUD_CHANNELS_SURROUND51 = 6,	/// 5.1 surround sound.
	AUD_CHANNELS_SURROUND61 = 7,	/// 6.1 surround sound.
	AUD_CHANNELS_SURROUND71 = 8,	/// 7.1 surround sound.
	AUD_CHANNELS_SURROUND72 = 9		/// 7.2 surround sound.
} AUD_Channels;

/**
 * The sample rate tells how many samples are played back within one second.
 * Some exotic formats may use other sample rates than provided here.
 */
typedef enum
{
	AUD_RATE_INVALID = 0,			/// Invalid sample rate.
	AUD_RATE_8000    = 8000,		/// 8000 Hz.
	AUD_RATE_16000   = 16000,		/// 16000 Hz.
	AUD_RATE_11025   = 11025,		/// 11025 Hz.
	AUD_RATE_22050   = 22050,		/// 22050 Hz.
	AUD_RATE_32000   = 32000,		/// 32000 Hz.
	AUD_RATE_44100   = 44100,		/// 44100 Hz.
	AUD_RATE_48000   = 48000,		/// 48000 Hz.
	AUD_RATE_88200   = 88200,		/// 88200 Hz.
	AUD_RATE_96000   = 96000,		/// 96000 Hz.
	AUD_RATE_192000  = 192000		/// 192000 Hz.
} AUD_SampleRate;

/**
 * Type of a reader.
 * @see AUD_IReader for details.
 */
typedef enum
{
	AUD_TYPE_INVALID = 0,			/// Invalid reader type.
	AUD_TYPE_BUFFER,				/// Reader reads from a buffer.
	AUD_TYPE_STREAM					/// Reader reads from a stream.
} AUD_ReaderType;

/// Status of a playback handle.
typedef enum
{
	AUD_STATUS_INVALID = 0,			/// Invalid handle. Maybe due to stopping.
	AUD_STATUS_PLAYING,				/// Sound is playing.
	AUD_STATUS_PAUSED				/// Sound is being paused.
} AUD_Status;

/// Error codes for exceptions (C++ library) or for return values (C API).
typedef enum
{
	AUD_NO_ERROR = 0,
	AUD_ERROR_READER,
	AUD_ERROR_FACTORY,
	AUD_ERROR_FILE,
	AUD_ERROR_FFMPEG,
	AUD_ERROR_SDL
} AUD_Error;

/// Sample pointer type.
typedef unsigned char sample_t;

/// Specification of a sound source or device.
typedef struct
{
	/// Sample rate in Hz.
	AUD_SampleRate rate;

	/// Sample format.
	AUD_SampleFormat format;

	/// Channel count.
	AUD_Channels channels;
} AUD_Specs;

/// Exception structure.
typedef struct
{
	/**
	 * Error code.
	 * \see AUD_Error
	 */
	AUD_Error error;

	// void* userData; - for the case it is needed someday
} AUD_Exception;

/// Handle structure.
typedef struct
{
} AUD_Handle;

#endif //AUD_ENUMS
