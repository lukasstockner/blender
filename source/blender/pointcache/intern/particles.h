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

#ifndef PTC_PARTICLES_H
#define PTC_PARTICLES_H

#include <Alembic/AbcGeom/IPoints.h>
#include <Alembic/AbcGeom/OPoints.h>

#include "schema.h"
#include "types.h"
#include "writer.h"

struct Object;
struct ParticleSystem;

namespace PTC {

#if 0
PTC_SCHEMA_INFO("Particles", "Particles", ".particles", ParticlesSchemaInfo);

class IParticlesSchema : public IGeomBaseSchema<ParticlesSchemaInfo>
{
public:
	class Sample
	{
	public:
		typedef Sample this_type;

		// Users don't ever create this data directly.
		Sample() { reset(); }

		P3fArraySamplePtr getPositions() const { return m_positions; }
		UInt64ArraySamplePtr getIds() const { return m_ids; }
		V3fArraySamplePtr getVelocities() const { return m_velocities; }

		Box3d getSelfBounds() const { return m_selfBounds; }

		bool valid() const
		{
			return m_positions && m_ids;
		}

		void reset()
		{
			m_positions.reset();
			m_velocities.reset();
			m_ids.reset();
			m_selfBounds.makeEmpty();
		}

		ALEMBIC_OPERATOR_BOOL( valid() );

	protected:
		friend class IParticlesSchema;
		P3fArraySamplePtr m_positions;
		UInt64ArraySamplePtr m_ids;
		V3fArraySamplePtr m_velocities;

		Box3d m_selfBounds;
	};

	//-*************************************************************************
	// POINTS SCHEMA
	//-*************************************************************************
public:
	//! By convention we always define this_type in AbcGeom classes.
	//! Used by unspecified-bool-type conversion below
	typedef IParticlesSchema this_type;

	//-*************************************************************************
	// CONSTRUCTION, DESTRUCTION, ASSIGNMENT
	//-*************************************************************************

	//! The default constructor creates an empty IParticlesSchema
	//! ...
	IParticlesSchema() {}

	//! This templated, explicit function creates a new scalar property reader.
	//! The first argument is any Abc (or AbcCoreAbstract) object
	//! which can intrusively be converted to an CompoundPropertyReaderPtr
	//! to use as a parent, from which the error handler policy for
	//! inheritance is also derived.  The remaining optional arguments
	//! can be used to override the ErrorHandlerPolicy, to specify
	//! MetaData, and to set TimeSamplingType.
	template <class CPROP_PTR>
	IParticlesSchema(CPROP_PTR iParent,
	                 const std::string &iName,

	                 const Argument &iArg0 = Argument(),
	                 const Argument &iArg1 = Argument())
	  : IGeomBaseSchema<ParticlesSchemaInfo>(iParent, iName,
	                                         iArg0, iArg1)
	{
		init(iArg0, iArg1);
	}

	//! This constructor is the same as above, but with default
	//! schema name used.
	template <class CPROP_PTR>
	explicit IParticlesSchema(CPROP_PTR iParent,
	                          const Argument &iArg0 = Argument(),
	                          const Argument &iArg1 = Argument())
	    : IGeomBaseSchema<ParticlesSchemaInfo>(iParent,
	                                           iArg0, iArg1)
	{
		init(iArg0, iArg1);
	}

	//! Copy constructor.
	IParticlesSchema(const IParticlesSchema& iCopy)
	    : IGeomBaseSchema<ParticlesSchemaInfo>()
	{
		*this = iCopy;
	}

	//! Default assignment operator used.

	//-*************************************************************************
	// SCALAR PROPERTY READER FUNCTIONALITY
	//-*************************************************************************

	//! Return the number of samples contained in the property.
	//! This can be any number, including zero.
	//! This returns the number of samples that were written, independently
	//! of whether or not they were constant.
	size_t getNumSamples() const
	{
		return std::max(m_positionsProperty.getNumSamples(),
		                m_idsProperty.getNumSamples());
	}

