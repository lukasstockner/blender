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

#ifndef AUD_FFMPEGFACTORY
#define AUD_FFMPEGFACTORY

#include "AUD_IFactory.h"

/**
 * This factory reads a sound file via ffmpeg.
 * \warning Notice that the needed formats and codecs have to be registered
 * for ffmpeg before this class can be used.
 */
class AUD_FFMPEGFactory : public AUD_IFactory
{
private:
	/**
	 * The filename of the sound source file.
	 */
	const char* m_filename;

public:
	/**
	 * Creates a new factory.
	 * \param filename The sound file path.
	 */
	AUD_FFMPEGFactory(const char* filename);

	virtual AUD_IReader* createReader();
};

#endif //AUD_FFMPEGFACTORY
