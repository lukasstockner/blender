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

#ifndef PTC_API_H
#define PTC_API_H

#include "util/util_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Scene;
struct PointCache;
struct PointerRNA;

struct ClothModifierData;
struct DynamicPaintSurface;
struct Object;
struct ParticleSystem;
struct RigidBodyWorld;
struct SmokeModifierData;
struct SoftBody;

struct PTCWriter;
struct PTCReader;

void PTC_validate(struct PointCache *cache, int framenr);
void PTC_invalidate(struct PointCache *cache);

void PTC_bake(struct Main *bmain, struct Scene *scene, struct PTCWriter *writer, int start_frame, int end_frame,
              short *stop, short *do_update, float *progress);


/*** Reader/Writer Interface ***/

void PTC_writer_free(struct PTCWriter *writer);
void PTC_write_sample(struct PTCWriter *writer);

void PTC_reader_free(struct PTCReader *reader);
PTCReadSampleResult PTC_read_sample(struct PTCReader *reader, float frame);
void PTC_reader_get_frame_range(struct PTCReader *reader, int *start_frame, int *end_frame);

/* get writer/reader from RNA type */
struct PTCWriter *PTC_writer_from_rna(struct Scene *scene, struct PointerRNA *ptr);
struct PTCReader *PTC_reader_from_rna(struct Scene *scene, struct PointerRNA *ptr);

/* Particles */
struct PTCWriter *PTC_writer_particles(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particles(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys);
int PTC_reader_particles_totpoint(struct PTCReader *reader);

/* Cloth */
struct PTCWriter *PTC_writer_cloth(struct Scene *scene, struct Object *ob, struct ClothModifierData *clmd);
struct PTCReader *PTC_reader_cloth(struct Scene *scene, struct Object *ob, struct ClothModifierData *clmd);

/* SoftBody */
struct PTCWriter *PTC_writer_softbody(struct Scene *scene, struct Object *ob, struct SoftBody *softbody);
struct PTCReader *PTC_reader_softbody(struct Scene *scene, struct Object *ob, struct SoftBody *softbody);

/* Rigid Bodies */
struct PTCWriter *PTC_writer_rigidbody(struct Scene *scene, struct RigidBodyWorld *rbw);
struct PTCReader *PTC_reader_rigidbody(struct Scene *scene, struct RigidBodyWorld *rbw);

/* Smoke */
struct PTCWriter *PTC_writer_smoke(struct Scene *scene, struct Object *ob, struct SmokeModifierData *smd);
struct PTCReader *PTC_reader_smoke(struct Scene *scene, struct Object *ob, struct SmokeModifierData *smd);

/* Dynamic Paint */
struct PTCWriter *PTC_writer_dynamicpaint(struct Scene *scene, struct Object *ob, struct DynamicPaintSurface *surface);
struct PTCReader *PTC_reader_dynamicpaint(struct Scene *scene, struct Object *ob, struct DynamicPaintSurface *surface);

#ifdef __cplusplus
} /* extern C */
#endif

#endif  /* PTC_API_H */