	//! Ask if we're constant - no change in value amongst samples,
	//! regardless of the time sampling.
	bool isConstant() const { return m_positionsProperty.isConstant() && m_idsProperty.isConstant(); }

	//! Time sampling Information.
	//!
	TimeSamplingPtr getTimeSampling() const
	{
		if (m_positionsProperty.valid())
		{
			return m_positionsProperty.getTimeSampling();
		}
		return getObject().getArchive().getTimeSampling(0);
	}

	//-*************************************************************************
	void get(Sample &oSample,
	         const ISampleSelector &iSS = ISampleSelector()) const
	{
		ALEMBIC_ABC_SAFE_CALL_BEGIN("IParticlesSchema::get()");

		m_positionsProperty.get(oSample.m_positions, iSS);
		m_idsProperty.get(oSample.m_ids, iSS);

		m_selfBoundsProperty.get(oSample.m_selfBounds, iSS);

		if (m_velocitiesProperty && m_velocitiesProperty.getNumSamples() > 0) {
			m_velocitiesProperty.get( oSample.m_velocities, iSS );
		}

		// Could error check here.

		ALEMBIC_ABC_SAFE_CALL_END();
	}

	Sample getValue(const ISampleSelector &iSS = ISampleSelector()) const
	{
		Sample smp;
		get(smp, iSS);
		return smp;
	}

	IP3fArrayProperty getPositionsProperty() const { return m_positionsProperty; }

	IV3fArrayProperty getVelocitiesProperty() const { return m_velocitiesProperty; }

	IUInt64ArrayProperty getIdsProperty() const { return m_idsProperty; }

	IFloatGeomParam getWidthsParam() const { return m_widthsParam; }

	//-*************************************************************************
	// ABC BASE MECHANISMS
	// These functions are used by Abc to deal with errors, rewrapping,
	// and so on.
	//-*************************************************************************

	//! Reset returns this function set to an empty, default
	//! state.
	void reset()
	{
		m_positionsProperty.reset();
		m_velocitiesProperty.reset();
		m_idsProperty.reset();
		m_widthsParam.reset();

		IGeomBaseSchema<ParticlesSchemaInfo>::reset();
	}

	//! Valid returns whether this function set is
	//! valid.
	bool valid() const
	{
		return (IGeomBaseSchema<ParticlesSchemaInfo>() &&
		        m_positionsProperty.valid() &&
		        m_idsProperty.valid());
	}

	//! unspecified-bool-type operator overload.
	//! ...
	ALEMBIC_OVERRIDE_OPERATOR_BOOL(IParticlesSchema::valid());

protected:
	void init(const Argument &iArg0,
	          const Argument &iArg1);

	IP3fArrayProperty m_positionsProperty;
	IUInt64ArrayProperty m_idsProperty;
	IV3fArrayProperty m_velocitiesProperty;
	IFloatGeomParam m_widthsParam;
};

//-*****************************************************************************
typedef ISchemaObject<IParticlesSchema> IParticles;

typedef Util::shared_ptr< IParticles > IParticlesPtr;



class OParticlesSchema : public OGeomBaseSchema<ParticlesSchemaInfo>
{
public:
	//-*************************************************************************
	// POINTS SCHEMA SAMPLE TYPE
	//-*************************************************************************
	class Sample
	{
	public:
		//! Creates a default sample with no data in it.
		//! ...
		Sample() { reset(); }

		//! Creates a sample with position data but no id
		//! data. For specifying samples after the first one
		Sample(const P3fArraySample &iPos,
		       const V3fArraySample &iVelocities = V3fArraySample(),
		       const OFloatGeomParam::Sample &iWidths = OFloatGeomParam::Sample())
		    : m_positions( iPos )
		    , m_velocities( iVelocities )
		    , m_widths( iWidths )
		{}

