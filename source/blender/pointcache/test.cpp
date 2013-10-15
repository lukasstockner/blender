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
#include "particles.h"
#include "schema.h"

#include <Alembic/AbcCoreHDF5/ReadWrite.h>

//namespace PTC {

void PTC_test_archive(void)
{
#if 0
	std::string archiveName("myFirstArchive.abc");
	OArchive archive(Alembic::AbcCoreHDF5::WriteArchive(),
	                 archiveName,
	                 ErrorHandler::kThrowPolicy);
	
	OObject root = archive.getTop();
//	obj = schema.getObject();
//	ts = schema.getTimeSampling();
//	OCompoundProperty props = root.getProperties();
	OParticles particles(root, "particles");
//	OFloatProperty size(props, "size");
	OParticlesSchema schema = particles.getSchema();

//	for (int i = 0; i < 10; ++i) {
	OParticlesSchema::Sample sample;
		
//		size.set(i*i - 0.43*i);
		schema.set(sample);
//	}
#endif

#if 0
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

	TimeSampling tst(dt, 0.0f);               // uniform with cycle=dt
	
	// Create a scalar property on this child object named 'mass'
	ODoubleProperty mass( childProps,  // owner
	                      "mass", // name
	                      TimeSamplingPtr(&tst) );

	// Write out the samples
	const unsigned int numSamples = 5;
	const chrono_t startTime = 10.0;
	for (int tt=0; tt<numSamples; tt++)
	{
		double mm = (1.0 + 0.1*tt); // vary the mass
		mass.set(mm);
	}

	// The archive is closed (and written to disk) when 'archive'
	//  goes out of scope.
#endif
}

//} /* namespace PTC */
