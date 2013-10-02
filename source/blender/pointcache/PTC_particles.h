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

//namespace PTC {

#include "PTC_schema.h"

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

		Abc::P3fArraySamplePtr getPositions() const { return m_positions; }
		Abc::UInt64ArraySamplePtr getIds() const { return m_ids; }
		Abc::V3fArraySamplePtr getVelocities() const { return m_velocities; }

		Abc::Box3d getSelfBounds() const { return m_selfBounds; }

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
		Abc::P3fArraySamplePtr m_positions;
		Abc::UInt64ArraySamplePtr m_ids;
		Abc::V3fArraySamplePtr m_velocities;

		Abc::Box3d m_selfBounds;
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

	                 const Abc::Argument &iArg0 = Abc::Argument(),
	                 const Abc::Argument &iArg1 = Abc::Argument())
	  : IGeomBaseSchema<ParticlesSchemaInfo>(iParent, iName,
	                                         iArg0, iArg1)
	{
		init(iArg0, iArg1);
	}

	//! This constructor is the same as above, but with default
	//! schema name used.
	template <class CPROP_PTR>
	explicit IParticlesSchema(CPROP_PTR iParent,
	                          const Abc::Argument &iArg0 = Abc::Argument(),
	                          const Abc::Argument &iArg1 = Abc::Argument())
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
	Abc::TimeSamplingPtr getTimeSampling() const
	{
		if (m_positionsProperty.valid())
		{
			return m_positionsProperty.getTimeSampling();
		}
		return getObject().getArchive().getTimeSampling(0);
	}

	//-*************************************************************************
	void get(Sample &oSample,
	         const Abc::ISampleSelector &iSS = Abc::ISampleSelector()) const
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

	Sample getValue(const Abc::ISampleSelector &iSS = Abc::ISampleSelector()) const
	{
		Sample smp;
		get(smp, iSS);
		return smp;
	}

	Abc::IP3fArrayProperty getPositionsProperty() const { return m_positionsProperty; }

	Abc::IV3fArrayProperty getVelocitiesProperty() const { return m_velocitiesProperty; }

	Abc::IUInt64ArrayProperty getIdsProperty() const { return m_idsProperty; }

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
	void init(const Abc::Argument &iArg0,
	          const Abc::Argument &iArg1);

	Abc::IP3fArrayProperty m_positionsProperty;
	Abc::IUInt64ArrayProperty m_idsProperty;
	Abc::IV3fArrayProperty m_velocitiesProperty;
	IFloatGeomParam m_widthsParam;
};

//-*****************************************************************************
typedef Abc::ISchemaObject<IParticlesSchema> IParticles;

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
		Sample(const Abc::P3fArraySample &iPos,
		       const Abc::V3fArraySample &iVelocities = Abc::V3fArraySample(),
		       const OFloatGeomParam::Sample &iWidths = OFloatGeomParam::Sample())
		    : m_positions( iPos )
		    , m_velocities( iVelocities )
		    , m_widths( iWidths )
		{}

		//! Creates a sample with position data and id data. The first
		//! sample must be full like this. Subsequent samples may also
		//! be full like this, which would indicate a change of topology
		Sample(const Abc::P3fArraySample &iPos,
		       const Abc::UInt64ArraySample &iId,
		       const Abc::V3fArraySample &iVelocities = Abc::V3fArraySample(),
		       const OFloatGeomParam::Sample &iWidths =  OFloatGeomParam::Sample())
		    : m_positions(iPos)
		    , m_velocities(iVelocities)
		    , m_ids(iId)
		    , m_widths(iWidths)
		{}

		// positions accessor
		const Abc::P3fArraySample &getPositions() const { return m_positions; }
		void setPositions(const Abc::P3fArraySample &iSmp) { m_positions = iSmp; }

		// ids accessor
		const Abc::UInt64ArraySample &getIds() const { return m_ids; }
		void setIds(const Abc::UInt64ArraySample &iSmp) { m_ids = iSmp; }

		// velocities accessor
		const Abc::V3fArraySample &getVelocities() const { return m_velocities; }
		void setVelocities(const Abc::V3fArraySample &iVelocities) { m_velocities = iVelocities; }

		// widths accessor
		const OFloatGeomParam::Sample &getWidths() const { return m_widths; }
		void setWidths(const OFloatGeomParam::Sample &iWidths) { m_widths = iWidths; }

		const Abc::Box3d &getSelfBounds() const { return m_selfBounds; }
		void setSelfBounds(const Abc::Box3d &iBnds) { m_selfBounds = iBnds; }

		void reset()
		{
			m_positions.reset();
			m_velocities.reset();
			m_ids.reset();
			m_widths.reset();

			m_selfBounds.makeEmpty();
		}

	protected:
		Abc::P3fArraySample m_positions;
		Abc::V3fArraySample m_velocities;
		Abc::UInt64ArraySample m_ids;
		OFloatGeomParam::Sample m_widths;

		Abc::Box3d m_selfBounds;
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

	                 const Abc::Argument &iArg0 = Abc::Argument(),
	                 const Abc::Argument &iArg1 = Abc::Argument(),
	                 const Abc::Argument &iArg2 = Abc::Argument())
	    : OGeomBaseSchema<ParticlesSchemaInfo>(
	          GetCompoundPropertyWriterPtr(iParent),
	          iName, iArg0, iArg1, iArg2)
	{
		AbcA::TimeSamplingPtr tsPtr = Abc::GetTimeSampling( iArg0, iArg1, iArg2 );
		uint32_t tsIndex = Abc::GetTimeSamplingIndex( iArg0, iArg1, iArg2 );

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
	                          const Abc::Argument &iArg0 = Abc::Argument(),
	                          const Abc::Argument &iArg1 = Abc::Argument(),
	                          const Abc::Argument &iArg2 = Abc::Argument())
	    : OGeomBaseSchema<ParticlesSchemaInfo>(
	          GetCompoundPropertyWriterPtr(iParent),
	          iArg0, iArg1, iArg2)
	{
		AbcA::TimeSamplingPtr tsPtr = Abc::GetTimeSampling( iArg0, iArg1, iArg2 );
		uint32_t tsIndex = Abc::GetTimeSamplingIndex( iArg0, iArg1, iArg2 );

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
	AbcA::TimeSamplingPtr getTimeSampling() const { return m_positionsProperty.getTimeSampling(); }

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
	void setTimeSampling(AbcA::TimeSamplingPtr iTime);

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

	Abc::OP3fArrayProperty m_positionsProperty;
	Abc::OUInt64ArrayProperty m_idsProperty;
	Abc::OV3fArrayProperty m_velocitiesProperty;
	OFloatGeomParam m_widthsParam;

};

//-*****************************************************************************
// SCHEMA OBJECT
//-*****************************************************************************
typedef Abc::OSchemaObject<OParticlesSchema> OPoints;

typedef Util::shared_ptr< OPoints > OPointsPtr;

//} /* namespace PTC */

#endif  /* PTC_PARTICLES_H */