		//! Creates a sample with position data and id data. The first
		//! sample must be full like this. Subsequent samples may also
		//! be full like this, which would indicate a change of topology
		Sample(const P3fArraySample &iPos,
		       const UInt64ArraySample &iId,
		       const V3fArraySample &iVelocities = V3fArraySample(),
		       const OFloatGeomParam::Sample &iWidths =  OFloatGeomParam::Sample())
		    : m_positions(iPos)
		    , m_velocities(iVelocities)
		    , m_ids(iId)
		    , m_widths(iWidths)
		{}

		// positions accessor
		const P3fArraySample &getPositions() const { return m_positions; }
		void setPositions(const P3fArraySample &iSmp) { m_positions = iSmp; }

		// ids accessor
		const UInt64ArraySample &getIds() const { return m_ids; }
		void setIds(const UInt64ArraySample &iSmp) { m_ids = iSmp; }

		// velocities accessor
		const V3fArraySample &getVelocities() const { return m_velocities; }
		void setVelocities(const V3fArraySample &iVelocities) { m_velocities = iVelocities; }

		// widths accessor
		const OFloatGeomParam::Sample &getWidths() const { return m_widths; }
		void setWidths(const OFloatGeomParam::Sample &iWidths) { m_widths = iWidths; }

		const Box3d &getSelfBounds() const { return m_selfBounds; }
		void setSelfBounds(const Box3d &iBnds) { m_selfBounds = iBnds; }

		void reset()
		{
			m_positions.reset();
			m_velocities.reset();
			m_ids.reset();
			m_widths.reset();

			m_selfBounds.makeEmpty();
		}

	protected:
		P3fArraySample m_positions;
		V3fArraySample m_velocities;
		UInt64ArraySample m_ids;
		OFloatGeomParam::Sample m_widths;

		Box3d m_selfBounds;
	};

	//-*************************************************************************
	// POINTS SCHEMA
	//-*************************************************************************
public:
	//! By convention we always define this_type in AbcGeom classes.
	//! Used by unspecified-bool-type conversion below
	typedef OParticlesSchema this_type;

	//-*************************************************************************
	// CONSTRUCTION, DESTRUCTION, ASSIGNMENT
	//-*************************************************************************

	//! The default constructor creates an empty OParticlesSchema
	//! ...
	OParticlesSchema() {}

	//! This templated, primary constructor creates a new poly mesh writer.
	//! The first argument is any Abc (or AbcCoreAbstract) object
	//! which can intrusively be converted to an CompoundPropertyWriterPtr
	//! to use as a parent, from which the error handler policy for
	//! inheritance is also derived.  The remaining optional arguments
	//! can be used to override the ErrorHandlerPolicy, to specify
	//! MetaData, and to set TimeSamplingType.
	template <class CPROP_PTR>
	OParticlesSchema(CPROP_PTR iParent,
	                 const std::string &iName,

	                 const Argument &iArg0 = Argument(),
	                 const Argument &iArg1 = Argument(),
	                 const Argument &iArg2 = Argument())
	    : OGeomBaseSchema<ParticlesSchemaInfo>(
	          GetCompoundPropertyWriterPtr(iParent),
	          iName, iArg0, iArg1, iArg2)
	{
		TimeSamplingPtr tsPtr = GetTimeSampling( iArg0, iArg1, iArg2 );
		uint32_t tsIndex = GetTimeSamplingIndex( iArg0, iArg1, iArg2 );

		// if we specified a valid TimeSamplingPtr, use it to determine the
		// index otherwise we'll use the index, which defaults to the intrinsic
		// 0 index
		if (tsPtr)
		{
			tsIndex = GetCompoundPropertyWriterPtr( iParent )->getObject()->getArchive()->addTimeSampling(*tsPtr);
		}

		// Meta data and error handling are eaten up by
		// the super type, so all that's left is time sampling.
		init(tsIndex);
	}

