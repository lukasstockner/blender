/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Brecht van Lommel
 * Contributor(s): Lukas Toenne
 */

#ifndef __DEPSGRAPH_UTIL_STRING_H__
#define __DEPSGRAPH_UTIL_STRING_H__

extern "C" {
#include "BLI_math.h"
}

struct Transform3 {
	typedef float (*data_ptr)[3];
	typedef const float (*const_data_ptr)[3];
	typedef float (&data_ref)[3][3];
	typedef const float (&const_data_ref)[3][3];
	
	static const Transform3 identity;
	
	Transform3()
	{
		zero_m3(data);
	}

	Transform3(const_data_ref copy)
	{
		copy_m3_m3(data, (data_ptr)copy);
	}
	
	Transform3 &operator = (const_data_ref copy)
	{
		copy_m3_m3(data, (data_ptr)copy);
		return *this;
	}
	
	operator data_ref() { return data; }
	operator const_data_ref() const { return data; }
	
	float data[3][3];
};

struct Transform4 {
	typedef float (*data_ptr)[4];
	typedef const float (*const_data_ptr)[4];
	typedef float (&data_ref)[4][4];
	typedef const float (&const_data_ref)[4][4];
	
	static const Transform4 identity;
	
	Transform4()
	{
		zero_m4(data);
	}

	Transform4(const_data_ref copy)
	{
		copy_m4_m4(data, (data_ptr)copy);
	}
	
	Transform4 &operator = (const_data_ref copy)
	{
		copy_m4_m4(data, (data_ptr)copy);
		return *this;
	}
	
	operator data_ref() { return data; }
	operator const_data_ref() const { return data; }
	
	float data[4][4];
};

#endif /* __DEPSGRAPH_UTIL_STRING_H__ */
