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

#ifndef PTC_SCHEMA_H
#define PTC_SCHEMA_H

//namespace PTC {

#ifdef WITH_ALEMBIC

#include <Alembic/AbcGeom/Foundation.h>
#include <Alembic/AbcGeom/SchemaInfoDeclarations.h>
#include <Alembic/AbcGeom/IGeomParam.h>
#include <Alembic/AbcGeom/IGeomBase.h>
#include <Alembic/AbcGeom/OGeomBase.h>
#include <Alembic/AbcGeom/OGeomParam.h>

//using namespace Alembic::AbcGeom::ALEMBIC_VERSION_NS;
using namespace Alembic::AbcGeom;
namespace Util = Alembic::Util;

#define PTC_SCHEMA_INFO ALEMBIC_ABCGEOM_DECLARE_SCHEMA_INFO

//template <class INFO>
//class ISchema : public Abc::ISchema<INFO>
//{
//};


//template <class SCHEMA>
//class ISchemaObject : public Abc::ISchemaObject<SCHEMA>
//{
//};


//typedef Abc::ISampleSelector ISampleSelector;


//template <class INFO>
//class OSchema : public Abc::OSchema<INFO>
//{
//};

//typedef Abc::SchemaObject<GeometricThingySchema> GeometricThingy;

#else

#define PTC_SCHEMA_INFO(STITLE, SBTYP, SDFLT, STDEF)

template <class INFO>
class ISchema
{
};


template <class SCHEMA>
class ISchemaObject
{
};


template <class INFO>
class OSchema
{
};

#endif

//} /* namespace PTC */

#endif  /* PTC_SCHEMA_H */
