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

void PTC_bake(Main *bmain, Scene *scene, struct PTCWriter *writer, int start_frame, int end_frame)
{
	int /*use_timer = FALSE, */sfra, efra;
//	double stime, ptime, ctime, fetd;
//	char run[32], cur[32], etd[32];

//	ptcache_bake_data *data = (ptcache_bake_data*)ptr;

//	stime = ptime = PIL_check_seconds_timer();
	sfra = start_frame;
	efra = end_frame;

	for (; (*data->cfra_ptr <= data->endframe) && !data->break_operation; *data->cfra_ptr+=data->step) {
		BKE_scene_update_for_newframe(data->main, data->scene, data->scene->lay);
#if 0
		if (G.background) {
			printf("bake: frame %d :: %d\n", (int)*data->cfra_ptr, data->endframe);
		}
		else {
			ctime = PIL_check_seconds_timer();

			fetd = (ctime-ptime)*(efra-*data->cfra_ptr)/data->step;

			if (use_timer || fetd > 60.0) {
				use_timer = TRUE;

				ptcache_dt_to_str(cur, ctime-ptime);
				ptcache_dt_to_str(run, ctime-stime);
				ptcache_dt_to_str(etd, fetd);

				printf("Baked for %s, current frame: %i/%i (%.3fs), ETC: %s\r", run, *data->cfra_ptr-sfra+1, efra-sfra+1, ctime-ptime, etd);
			}
			ptime = ctime;
		}
#endif
	}

#if 0
	if (use_timer) {
		/* start with newline because of \r above */
		ptcache_dt_to_str(run, PIL_check_seconds_timer()-stime);
		printf("\nBake %s %s (%i frames simulated).\n", (data->break_operation ? "canceled after" : "finished in"), run, *data->cfra_ptr-sfra);
	}
#endif

//	data->thread_ended = TRUE;
//	return NULL;
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