	template <class CPROP_PTR>
	explicit OParticlesSchema(CPROP_PTR iParent,
	                          const Argument &iArg0 = Argument(),
	                          const Argument &iArg1 = Argument(),
	                          const Argument &iArg2 = Argument())
	    : OGeomBaseSchema<ParticlesSchemaInfo>(
	          GetCompoundPropertyWriterPtr(iParent),
	          iArg0, iArg1, iArg2)
	{
		TimeSamplingPtr tsPtr = GetTimeSampling( iArg0, iArg1, iArg2 );
		uint32_t tsIndex = GetTimeSamplingIndex( iArg0, iArg1, iArg2 );

		// if we specified a valid TimeSamplingPtr, use it to determine the
		// index otherwise we'll use the index, which defaults to the intrinsic
		// 0 index
		if (tsPtr) {
			tsIndex = GetCompoundPropertyWriterPtr( iParent )->getObject()->getArchive()->addTimeSampling(*tsPtr);
		}

		// Meta data and error handling are eaten up by
		// the super type, so all that's left is time sampling.
		init( tsIndex );
	}

	//! Copy constructor.
	OParticlesSchema(const OParticlesSchema& iCopy)
	    : OGeomBaseSchema<ParticlesSchemaInfo>()
	{
		*this = iCopy;
	}

	//! Default assignment operator used.

	//-*************************************************************************
	// SCHEMA STUFF
	//-*************************************************************************

	//! Return the time sampling
	TimeSamplingPtr getTimeSampling() const { return m_positionsProperty.getTimeSampling(); }

	//-*************************************************************************
	// SAMPLE STUFF
	//-*************************************************************************

	//! Get number of samples written so far.
	//! ...
	size_t getNumSamples() const { return m_positionsProperty.getNumSamples(); }

	//! Set a sample
	void set(const Sample &iSamp);

	//! Set from previous sample. Will apply to each of positions,
	//! ids, velocities, and widths
	void setFromPrevious();

	void setTimeSampling(uint32_t iIndex);
	void setTimeSampling(TimeSamplingPtr iTime);

	//-*************************************************************************
	// ABC BASE MECHANISMS
	// These functions are used by Abc to deal with errors, validity,
	// and so on.
	//-*************************************************************************

	//! Reset returns this function set to an empty, default
	//! state.
	void reset()
	{
		m_positionsProperty.reset();
		m_idsProperty.reset();
		m_velocitiesProperty.reset();
		m_widthsParam.reset();

		OGeomBaseSchema<ParticlesSchemaInfo>::reset();
	}

	//! Valid returns whether this function set is
	//! valid.
	bool valid() const
	{
		return (OGeomBaseSchema<ParticlesSchemaInfo>::valid() &&
		        m_positionsProperty.valid() &&
		        m_idsProperty.valid());
	}

	//! unspecified-bool-type operator overload.
	//! ...
	ALEMBIC_OVERRIDE_OPERATOR_BOOL( OParticlesSchema::valid() );

protected:
	void init(uint32_t iTsIdx);

	OP3fArrayProperty m_positionsProperty;
	OUInt64ArrayProperty m_idsProperty;
	OV3fArrayProperty m_velocitiesProperty;
	OFloatGeomParam m_widthsParam;

};

//-*****************************************************************************
// SCHEMA OBJECT
//-*****************************************************************************
typedef OSchemaObject<OParticlesSchema> OParticles;

typedef Util::shared_ptr< OParticles > OParticlesPtr;
#endif


class ParticlesWriter : public Writer {
public:
	ParticlesWriter(const std::string &filename, Object *ob, ParticleSystem *psys);
	~ParticlesWriter();
	
	void write();
	
private:
	Object *m_ob;
	ParticleSystem *m_psys;
	
	AbcGeom::OPoints m_points;
};

} /* namespace PTC */

#endif  /* PTC_PARTICLES_H */
