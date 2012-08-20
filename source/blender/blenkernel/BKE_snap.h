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
 * Contributor(s): Luke Frisken 2012
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#ifndef __BKE_SNAP_H__
#define __BKE_SNAP_H__

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "BKE_context.h"
#include "WM_types.h"

typedef struct{
	//perhaps more info later I might guess
	float location[3];
	float normal[3];
	int r_dist;
	float r_depth;
}SnapPoint;

typedef struct SnapSystem SnapSystem;

typedef enum{
	SNAPSYSTEM_MODE_SELECT_ALL = 0,
	SNAPSYSTEM_MODE_SELECT_NOT_SELECTED = 1,
	SNAPSYSTEM_MODE_SELECT_NOT_OBEDIT = 2
}SnapSystem_mode_select;

typedef enum{
	SNAPSYSTEM_STATE_SNAPPING,
	SNAPSYSTEM_STATE_INIT_SNAP,
	SNAPSYSTEM_STATE_IDLE
}SnapSystem_state;

/*This function creates a new instance of the snapping system*/
/*callback_data: is the data to be used by the code using the snapping system (e.g transform)
  when the callbacks are called by snapsystem.

  update_callback: gets called when the snapping system has not yet recieved confirmation from the user
  of the chosen snap point, but there is a temporary snap_point available for providing realtime feedback
  to the user of their actions when moving the mouse. In the case of transform, this means the geometry
  moves towards the snap point without permenantly being applied.

  finish_callback: same as update_callback, but the snap_point is to be applied this time.

  cancel_callback: tells the code using the snapsystem that the current snapsystem snap search has been cancelled.

  nofeedback_callback: will probably be removed/changed/refactored soon.
  */
/*The return value pointer to SnapSystem, when finished requires the code using the system to call SnapSystem_free
  on it to free allocated data within the system*/
SnapSystem* SnapSystem_create( Scene* scene, View3D* v3d, ARegion* ar, Object* obedit, bContext* C,
								void* callback_data,
								void (*update_callback)(SnapSystem *ssystem, void* callback_data, SnapPoint sp),
							   void (*nofeedback_callback)(SnapSystem *ssystem, void* callback_data),
								void (*finish_callback)(SnapSystem *ssystem, void* callback_data, SnapPoint sp),
							   void (*cancel_callback)(SnapSystem *ssystem, void* callback_data));

void SnapSystem_test_run(SnapSystem* ssystem);
void SnapSystem_reset_ob_list(SnapSystem* ssystem); //reset the list of objects the snapsystem uses for snapping
void SnapSystem_evaluate_stack(SnapSystem* ssystem);
void SnapSystem_find_snap(SnapSystem* ssystem);
void SnapSystem_pick_snap(SnapSystem* ssystem);
void SnapSystem_add_ob(SnapSystem* ssystem, Object* ob);
Object* SnapSystem_get_ob(SnapSystem* ssystem, int index);
void SnapSystem_set_mode_select(SnapSystem* ssystem, SnapSystem_mode_select mode_select);
int SnapSystem_get_retval(SnapSystem* ssystem);
bContext* SnapSystem_get_C(SnapSystem* ssystem);
SnapSystem_state SnapSystem_get_state(SnapSystem* ssystem);

/*This function handles the events of the snapsystem, and returns a 1 if the event has been
  handled (and consumed) by the snapsystem*/
int SnapSystem_Event(SnapSystem* ssystem, wmEvent* event);
/*This function draws all the currently active snaps within the snapsystem*/
void SnapSystem_draw(SnapSystem* ssystem);

void SnapSystem_reset_snappoint(SnapSystem* ssystem);
void SnapSystem_clear_pick(SnapSystem* ssystem);
void SnapSystem_reset(SnapSystem* ssystem);
void SnapSystem_free(SnapSystem* ssystem);

/*default callbacks*/
void SnapSystem_default_object_iterator(SnapSystem* ssystem, void* callback_data);
void SnapSystem_default_object_handler(SnapSystem* ssystem, void* callback_data, Object* ob);

typedef struct Snap Snap;

typedef enum{
	SNAP_TYPE_MESH,
	SNAP_TYPE_CURVE,
	SNAP_TYPE_BONE,
	SNAP_TYPE_AXIS
}Snap_type;

typedef enum{
	SNAPMESH_DATA_TYPE_DerivedMesh,
	SNAPMESH_DATA_TYPE_BMEditMesh
}SnapMesh_data_type;

typedef enum{
	SNAPMESH_TYPE_VERTEX,
	SNAPMESH_TYPE_FACE,
	SNAPMESH_TYPE_PLANAR,
	SNAPMESH_TYPE_EDGE,
	SNAPMESH_TYPE_EDGE_MIDPOINT,
	SNAPMESH_TYPE_EDGE_PARALLEL
}SnapMesh_type;

typedef enum{
	SNAPMESH_DAT_vert,
	SNAPMESH_DAT_edge,
	SNAPMESH_DAT_face
}SnapMesh_data_array_type;

/*This struct provides a storage of face + verts in one place for using
  as return geometry in face picking, and gets used for planar snapping*/
typedef struct{
	MFace *face;
	MVert *verts;
	int nverts;
	float no[3];
}MeshData_pickface;

/*Return data type for SnapMesh class*/
typedef enum{
	SNAPMESH_RET_DAT_vert_index,
	SNAPMESH_RET_DAT_edge_index,
	SNAPMESH_RET_DAT_pface //see MeshData_pickface struct
}SnapMesh_ret_data_type;

/*Flags for use in VertCheck or EdgeCheck or FaceCheck MeshData functions.*/
typedef enum{
	MESHDATA_CHECK_NONE = 0,
	MESHDATA_CHECK_HIDDEN = 1,
	MESHDATA_CHECK_SELECTED = 2
}MeshData_check;

void Snap_run(Snap* s);
void Snap_free(Snap* s);

int Snap_getretval(Snap* s);
SnapPoint* Snap_getSnapPoint(Snap* s);
void Snap_setClosestPoint(Snap* s, SnapPoint* sp);
void Snap_setpick(Snap* s, Snap* pick);

Snap* SnapMesh_create(	void* mesh_data,
						SnapMesh_data_type data_type,
						int free_mesh_data,
						MeshData_check check,
						SnapMesh_type sm_type,
						Scene *scene, Object *ob, View3D *v3d, ARegion *ar, bContext *C, int mval[2]);

void SnapMesh_draw(Snap *sm);

#endif // __BKE_SNAP_H__
