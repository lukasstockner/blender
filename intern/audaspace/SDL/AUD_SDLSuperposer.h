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

#ifndef AUD_SDLSUPERPOSER
#define AUD_SDLSUPERPOSER

#include "AUD_ISuperposer.h"

/**
 * This class is able to superpose two audiosignals with the help of SDL.
 * The specification of the audio signals has to be the same as the
 * specification of the device.
 */
class AUD_SDLSuperposer : public AUD_ISuperposer
{
public:
	/**
	 * Creates the superposer.
	 */
	AUD_SDLSuperposer();

	virtual void superpose(sample_t* destination, sample_t* source, int length);
};

#endif //AUD_SDLSUPERPOSER
