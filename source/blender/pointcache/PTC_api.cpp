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

#include "reader.h"
#include "writer.h"
#include "particles.h"

#include "util_path.h"

extern "C" {
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
}

using namespace PTC;

void PTC_writer_free(PTCWriter *_writer)
{
	PTC::Writer *writer = (PTC::Writer *)_writer;
	delete writer;
}

void PTC_write_sample(struct PTCWriter *_writer)
{
	PTC::Writer *writer = (PTC::Writer *)_writer;
	writer->write_sample();
}

void PTC_bake(struct PTCWriter *writer, int start_frame, int end_frame)
{
	
}


void PTC_reader_free(PTCReader *_reader)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	delete reader;
}

void PTC_read_sample(struct PTCReader *_reader)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	reader->read_sample();
}


/* Particles */
PTCWriter *PTC_writer_particles(Object *ob, ParticleSystem *psys)
{
	PointCache *cache = psys->pointcache;
	if (!cache)
		return NULL;
	std::string filename = ptc_archive_path(cache->name, cache->index, cache->path, &ob->id,
	                                        true, true,
	                                        cache->flag & PTCACHE_EXTERNAL,
	                                        cache->flag & PTCACHE_IGNORE_LIBPATH);
	
	PTC::ParticlesWriter *writer = new PTC::ParticlesWriter(filename, ob, psys);
	return (PTCWriter *)writer;
}

PTCReader *PTC_reader_particles(Object *ob, ParticleSystem *psys)
{
	PointCache *cache = psys->pointcache;
	if (!cache)
		return NULL;
	std::string filename = ptc_archive_path(cache->name, cache->index, cache->path, &ob->id,
	                                        true, true,
	                                        cache->flag & PTCACHE_EXTERNAL,
	                                        cache->flag & PTCACHE_IGNORE_LIBPATH);

	PTC::ParticlesReader *reader = new PTC::ParticlesReader(filename, ob, psys);
	return (PTCReader *)reader;
}

#else

void PTC_writer_free(PTCWriter *_writer)
{
}

void PTC_write(struct PTCWriter *_writer)
{
}

PTCWriter *PTC_writer_create_particles(const char *filename, struct Object *ob, struct ParticleSystem *psys)
{
	return NULL;
}

#endif
