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

#ifndef PTC_ALEMBIC_H
#define PTC_ALEMBIC_H

#include "reader.h"
#include "writer.h"

struct Object;
struct ClothModifierData;
struct DynamicPaintSurface;
struct PointCacheModifierData;
struct DerivedMesh;
struct ParticleSystem;
struct RigidBodyWorld;
struct SmokeDomainSettings;
struct SoftBody;

namespace PTC {

/* Particles */
Writer *abc_writer_particles(Scene *scene, Object *ob, ParticleSystem *psys);
Reader *abc_reader_particles(Scene *scene, Object *ob, ParticleSystem *psys);

/* Cloth */
Writer *abc_writer_cloth(Scene *scene, Object *ob, ClothModifierData *clmd);
Reader *abc_reader_cloth(Scene *scene, Object *ob, ClothModifierData *clmd);

/* SoftBody */
Writer *abc_writer_softbody(Scene *scene, Object *ob, SoftBody *softbody);
Reader *abc_reader_softbody(Scene *scene, Object *ob, SoftBody *softbody);

/* Rigid Bodies */
Writer *abc_writer_rigidbody(Scene *scene, RigidBodyWorld *rbw);
Reader *abc_reader_rigidbody(Scene *scene, RigidBodyWorld *rbw);

/* Smoke */
Writer *abc_writer_smoke(Scene *scene, Object *ob, SmokeDomainSettings *domain);
Reader *abc_reader_smoke(Scene *scene, Object *ob, SmokeDomainSettings *domain);

/* Dynamic Paint */
Writer *abc_writer_dynamicpaint(Scene *scene, Object *ob, DynamicPaintSurface *surface);
Reader *abc_reader_dynamicpaint(Scene *scene, Object *ob, DynamicPaintSurface *surface);

/* Modifier Stack */
Writer *abc_writer_point_cache(Scene *scene, Object *ob, PointCacheModifierData *pcmd);
Reader *abc_reader_point_cache(Scene *scene, Object *ob, PointCacheModifierData *pcmd);

#if 0
class ClothWriter : public Writer {
public:
	ClothWriter(Scene *scene, Object *ob, ClothModifierData *clmd);
	~ClothWriter();
	
private:
	Object *m_ob;
	ClothModifierData *m_clmd;
};

class ClothReader : public Reader {
public:
	ClothReader(Scene *scene, Object *ob, ClothModifierData *clmd);
	~ClothReader();
	
private:
	Object *m_ob;
	ClothModifierData *m_clmd;
};


class DynamicPaintWriter : public Writer {
public:
	DynamicPaintWriter(Scene *scene, Object *ob, DynamicPaintSurface *surface);
	~DynamicPaintWriter();
	
private:
	Object *m_ob;
	DynamicPaintSurface *m_surface;
};

class DynamicPaintReader : public Reader {
public:
	DynamicPaintReader(Scene *scene, Object *ob, DynamicPaintSurface *surface);
	~DynamicPaintReader();
	
private:
	Object *m_ob;
	DynamicPaintSurface *m_surface;
};


class PointCacheWriter : public Writer {
public:
	PointCacheWriter(Scene *scene, Object *ob, PointCacheModifierData *pcmd);
	~PointCacheWriter();
	
private:
	Object *m_ob;
	PointCacheModifierData *m_pcmd;
};

class PointCacheReader : public Reader {
public:
	PointCacheReader(Scene *scene, Object *ob, PointCacheModifierData *pcmd);
	~PointCacheReader();
	
	DerivedMesh *acquire_result();
	void discard_result();
	
private:
	Object *m_ob;
	PointCacheModifierData *m_pcmd;
	
	DerivedMesh *m_result;
};


class ParticlesWriter : public Writer {
public:
	ParticlesWriter(Scene *scene, Object *ob, ParticleSystem *psys);
	~ParticlesWriter();
	
private:
	Object *m_ob;
	ParticleSystem *m_psys;
};

class ParticlesReader : public Reader {
public:
	ParticlesReader(Scene *scene, Object *ob, ParticleSystem *psys);
	~ParticlesReader();
	
	int totpoint() const { return m_totpoint; }
	
private:
	Object *m_ob;
	ParticleSystem *m_psys;
	
	int m_totpoint;
};


class RigidBodyWriter : public Writer {
public:
	RigidBodyWriter(Scene *scene, RigidBodyWorld *rbw);
	~RigidBodyWriter();
	
private:
	RigidBodyWorld *m_rbw;
};

class RigidBodyReader : public Reader {
public:
	RigidBodyReader(Scene *scene, RigidBodyWorld *rbw);
	~RigidBodyReader();
	
private:
	RigidBodyWorld *m_rbw;
};


class SmokeWriter : public Writer {
public:
	SmokeWriter(Scene *scene, Object *ob, SmokeDomainSettings *domain);
	~SmokeWriter();
	
private:
	Object *m_ob;
	SmokeDomainSettings *m_domain;
};

class SmokeReader : public Reader {
public:
	SmokeReader(Scene *scene, Object *ob, SmokeDomainSettings *domain);
	~SmokeReader();
	
private:
	Object *m_ob;
	SmokeDomainSettings *m_domain;
};


class SoftBodyWriter : public Writer {
public:
	SoftBodyWriter(Scene *scene, Object *ob, SoftBody *softbody);
	~SoftBodyWriter();
	
private:
	Object *m_ob;
	SoftBody *m_softbody;
};

class SoftBodyReader : public Reader {
public:
	SoftBodyReader(Scene *scene, Object *ob, SoftBody *softbody);
	~SoftBodyReader();
	
private:
	Object *m_ob;
	SoftBody *m_softbody;
};
#endif

} /* namespace PTC */

#endif  /* PTC_EXPORT_H */
