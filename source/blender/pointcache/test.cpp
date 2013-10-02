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
#include "PTC_schema.h"

using namespace Alembic::Abc;

namespace PTC {

void test_archive()
{
	std::string archiveName("myFirstArchive.abc");

	// Create an archive with the default writer
	OArchive archive(Alembic::AbcCoreHDF5::WriteArchive(),
	                 archiveName, 
	                 ErrorHandler::kThrowPolicy);

	OObject archiveTop = archive.getTop();

	std::string name = "myFirstChild";
	OObject child( archiveTop, name );

	OCompoundProperty childProps = child.getProperties();

	// Create a TimeSampling object: one sample every 24th of a second
	const chrono_t dt = 1.0 / 24.0;

	TimeSamplingType tst( dt );               // uniform with cycle=dt

	// Create a scalar property on this child object named 'mass'
	ODoubleProperty mass( childProps,  // owner
	                      "mass", // name
	                      tst );

	// Write out the samples
	const unsigned int numSamples = 5;
	const chrono_t startTime = 10.0;
	for (int tt=0; tt<numSamples; tt++)
	{
		double mm = (1.0 + 0.1*tt); // vary the mass
		mass.set( mm,  OSampleSelector(tt, startTime + tt*dt ) );
	}

	// The archive is closed (and written to disk) when 'archive'
	//  goes out of scope.	
}

#if 0
PTC_SCHEMA_INFO("ParticleSystem", "ParticleSystem", ".particles", ParticlesSchemaInfo);

class IParticlesSchema : public ISchema<ParticlesSchemaInfo>
{
public:
	class Sample
	{
	public:
		Sample() {}

//		V3dArraySamplePtr getPositions() const { return m_positions; }

	protected:
		friend class IParticleSchema;
//		V3dArraySamplePtr m_positions;
	};

	// fill the provided Sample ref with the data
	void get(Sample &iSample, const ISampleSelector iSS = ISampleSelector());

	// return a Sample object filled with the data
	Sample getValue(const ISampleSelector &iSS = ISampleSelector());

protected:
	void init(Argument);
//	V3dArrayProperty m_positions;
};

typedef ISchemaObject<IParticlesSchema> IParticles;

class OParticlesSchema : public OSchema<ParticlesSchemaInfo>
{
};
#endif

} /* namespace PTC */
