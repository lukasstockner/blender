/*
 * Copyright 2015, Blender Foundation.
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

#ifndef PTC_ABC_INTERPOLATE_H
#define PTC_ABC_INTERPOLATE_H

#include <Alembic/Abc/ISampleSelector.h>
#include <Alembic/Abc/IScalarProperty.h>
#include <Alembic/Abc/TypedPropertyTraits.h>
#include <Alembic/Abc/ITypedArrayProperty.h>
#include <Alembic/Abc/ITypedScalarProperty.h>
#include <Alembic/Abc/TypedArraySample.h>

extern "C" {
#include "BLI_math.h"
#include "BLI_utildefines.h"
}

namespace PTC {

using namespace Alembic;
using namespace Abc;

using Alembic::Util::shared_ptr;

template <typename PropT>
BLI_INLINE typename PropT::value_type interpolate_sample(const typename PropT::value_type &val0, const typename PropT::value_type &val1, float t);

BLI_INLINE float interpolate_sample(const float &val0, const float &val1, float t)
{
	return val0 * (1.0f-t) + val1 * t;
}

BLI_INLINE V3f interpolate_sample(const V3f &val0, const V3f &val1, float t)
{
	return val0 * (1.0f-t) + val1 * t;
}

BLI_INLINE Quatf interpolate_sample(const Quatf &val0, const Quatf &val1, float t)
{
	float qt0[4] = {val0.r, val0.v.x, val0.v.y, val0.v.z};
	float qt1[4] = {val1.r, val1.v.x, val1.v.y, val1.v.z};
	float result[4];
	interp_qt_qtqt(result, qt0, qt1, t);
	return Quatf(result[0], result[1], result[2], result[3]);
}

BLI_INLINE M44f interpolate_sample(const M44f &val0, const M44f &val1, float t)
{
	float loc[3], quat[4], size[3];
	float loc0[3], quat0[4], size0[3];
	float loc1[3], quat1[4], size1[3];
	mat4_decompose(loc0, quat0, size0, (float (*)[4])val0.getValue());
	mat4_decompose(loc1, quat1, size1, (float (*)[4])val1.getValue());
	
	/* linear interpolation for rotation and scale */
	interp_v3_v3v3(loc, loc0, loc1, t);
	
	/* use simpe nlerp instead of slerp. it's faster and almost the same */
	interp_v4_v4v4(quat, quat0, quat1, t);
	normalize_qt(quat);
	
	interp_v3_v3v3(size, size0, size1, t);
	
	M44f result;
	loc_quat_size_to_mat4((float (*)[4])result.getValue(), loc, quat, size);
	
	return result;
}

template <typename TraitsT>
BLI_INLINE shared_ptr< TypedArraySample<TraitsT> > interpolate_sample(const TypedArraySample<TraitsT> &val0, const TypedArraySample<TraitsT> &val1, float t)
{
	typedef TypedArraySample<TraitsT> sample_type;
	typedef shared_ptr<sample_type> sample_ptr_type;
	typedef typename sample_type::value_type value_type;
	
	size_t size0 = val0.size();
	size_t size1 = val1.size();
	size_t maxsize = size0 > size1 ? size0 : size1;
	size_t minsize = size0 < size1 ? size0 : size1;
	
	const value_type *data0 = val0.get();
	const value_type *data1 = val1.get();
	value_type *result = new value_type[maxsize];
	value_type *data = result;
	
	for (size_t i = 0; i < minsize; ++i) {
		*data = interpolate_sample(*data0, *data1, t);
		++data;
		++data0;
		++data1;
	}
	
	if (size0 > minsize) {
		for (size_t i = minsize; i < size0; ++i) {
			*data = *data0;
			++data;
			++data0;
		}
	}
	else if (size1 > minsize) {
		for (size_t i = minsize; i < size1; ++i) {
			*data = *data1;
			++data;
			++data1;
		}
	}
	
	return sample_ptr_type(new sample_type(result, maxsize));
}

/* ------------------------------------------------------------------------- */

template <typename PropT>
typename PropT::value_type abc_interpolate_sample_linear(const PropT &prop, chrono_t time)
{
	ISampleSelector ss0(time, ISampleSelector::kFloorIndex);
	ISampleSelector ss1(time, ISampleSelector::kCeilIndex);
	
	index_t index0 = ss0.getIndex(prop.getTimeSampling(), prop.getNumSamples());
	index_t index1 = ss1.getIndex(prop.getTimeSampling(), prop.getNumSamples());
	if (index0 == index1) {
		/* no interpolation needed */
		return prop.getValue(ss0);
	}
	else {
		chrono_t time0 = prop.getTimeSampling()->getSampleTime(index0);
		chrono_t time1 = prop.getTimeSampling()->getSampleTime(index1);
		
		float t = (time1 > time0) ? (time - time0) / (time1 - time0) : 0.0f;
		return interpolate_sample(prop.getValue(ss0), prop.getValue(ss1), t);
	}
}

template <typename TraitsT>
shared_ptr<typename ITypedArrayProperty<TraitsT>::sample_type> abc_interpolate_sample_linear(const ITypedArrayProperty<TraitsT> &prop, chrono_t time)
{
	ISampleSelector ss0(time, ISampleSelector::kFloorIndex);
	ISampleSelector ss1(time, ISampleSelector::kCeilIndex);
	
	index_t index0 = ss0.getIndex(prop.getTimeSampling(), prop.getNumSamples());
	index_t index1 = ss1.getIndex(prop.getTimeSampling(), prop.getNumSamples());
	if (index0 == index1) {
		/* no interpolation needed */
		return prop.getValue(ss0);
	}
	else {
		chrono_t time0 = prop.getTimeSampling()->getSampleTime(index0);
		chrono_t time1 = prop.getTimeSampling()->getSampleTime(index1);
		
		float t = (time1 > time0) ? (time - time0) / (time1 - time0) : 0.0f;
		return interpolate_sample(*prop.getValue(ss0), *prop.getValue(ss1), t);
	}
}

} /* namespace PTC */

#endif  /* PTC_ABC_INTERPOLATE_H */
