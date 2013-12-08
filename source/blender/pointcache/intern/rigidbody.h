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

#ifndef PTC_RIGIDBODY_H
#define PTC_RIGIDBODY_H

//#include <Alembic/AbcGeom/IPoints.h>
//#include <Alembic/AbcGeom/OPoints.h>

#include "reader.h"
#include "schema.h"
#include "writer.h"

struct RigidBodyWorld;

namespace PTC {

class RigidBodyWriter : public Writer {
public:
	RigidBodyWriter(Scene *scene, RigidBodyWorld *rbw);
	~RigidBodyWriter();
	
	void write_sample();
	
private:
	RigidBodyWorld *m_rbw;
	
//	AbcGeom::OPoints m_points;
};

class RigidBodyReader : public Reader {
public:
	RigidBodyReader(Scene *scene, RigidBodyWorld *rbw);
	~RigidBodyReader();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	RigidBodyWorld *m_rbw;
	
//	AbcGeom::IPoints m_points;
};

} /* namespace PTC */

#endif  /* PTC_RIGIDBODY_H */
