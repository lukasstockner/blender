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

#include "AUD_FFMPEGFactory.h"
#include "AUD_FFMPEGReader.h"
#include "AUD_Space.h"

AUD_FFMPEGFactory::AUD_FFMPEGFactory(const char* filename)
{
	m_filename = filename;
}

AUD_IReader* AUD_FFMPEGFactory::createReader()
{
	try
	{
		AUD_IReader* reader = new AUD_FFMPEGReader(m_filename);
		AUD_NEW("reader")
		return reader;
	}
	catch(AUD_Exception e)
	{
		// return 0 if ffmpeg cannot read the file
		if(e.error == AUD_ERROR_FFMPEG)
			return 0;
		// but throw an exception if the file doesn't exist
		else
			throw;
	}
}
