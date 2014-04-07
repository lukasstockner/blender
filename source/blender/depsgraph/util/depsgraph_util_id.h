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
 * Original Author: Lukas Toenne
 * Contributor(s): None yet
 */

#ifndef __DEPSGRAPH_UTIL_ID_H__
#define __DEPSGRAPH_UTIL_ID_H__

/* Helper types for handling ID subtypes in C
 * 
 * These can be casted implicitly from/to ID*
 * without the need to access nested members (like &scene->id)
 */

struct IDPtr {
	IDPtr(ID *id) : m_ptr(id) {}
	template <typename IDType>
	IDPtr(IDType *id) : m_ptr(&id->id) {}
	
	const IDPtr &operator=(ID *id) { m_ptr = id; return *this; }
	template <typename IDType>
	const IDPtr &operator=(IDType *id) { m_ptr = &id->id; return *this; }
	
	operator ID *() const { return m_ptr; }
	ID &operator *() const { return *m_ptr; }
	ID *operator ->() const { return m_ptr; }
	
private:
	ID *m_ptr;
};

struct ConstIDPtr {
	ConstIDPtr(const ID *id) : m_ptr(id) {}
	template <typename IDType>
	ConstIDPtr(const IDType *id) : m_ptr(&id->id) {}
	
	const ConstIDPtr &operator=(const ID *id) { m_ptr = id; return *this; }
	template <typename IDType>
	const ConstIDPtr &operator=(const IDType *id) { m_ptr = &id->id; return *this; }
	
	operator const ID *() const { return m_ptr; }
	const ID &operator *() const { return *m_ptr; }
	const ID *operator ->() const { return m_ptr; }
	
private:
	const ID *m_ptr;
};

#endif /* __DEPSGRAPH_UTIL_ID_H__ */
