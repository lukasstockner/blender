# $Id$
# ***** BEGIN LGPL LICENSE BLOCK *****
#
# Copyright 2009 Jörg Hermann Müller
#
# This file is part of AudaSpace.
#
# AudaSpace is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# AudaSpace is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
#
# ***** END LGPL LICENSE BLOCK *****

SET(INC . intern SDL ffmpeg)

IF(WITH_FFMPEG)
  FILE(GLOB SRC ffmpeg/*.cpp SDL/*.cpp intern/*.cpp intern/*.h)
  INCLUDE_DIRECTORIES(${SDL_INC} ${FFMPEG_INC} ${PTHREADS_INC})
ELSE(WITH_FFMPEG)
  FILE(GLOB SRC SDL/*.cpp intern/*.cpp intern/*.h)
  INCLUDE_DIRECTORIES(${SDL_INC} ${PTHREADS_INC})
ENDIF(WITH_FFMPEG)

BLENDERLIB(bf_audaspace "${SRC}" "${INC}")
