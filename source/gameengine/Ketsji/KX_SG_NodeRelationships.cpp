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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_SG_NodeRelationships.cpp
 *  \ingroup ketsji
 */


#include "KX_SG_NodeRelationships.h"


/** 
 * KX_NormalParentRelation - a regular parent/child relation, the child's coordinates are relative to the parent
 */

KX_NormalParentRelation* KX_NormalParentRelation::New()
{
	return new KX_NormalParentRelation();
}

bool KX_NormalParentRelation::UpdateChildCoordinates(SG_Spatial *child, const SG_Spatial *parent, bool &parentUpdated)
{
	MT_assert(child != NULL);

	/* If nothing changed in the parent or child, there is nothing to do here */
	if (!parentUpdated && !child->IsModified())
		return false;

	/* The child has no parent, it is a root object.
	 * The world and local coordinates should be the same and applied directly. */
	if (parent==NULL) {
		child->SetWorldFromLocalTransform();
	}
	/* The child has a parent. The child's coordinates are defined relative to the parent's.
	 * The parent's coordinates should be applied to the child's local ones to calculate the real world position. */
	else {
		const MT_Vector3 & parent_world_scale = parent->GetWorldScaling();
		const MT_Point3 & parent_world_pos = parent->GetWorldPosition();
		const MT_Matrix3x3 & parent_world_ori = parent->GetWorldOrientation();
		const MT_Vector3 & child_local_scale = child->GetLocalScale();
		const MT_Point3 & child_local_pos = child->GetLocalPosition();
		const MT_Matrix3x3 & child_local_ori = child->GetLocalOrientation();

		const MT_Vector3 & new_world_scale = parent_world_scale * child_local_scale;
		const MT_Matrix3x3 & new_world_ori = parent_world_ori * child_local_ori;
		const MT_Point3 & new_world_pos = parent_world_pos + (new_world_scale * (new_world_ori * child_local_pos));

		child->SetWorldScale(new_world_scale);
		child->SetWorldOrientation(new_world_ori);
		child->SetWorldPosition(new_world_pos);
	}

	parentUpdated = true;  //this variable is going to be used to update the children of this child
	child->ClearModified();
	return true;
}

SG_ParentRelation* KX_NormalParentRelation::NewCopy()
{
	return new KX_NormalParentRelation();
}

KX_NormalParentRelation::~KX_NormalParentRelation()
{
	//nothing to do
}

KX_NormalParentRelation::KX_NormalParentRelation()
{
	// nothing to do
}





/**
 * KX_VertexParentRelation - the child only inherits the position, not the orientation or scale
 */

KX_VertexParentRelation* KX_VertexParentRelation::New()
{
	return new KX_VertexParentRelation();
}

bool KX_VertexParentRelation::UpdateChildCoordinates(SG_Spatial *child, const SG_Spatial *parent, bool &parentUpdated)
{
	MT_assert(child != NULL);

	/* If nothing changed in the parent or child, there is nothing to do here */
	if (!parentUpdated && !child->IsModified())
		return false;

	/* The parent (if existing) is a vertex, so only position should be applied
	 * to the child's local coordinates to calculate the real world position. */

	if (parent==NULL)
		child->SetWorldPosition(child->GetLocalPosition());
	else
		child->SetWorldPosition(child->GetLocalPosition()+parent->GetWorldPosition());

	child->SetWorldScale(child->GetLocalScale());
	child->SetWorldOrientation(child->GetLocalOrientation());

	parentUpdated = true;  //this variable is going to be used to update the children of this child
	child->ClearModified();
	return true;
}

SG_ParentRelation* KX_VertexParentRelation::NewCopy()
{
	return new KX_VertexParentRelation();
}

KX_VertexParentRelation::~KX_VertexParentRelation()
{
	//nothing to do
}

KX_VertexParentRelation::KX_VertexParentRelation()
{
	//nothing to do
}





/**
 * KX_SlowParentRelation - the child only inherits the position, not the orientation or scale
 */

KX_SlowParentRelation* KX_SlowParentRelation::New(MT_Scalar relaxation)
{
	return new KX_SlowParentRelation(relaxation);
}

bool KX_SlowParentRelation::UpdateChildCoordinates(SG_Spatial *child, const SG_Spatial *parent, bool &parentUpdated)
{
	MT_assert(child != NULL);

	/* The child has no parent, it is a root object.
	 * The world and local coordinates should be the same and applied directly. */
	if (parent==NULL) {
		child->SetWorldFromLocalTransform();
	}
	/* The child's coordinates get linearly interpolated with the parent's */
	else {
		const MT_Vector3 & parent_world_scale = parent->GetWorldScaling();
		const MT_Point3 & parent_world_pos = parent->GetWorldPosition();
		const MT_Matrix3x3 & parent_world_ori = parent->GetWorldOrientation();
		const MT_Vector3 & child_local_scale = child->GetLocalScale();
		const MT_Point3 & child_local_pos = child->GetLocalPosition();
		const MT_Matrix3x3 & child_local_ori = child->GetLocalOrientation();

		/* Compute the normal world coordinates, where the child would be if it was a normal parent relation */
		const MT_Vector3 & normal_world_scale = parent_world_scale * child_local_scale;
		const MT_Matrix3x3 & normal_world_ori = parent_world_ori * child_local_ori;
		const MT_Point3 & normal_world_pos = parent_world_pos + (normal_world_scale * (normal_world_ori * child_local_pos));

		MT_Vector3 new_world_scale;
		MT_Point3 new_world_pos;
		MT_Matrix3x3 new_world_ori;

		if (m_initialized) {

			/* Get the current world positions */
			const MT_Vector3 & current_world_scale = child->GetWorldScaling();
			const MT_Matrix3x3 & current_world_ori = child->GetWorldOrientation();
			const MT_Point3 & current_world_pos = child->GetWorldPosition();

			/* Interpolate between the current world coordinates and the normal ones according to the weight.
			 * a bigger relax parameter, is a smaller weight,
			 * meaning that the child follows its normal position in smaller steps*/
			/* XXX - this design has problems as it does not consider elapsed time between last update */
			new_world_ori.setRotation(current_world_ori.getRotation().slerp(normal_world_ori.getRotation(), m_weight));
			new_world_pos = current_world_pos + ( (normal_world_pos - current_world_pos) * m_weight);
			new_world_scale = current_world_scale + ( (normal_world_scale - current_world_scale) * m_weight);

			//FIXME: update physics controller.

		} else {
			/**
			 * We need to compute valid world coordinates the first
			 * time we update spatial data of the child. This is done
			 * by just doing a normal parent relation the first time.
			 */
			new_world_scale = normal_world_scale;
			new_world_ori = normal_world_ori;
			new_world_pos = normal_world_pos;
			m_initialized = true;
		}

		child->SetWorldScale(new_world_scale);
		child->SetWorldOrientation(new_world_ori);
		child->SetWorldPosition(new_world_pos);
	}

	parentUpdated = true;  //this variable is going to be used to update the children of this child
	child->ClearModified();
	// this node must always be updated, so reschedule it for next time
	child->ActivateRecheduleUpdateCallback();
	return true;
}

SG_ParentRelation* KX_SlowParentRelation::NewCopy()
{
	return new 	KX_SlowParentRelation(m_relax);
}

KX_SlowParentRelation::KX_SlowParentRelation(MT_Scalar relaxation)
	:m_relax(relaxation),
	m_initialized(false)
{
	m_weight = MT_Scalar(1)/(m_relax + 1);
}

KX_SlowParentRelation::~KX_SlowParentRelation()
{
	//nothing to do
}
