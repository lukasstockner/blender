/*
 * Copyright 2013, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "PTC_api.h"

#ifdef WITH_ALEMBIC

#include "archive.h"
#include "particles.h"

#include "DNA_object_types.h"
#include "DNA_particle_types.h"

typedef struct PTCArchive PTCArchive;
typedef struct PTCObject PTCObject;

PTCArchive *PTC_archive_create(const char *filename)
{
	OArchive *archive = new OArchive(Alembic::AbcCoreHDF5::WriteArchive(),
	                                 std::string(filename),
	                                 ErrorHandler::kThrowPolicy);
	
	return (PTCArchive *)archive;
}

void PTC_archive_free(PTCArchive *_archive)
{
	OArchive *archive = (OArchive *)_archive;
	delete archive;
}


void PTC_write_particles(PTCArchive *_archive, Object *ob, ParticleSystem *psys)
{
	OArchive *archive = (OArchive *)_archive;
	
	OObject root = archive->getTop();
	
//	OParticles particles(root.getChild(psys->name).getPtr(), kWrapExisting);
//	if (!particles.getPtr())
//		particles = OParticles(root, psys->name);
}

#else

PTCArchive *PTC_archive_create(const char *filename)
{
	return NULL;
}

void PTC_archive_free(PTCArchive *_archive)
{
}

#endif
