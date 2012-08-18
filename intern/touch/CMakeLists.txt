# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2012, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Nicholas Rishel
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../string
)

set(INC_SYS

)

set(SRC
	intern/TOUCH_API.cpp
	
	intern/TOUCH_Manager.cpp
	intern/TOUCH_Context.cpp
	intern/TOUCH_ContextBlender.cpp
	intern/TOUCH_ContextGameEngine.cpp

	TOUCH_API.h
	TOUCH_Types.h
	TOUCH_TypesBlender.h
	TOUCH_TypesGameEngine.h
	
	TOUCH_Manager.h
	TOUCH_Context.h
	TOUCH_ContextBlender.h
	TOUCH_ContextGameEngine.h
	
)

blender_add_lib(bf_intern_touch "${SRC}" "${INC}" "${INC_SYS}")
