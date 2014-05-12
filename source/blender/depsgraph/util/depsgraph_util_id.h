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

#include "depsgraph_util_type_traits.h"

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_ID.h"
}

/* Get the ID code based on associated DNA struct type */
template <typename IDType>
struct IDCode {
	operator int() const { return 0; } /* return 'invalid' code 0 by default */
};

#define DEF_ID_CODE_TYPE(code, type) \
	struct type; /* declare struct type */ \
	\
	template <> \
	struct IDCode<type> { \
		operator int() const { return code; } \
	};

/* XXX Copied from idcode.c, where this only exists in string form.
 * Should be unified to provide real compile-time info about associated struct types!
 */

DEF_ID_CODE_TYPE(ID_AC,     Action)
DEF_ID_CODE_TYPE(ID_AR,     Armature)
DEF_ID_CODE_TYPE(ID_BR,     Brush)
DEF_ID_CODE_TYPE(ID_CA,     Camera)
DEF_ID_CODE_TYPE(ID_CU,     Curve)
DEF_ID_CODE_TYPE(ID_GD,     GPencil)
DEF_ID_CODE_TYPE(ID_GR,     Group)
DEF_ID_CODE_TYPE(ID_ID,     ID)
DEF_ID_CODE_TYPE(ID_IM,     Image)
DEF_ID_CODE_TYPE(ID_IP,     Ipo)
DEF_ID_CODE_TYPE(ID_KE,     Key)
DEF_ID_CODE_TYPE(ID_LA,     Lamp)
DEF_ID_CODE_TYPE(ID_LI,     Library)
DEF_ID_CODE_TYPE(ID_LS,     FreestyleLineStyle)
DEF_ID_CODE_TYPE(ID_LT,     Lattice)
DEF_ID_CODE_TYPE(ID_MA,     Material)
DEF_ID_CODE_TYPE(ID_MB,     Metaball)
DEF_ID_CODE_TYPE(ID_MC,     MovieClip)
DEF_ID_CODE_TYPE(ID_ME,     Mesh)
DEF_ID_CODE_TYPE(ID_MSK,    Mask)
DEF_ID_CODE_TYPE(ID_NT,     NodeTree)
DEF_ID_CODE_TYPE(ID_OB,     Object)
DEF_ID_CODE_TYPE(ID_PA,     ParticleSettings)
DEF_ID_CODE_TYPE(ID_SCE,    Scene)
DEF_ID_CODE_TYPE(ID_SCR,    Screen)
DEF_ID_CODE_TYPE(ID_SEQ,    Sequence)
DEF_ID_CODE_TYPE(ID_SPK,    Speaker)
DEF_ID_CODE_TYPE(ID_SO,     Sound)
DEF_ID_CODE_TYPE(ID_TE,     Texture)
DEF_ID_CODE_TYPE(ID_TXT,    Text)
DEF_ID_CODE_TYPE(ID_VF,     VFont)
DEF_ID_CODE_TYPE(ID_WO,     World)
DEF_ID_CODE_TYPE(ID_WM,     WindowManager)

#undef DEF_ID_CODE_TYPE

template <typename IDPtrType>
IDPtrType static_cast_id(ID *id)
{
	typedef typename remove_pointer<IDPtrType>::type IDType;
	
	if (id)
		BLI_assert(IDCode<IDType>() == GS(id->name));
	return (IDType *)id;
}

template <typename IDPtrType>
IDPtrType static_cast_id(const ID *id)
{
	typedef typename remove_pointer<IDPtrType>::type IDType;
	
	if (id)
		BLI_assert(IDCode<IDType>() == GS(id->name));
	return (const IDType *)id;
}

template <typename IDPtrType>
IDPtrType *dynamic_cast_id(ID *id)
{
	typedef typename remove_pointer<IDPtrType>::type IDType;
	
	if (id && IDCode<IDType>() == GS(id->name))
		return (IDType *)id;
	else
		return NULL;
}

template <typename IDPtrType>
IDPtrType *dynamic_cast_id(const ID *id)
{
	typedef typename remove_pointer<IDPtrType>::type IDType;
	
	if (id && IDCode<IDType>() == GS(id->name))
		return (const IDType *)id;
	else
		return NULL;
}

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


BLI_INLINE bool id_is_tagged(ConstIDPtr id)
{
	return id->flag & LIB_DOIT;
}

BLI_INLINE void id_tag_set(IDPtr id)
{
	id->flag |= LIB_DOIT;
}

BLI_INLINE void id_tag_clear(IDPtr id)
{
	id->flag &= ~LIB_DOIT;
}

#endif /* __DEPSGRAPH_UTIL_ID_H__ */
