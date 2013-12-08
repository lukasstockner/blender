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
#include "export.h"

#include "particles.h"
#include "cloth.h"
#include "softbody.h"
#include "rigidbody.h"
#include "smoke.h"
#include "dynamicpaint.h"

extern "C" {
#include "BLI_math.h"

#include "DNA_pointcache_types.h"

#include "RNA_access.h"
}

using namespace PTC;

void PTC_validate(PointCache *cache, int framenr)
{
	if (cache) {
		cache->state.simframe = framenr;
	}
}

void PTC_invalidate(PointCache *cache)
{
	if (cache) {
		cache->state.simframe = 0;
		cache->state.last_exact = min_ii(cache->startframe, 0);
	}
}


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

void PTC_bake(struct Main *bmain, struct Scene *scene, struct PTCWriter *_writer, int start_frame, int end_frame,
              short *stop, short *do_update, float *progress)
{
	PTC::Writer *writer = (PTC::Writer *)_writer;
	PTC::Exporter exporter(bmain, scene, stop, do_update, progress);
	exporter.bake(writer, start_frame, end_frame);
}


void PTC_reader_free(PTCReader *_reader)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	delete reader;
}

PTCReadSampleResult PTC_read_sample(struct PTCReader *_reader, float frame)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	return reader->read_sample(frame);
}

void PTC_reader_get_frame_range(PTCReader *_reader, int *start_frame, int *end_frame)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	int sfra, efra;
	reader->get_frame_range(sfra, efra);
	if (start_frame) *start_frame = sfra;
	if (end_frame) *end_frame = efra;
}

/* get writer/reader from RNA type */
PTCWriter *PTC_writer_from_rna(Scene *scene, PointerRNA *ptr)
{
	if (RNA_struct_is_a(ptr->type, &RNA_ParticleSystem)) {
		Object *ob = (Object *)ptr->id.data;
		ParticleSystem *psys = (ParticleSystem *)ptr->data;
		return PTC_writer_particles(scene, ob, psys);
	}
	return NULL;
}

PTCReader *PTC_reader_from_rna(Scene *scene, PointerRNA *ptr)
{
	if (RNA_struct_is_a(ptr->type, &RNA_ParticleSystem)) {
		Object *ob = (Object *)ptr->id.data;
		ParticleSystem *psys = (ParticleSystem *)ptr->data;
		return PTC_reader_particles(scene, ob, psys);
	}
	return NULL;
}

/* Particles */
PTCWriter *PTC_writer_particles(Scene *scene, Object *ob, ParticleSystem *psys)
{
	return (PTCWriter *)(new PTC::ParticlesWriter(scene, ob, psys));
}

PTCReader *PTC_reader_particles(Scene *scene, Object *ob, ParticleSystem *psys)
{
	return (PTCReader *)(new PTC::ParticlesReader(scene, ob, psys));
}

int PTC_reader_particles_totpoint(PTCReader *_reader)
{
	PTC::ParticlesReader *reader = (PTC::ParticlesReader *)_reader;
	return reader->totpoint();
}

/* Cloth */
PTCWriter *PTC_writer_cloth(Scene *scene, Object *ob, ClothModifierData *clmd)
{
	return (PTCWriter *)(new PTC::ClothWriter(scene, ob, clmd));
}

PTCReader *PTC_reader_cloth(Scene *scene, Object *ob, ClothModifierData *clmd)
{
	return (PTCReader *)(new PTC::ClothReader(scene, ob, clmd));
}

/* SoftBody */
PTCWriter *PTC_writer_softbody(Scene *scene, Object *ob, SoftBody *softbody)
{
	return (PTCWriter *)(new PTC::SoftBodyWriter(scene, ob, softbody));
}

PTCReader *PTC_reader_softbody(Scene *scene, Object *ob, SoftBody *softbody)
{
	return (PTCReader *)(new PTC::SoftBodyReader(scene, ob, softbody));
}

/* Rigid Bodies */
PTCWriter *PTC_writer_rigidbody(Scene *scene, RigidBodyWorld *rbw)
{
	return (PTCWriter *)(new PTC::RigidBodyWriter(scene, rbw));
}

PTCReader *PTC_reader_rigidbody(Scene *scene, RigidBodyWorld *rbw)
{
	return (PTCReader *)(new PTC::RigidBodyReader(scene, rbw));
}

/* Smoke */
PTCWriter *PTC_writer_smoke(Scene *scene, Object *ob, SmokeModifierData *smd)
{
	return (PTCWriter *)(new PTC::SmokeWriter(scene, ob, smd));
}

PTCReader *PTC_reader_smoke(Scene *scene, Object *ob, SmokeModifierData *smd)
{
	return (PTCReader *)(new PTC::SmokeReader(scene, ob, smd));
}

/* Dynamic Paint */
PTCWriter *PTC_writer_dynamicpaint(Scene *scene, Object *ob, DynamicPaintSurface *surface)
{
	return (PTCWriter *)(new PTC::DynamicPaintWriter(scene, ob, surface));
}

PTCReader *PTC_reader_dynamicpaint(Scene *scene, Object *ob, DynamicPaintSurface *surface)
{
	return (PTCReader *)(new PTC::DynamicPaintReader(scene, ob, surface));
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
