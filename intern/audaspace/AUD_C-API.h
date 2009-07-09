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

#include "AUD_Space.h"

typedef struct {} AUD_Sound;
typedef struct {} AUD_Device;

extern AUD_Device* AUD_init();

extern void AUD_exit(AUD_Device* device);

extern AUD_Sound* openSound(const char* filename);

extern void closeSound(AUD_Sound* sound);

extern void playSound(AUD_Device* device, AUD_Sound* sound);

#ifdef __cplusplus
}
#endif

#endif //AUD_CAPI
