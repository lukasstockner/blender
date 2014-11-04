/*
 * Copyright 2014, Blender Foundation.
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

#ifndef PTC_MESH_H
#define PTC_MESH_H

#include <Alembic/AbcGeom/IPolyMesh.h>
#include <Alembic/AbcGeom/OPolyMesh.h>

#include "reader.h"
#include "schema.h"
#include "writer.h"

struct Object;
struct MeshCacheModifierData;
struct PointCacheModifierData;
struct DerivedMesh;

namespace PTC {

class MeshCacheWriter : public Writer {
public:
	MeshCacheWriter(Scene *scene, Object *ob, MeshCacheModifierData *mcmd);
	~MeshCacheWriter();
	
	void write_sample();
	
private:
	Object *m_ob;
	MeshCacheModifierData *m_mcmd;
	
	AbcGeom::OPolyMesh m_mesh;
};

class MeshCacheReader : public Reader {
public:
	MeshCacheReader(Scene *scene, Object *ob, MeshCacheModifierData *mcmd);
	~MeshCacheReader();
	
	DerivedMesh *acquire_result();
	void discard_result();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	Object *m_ob;
	MeshCacheModifierData *m_mcmd;
	
	AbcGeom::IPolyMesh m_mesh;
	
	DerivedMesh *m_result;
};

/* -------------------------------- */

class PointCacheWriter : public Writer {
public:
	PointCacheWriter(Scene *scene, Object *ob, PointCacheModifierData *pcmd);
	~PointCacheWriter();
	
	void write_sample();
	
private:
	Object *m_ob;
	PointCacheModifierData *m_pcmd;
	
	AbcGeom::OPolyMesh m_mesh;
};

class PointCacheReader : public Reader {
public:
	PointCacheReader(Scene *scene, Object *ob, PointCacheModifierData *pcmd);
	~PointCacheReader();
	
	DerivedMesh *acquire_result();
	void discard_result();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	Object *m_ob;
	PointCacheModifierData *m_pcmd;
	
	AbcGeom::IPolyMesh m_mesh;
	
	DerivedMesh *m_result;
};

} /* namespace PTC */

#endif  /* PTC_MESH_H */
