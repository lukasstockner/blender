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

#ifndef AUD_ISUPERPOSER
#define AUD_ISUPERPOSER

#include "AUD_Space.h"

/**
 * This class is able to superpose two audiosignals.
 */
class AUD_ISuperposer
{
public:
	/**
	 * Destroys the superposer.
	 */
	virtual ~AUD_ISuperposer(){}

	/**
	 * Creates a reader for playback of the sound source.
	 * \param target The target buffer for superposing.
	 * \param source The source buffer for superposing.
	 * \param length The count of bytes to superpose.
	 */
	virtual void superpose(sample_t* destination,
						   sample_t* source,
						   int length)=0;
};

#endif //AUD_ISUPERPOSER
