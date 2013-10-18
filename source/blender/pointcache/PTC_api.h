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

#ifdef __cplusplus
extern "C" {
#endif

struct Scene;
struct Object;
struct ParticleSystem;

void PTC_test_archive(void);


struct PTCWriter;
struct PTCReader;

void PTC_writer_free(struct PTCWriter *writer);
void PTC_write_sample(struct PTCWriter *writer);
void PTC_bake(struct PTCWriter *writer, int start_frame, int end_frame);

void PTC_reader_free(struct PTCReader *reader);
void PTC_read_sample(struct PTCReader *reader);


/* Particles */
struct PTCWriter *PTC_writer_particles(struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particles(struct Object *ob, struct ParticleSystem *psys);

#ifdef __cplusplus
} /* extern C */
#endif

#endif  /* PTC_API_H */
